#pragma once
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <mmreg.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <memory>
#include <system_error>
#include <thread>

class AudioBeat {
public:
    ~AudioBeat() { stop(); }

    bool start() {
        stopEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!stopEvent_) return false;
        try {
            worker_ = std::thread([this] { listen(); });
        } catch (const std::system_error&) {
            CloseHandle(stopEvent_);
            stopEvent_ = nullptr;
            return false;
        }
        return true;
    }

    void stop() {
        if (!stopEvent_) return;
        SetEvent(stopEvent_);
        worker_.join();
        CloseHandle(stopEvent_);
        stopEvent_ = nullptr;
        pulse_ = 0.0f;
        audible_ = false;
        failed_ = false;
    }

    float pulse() const { return pulse_.load(); }
    bool audible() const { return audible_.load(); }
    bool failed() const { return failed_.load(); }

private:
    struct ComRelease {
        void operator()(IUnknown* object) const { object->Release(); }
    };
    template <class T> using ComPtr = std::unique_ptr<T, ComRelease>;
    struct ComFree {
        void operator()(void* memory) const { CoTaskMemFree(memory); }
    };
    struct EventClose {
        void operator()(void* event) const { CloseHandle(event); }
    };

    enum class Encoding { Unsupported, Float, Pcm16, Pcm24, Pcm32 };

    static Encoding encoding(const WAVEFORMATEX& format) {
        WORD tag = format.wFormatTag;
        if (tag == WAVE_FORMAT_EXTENSIBLE && format.cbSize >= 22) {
            const auto& ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE&>(format);
            const GUID subtype{ext.SubFormat.Data1, 0, 0x0010, {0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71}};
            if (!IsEqualGUID(subtype, ext.SubFormat)) return Encoding::Unsupported;
            tag = static_cast<WORD>(ext.SubFormat.Data1);
        }
        if (tag == WAVE_FORMAT_IEEE_FLOAT && format.wBitsPerSample == 32) return Encoding::Float;
        if (tag != WAVE_FORMAT_PCM) return Encoding::Unsupported;
        switch (format.wBitsPerSample) {
            case 16: return Encoding::Pcm16;
            case 24: return Encoding::Pcm24;
            case 32: return Encoding::Pcm32;
            default: return Encoding::Unsupported;
        }
    }

    static float readSample(const BYTE* data, Encoding type) {
        switch (type) {
            case Encoding::Float: {
                float value;
                std::memcpy(&value, data, sizeof(value));
                return value;
            }
            case Encoding::Pcm16: {
                std::int16_t value;
                std::memcpy(&value, data, sizeof(value));
                return value / 32768.0f;
            }
            case Encoding::Pcm24: {
                int value = data[0] + 256 * data[1] + 65536 * data[2];
                if (value >= 8388608) value -= 16777216;
                return value / 8388608.0f;
            }
            case Encoding::Pcm32: {
                std::int32_t value;
                std::memcpy(&value, data, sizeof(value));
                return value / 2147483648.0f;
            }
            default: return 0.0f;
        }
    }

    struct Envelope {
        float low[2]{};
        float baseline = 0.0f;
        float previous = 0.0f;
        float pulse = 0.0f;
        float cooldown = 0.0f;
        float quietTime = 1.0f;

        float step(const BYTE* data, UINT32 frames, const WAVEFORMATEX& format, Encoding type, bool silent) {
            const DWORD stride = std::max(1ul, format.nSamplesPerSec / 12000ul);
            int channels = std::min(2, static_cast<int>(format.nChannels));
            float filter = 1.0f - std::exp(-880.0f * stride / format.nSamplesPerSec);
            float squares = 0.0f, broad = 0.0f;
            unsigned samples = 0;
            for (UINT32 i = 0; i < frames; i += stride) {
                for (int channel = 0; channel < channels; ++channel) {
                    float value = silent ? 0.0f : readSample(data + i * format.nBlockAlign +
                        channel * (format.wBitsPerSample / 8), type);
                    low[channel] += filter * (value - low[channel]);
                    squares += low[channel] * low[channel];
                    broad += value * value;
                    ++samples;
                }
            }
            float energy = std::sqrt((squares + broad * 0.08f) / samples);
            float dt = (float)frames / format.nSamplesPerSec;
            quietTime = energy >= 0.0025f ? 0.0f : quietTime + dt;
            float floor = std::max(baseline, 0.0020f);
            cooldown -= dt;
            pulse *= std::exp(-dt * 10.0f);
            if (cooldown <= 0 && energy > floor * 1.55f && energy > previous * 1.13f) {
                pulse = std::clamp(0.30f + (energy - floor) * 12.0f, 0.0f, 1.0f);
                cooldown = 0.17f;
            }
            baseline += (energy - baseline) * (1.0f - std::exp(-dt * 3.0f));
            previous = energy;
            return pulse;
        }
    };

    bool changedEndpoint(IMMDeviceEnumerator& enumerator, const wchar_t* previousId) {
        IMMDevice* next = nullptr;
        if (FAILED(enumerator.GetDefaultAudioEndpoint(eRender, eMultimedia, &next))) return false;
        ComPtr<IMMDevice> device(next);
        wchar_t* nextId = nullptr;
        if (FAILED(device->GetId(&nextId))) return false;
        std::unique_ptr<wchar_t, ComFree> id(nextId);
        return std::wcscmp(id.get(), previousId) != 0;
    }

    bool capture(IMMDeviceEnumerator& enumerator) {
        IMMDevice* rawDevice = nullptr;
        if (FAILED(enumerator.GetDefaultAudioEndpoint(eRender, eMultimedia, &rawDevice))) return false;
        ComPtr<IMMDevice> device(rawDevice);
        wchar_t* rawId = nullptr;
        if (FAILED(device->GetId(&rawId))) return false;
        std::unique_ptr<wchar_t, ComFree> id(rawId);
        std::unique_ptr<void, EventClose> samples(CreateEventW(nullptr, FALSE, FALSE, nullptr));
        if (!samples) return false;
        IAudioClient* rawClient = nullptr;
        if (FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&rawClient)))) return false;
        ComPtr<IAudioClient> stream(rawClient);
        WAVEFORMATEX* rawFormat = nullptr;
        if (FAILED(stream->GetMixFormat(&rawFormat))) return false;
        std::unique_ptr<WAVEFORMATEX, ComFree> format(rawFormat);
        const Encoding type = encoding(*format);
        if (type == Encoding::Unsupported || format->nChannels == 0 || format->nSamplesPerSec == 0) return false;

        constexpr DWORD flags = AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
        if (FAILED(stream->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, 0, 0, format.get(), nullptr)) ||
            FAILED(stream->SetEventHandle(samples.get()))) return false;
        IAudioCaptureClient* rawReader = nullptr;
        if (FAILED(stream->GetService(IID_PPV_ARGS(&rawReader)))) return false;
        ComPtr<IAudioCaptureClient> reader(rawReader);
        if (FAILED(stream->Start())) return false;

        Envelope detector;
        HANDLE events[]{stopEvent_, samples.get()};
        auto endpointCheck = std::chrono::steady_clock::now() + std::chrono::seconds(1);
        bool reopen = false;
        HRESULT status = S_OK;
        for (;;) {
            DWORD ready = WaitForMultipleObjects(2, events, FALSE, 180);
            if (ready == WAIT_OBJECT_0 || ready == WAIT_FAILED) break;
            if (ready == WAIT_TIMEOUT) {
                detector = Envelope{};
                pulse_ = 0.0f;
                audible_ = false;
            } else {
                UINT32 frames;
                while (SUCCEEDED(status = reader->GetNextPacketSize(&frames)) && frames > 0) {
                    BYTE* data;
                    DWORD packetFlags;
                    status = reader->GetBuffer(&data, &frames, &packetFlags, nullptr, nullptr);
                    if (FAILED(status) || frames == 0) break;
                    pulse_ = detector.step(data, frames, *format, type, (packetFlags & AUDCLNT_BUFFERFLAGS_SILENT) != 0);
                    audible_ = detector.quietTime < 0.28f;
                    status = reader->ReleaseBuffer(frames);
                    if (FAILED(status)) break;
                }
                if (FAILED(status)) break;
            }
            if (std::chrono::steady_clock::now() >= endpointCheck) {
                reopen = changedEndpoint(enumerator, id.get());
                if (reopen) break;
                endpointCheck = std::chrono::steady_clock::now() + std::chrono::seconds(1);
            }
        }
        stream->Stop();
        pulse_ = 0.0f;
        audible_ = false;
        return reopen || (status == AUDCLNT_E_DEVICE_INVALIDATED && changedEndpoint(enumerator, id.get()));
    }

    void listen() {
        if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {
            failed_ = true;
            return;
        }
        {
            IMMDeviceEnumerator* rawEnumerator = nullptr;
            if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&rawEnumerator)))) {
                ComPtr<IMMDeviceEnumerator> enumerator(rawEnumerator);
                while (WaitForSingleObject(stopEvent_, 0) == WAIT_TIMEOUT && capture(*enumerator)) {}
            }
        }
        if (WaitForSingleObject(stopEvent_, 0) == WAIT_TIMEOUT) failed_ = true;
        CoUninitialize();
    }

    HANDLE stopEvent_ = nullptr;
    std::thread worker_;
    std::atomic<float> pulse_{0.0f};
    std::atomic<bool> audible_{false};
    std::atomic<bool> failed_{false};
};
