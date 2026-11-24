import AppKit
import AVFoundation
import CoreGraphics
import QuartzCore
import ScreenCaptureKit

final class SystemAudio: NSObject, SCStreamOutput, SCStreamDelegate {
    private let queue = DispatchQueue(label: "KuroCat.system-audio", qos: .utility)
    private let lock = NSLock()
    private var stream: SCStream?
    private var active = false
    private var generation = 0
    private var activeStream: ObjectIdentifier?
    private var estimate = Estimate()
    var onFailure: (() -> Void)?

    private struct Estimate {
        var low: Float = 0
        var floor: Float = 0
        var previous: Float = 0
        var pulse: Float = 0
        var lastBeat: CFTimeInterval = 0
        var lastSound: CFTimeInterval = 0
        var lastPacket: CFTimeInterval = 0

        mutating func add(_ samples: UnsafePointer<Float>, frames: Int, stride: Int,
                          sampleRate: Double, now: CFTimeInterval) {
            let rate = Float(sampleRate)
            let smoothing = 1 - exp(-880 / rate)
            var sum: Float = 0
            for index in 0..<frames {
                let sample = samples[index * stride]
                low += smoothing * (sample - low)
                sum += low * low + sample * sample * 0.08
            }
            let energy = sqrt(sum / Float(frames))
            let elapsed = Float(frames) / rate
            if energy >= 0.0025 { lastSound = now }
            pulse *= exp(-10 * elapsed)
            if energy > max(0.002, floor) * 1.55 && energy > previous * 1.13 && now - lastBeat >= 0.17 {
                pulse = min(1, 0.30 + (energy - floor) * 12)
                lastBeat = now
            }
            floor += (energy - floor) * (1 - exp(-3 * elapsed))
            previous = energy
            lastPacket = now
        }
    }

    func reading() -> (audible: Bool, pulse: CGFloat) {
        lock.lock()
        defer { lock.unlock() }
        let now = CACurrentMediaTime()
        return (now - estimate.lastSound < 0.28, now - estimate.lastPacket < 0.18 ? CGFloat(estimate.pulse) : 0)
    }

    func start(requestPermission: Bool) -> Bool {
        if active { return true }
        guard CGPreflightScreenCaptureAccess() ||
                (requestPermission && CGRequestScreenCaptureAccess()) else { return false }
        active = true
        generation += 1
        let token = generation
        Task { @MainActor in
            do {
                let content = try await SCShareableContent.excludingDesktopWindows(false, onScreenWindowsOnly: true)
                guard token == self.generation else { return }
                guard let display = content.displays.first else {
                    self.fail()
                    return
                }
                let configuration = SCStreamConfiguration()
                configuration.capturesAudio = true
                configuration.excludesCurrentProcessAudio = true
                configuration.sampleRate = 16_000
                configuration.channelCount = 1
                configuration.width = 2
                configuration.height = 2
                configuration.minimumFrameInterval = CMTime(value: 1, timescale: 1)
                let filter = SCContentFilter(display: display, excludingWindows: [])
                let live = SCStream(filter: filter, configuration: configuration, delegate: self)
                try live.addStreamOutput(self, type: .audio, sampleHandlerQueue: self.queue)
                self.stream = live
                self.lock.lock()
                self.activeStream = ObjectIdentifier(live)
                self.lock.unlock()
                try await live.startCapture()
                if token != self.generation { try? await live.stopCapture() }
            } catch {
                if token == self.generation { self.fail() }
            }
        }
        return true
    }

    func stop() {
        generation += 1
        active = false
        lock.lock()
        activeStream = nil
        estimate = Estimate()
        lock.unlock()
        if let old = stream {
            stream = nil
            Task { try? await old.stopCapture() }
        }
    }

    private func fail() {
        stop()
        onFailure?()
    }

    func stream(_ stream: SCStream, didStopWithError error: Error) {
        DispatchQueue.main.async { [weak self] in
            guard let self, self.stream === stream else { return }
            self.fail()
        }
    }

    func stream(_ stream: SCStream, didOutputSampleBuffer sampleBuffer: CMSampleBuffer, of type: SCStreamOutputType) {
        guard type == .audio, let description = CMSampleBufferGetFormatDescription(sampleBuffer),
              let format = AVAudioFormat(cmAudioFormatDescription: description) else { return }
        let count = CMSampleBufferGetNumSamples(sampleBuffer)
        guard count > 0, let pcm = AVAudioPCMBuffer(pcmFormat: format, frameCapacity: AVAudioFrameCount(count)) else { return }
        pcm.frameLength = AVAudioFrameCount(count)
        guard CMSampleBufferCopyPCMDataIntoAudioBufferList(sampleBuffer, at: 0, frameCount: Int32(count),
                                                          into: pcm.mutableAudioBufferList) == noErr,
              let channels = pcm.floatChannelData else { return }
        lock.lock()
        if activeStream == ObjectIdentifier(stream) {
            estimate.add(channels[0], frames: count, stride: pcm.stride,
                         sampleRate: format.sampleRate, now: CACurrentMediaTime())
        }
        lock.unlock()
    }
}
