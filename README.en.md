# 🐱 SachiCat — Full-State Keyboard & Mouse Desktop Cat Pet (macOS / Windows / Linux / UOS)

<p align="center">
  <a href="README.md">简体中文</a> | <b>English</b>
</p>

## Orca Cat (虎鲸猫) skin for Windows

This fork adds Orca Cat (虎鲸猫), a black-and-white tuxedo cat, to the Windows app. A fresh installation starts with Orca Cat. Right-click the cat and open the skin menu to switch between Orca Cat and the original skins.

Use the icons beneath the pet to give it a treat, put it to sleep, wake it up, switch desk modes, or toggle each kind of input bubble. Hover over an icon to see its name. After hiding the controls, open the cat or tray menu to show them again.

Click the headphones icon to give Orca Cat headphones. It closes its eyes and nods along while audio plays on the default Windows output device, and opens its eyes in silence. Click the headphones again or choose another enabled control to stop. Audio is processed locally in real time.

In Auto mode, Orca Cat picks up a stylus in supported art apps such as Blender, Krita, and Photoshop. It switches back to the keyboard and mouse when a text field has focus. Select **Check for Updates** in the tray menu to open the [SachiCat releases](https://github.com/catopia-meow/SachiCat/releases) in your default browser.

[Download the latest Windows x64 app](https://github.com/catopia-meow/SachiCat/releases/latest)

All platforms read [VERSION](VERSION) from the repository root. Prefix the release tag with `v`, for example `v2.0` or `v2.0.1`.

![Orca Cat using the keyboard and listening to audio with icon controls](assets/preview_orca_cat_controls.png)

<p align="center">
  <img src="assets/demo_typing.gif" width="265" alt="Typing on keyboard"/>
  <img src="assets/demo_hunting.gif" width="265" alt="Hunting boba eyes on fast mouse movement"/>
  <img src="assets/demo_combo.gif" width="265" alt="Typing + dragging mouse + 3rd key with hind paw"/>
</p>

<p align="center">
  <b>Ultra-lightweight native desktop pet · Hardware-level full-state capture for typing &amp; gaming · 2.5D spherical head tracking · Hunting boba eyes · 4 switchable skins</b><br/>
  Native macOS (Swift + CoreGraphics + IOHIDManager) · Windows (C++17 + GDI+ + Raw Input) · Linux / UOS (C++17 + GTK3/Cairo + XInput2/evdev)
</p>

---

## ✨ Highlights

- **🎮 Full-State Input Capture for Typing & Gaming (True NKRO)**
  - **macOS**: Dual-layer `IOHIDManager` (kernel HID hardware layer) + `CGEventTap`, unaffected by IME switching. Supports holding `WASD` in games, key combinations, standalone modifier keys (`⌘ / ⇧ / ⌥ / ⌃ / Fn`), arrow keys, and `F1–F12`.
  - **Windows**: Native `Raw Input API (RIDEV_INPUTSINK)` hardware scan-code listener. Captures global keystrokes and mouse micro-movements even inside DirectX/Vulkan games with locked cursor.
  - **Linux & UOS (统信 UOS)**: Dual-layer `XInput2 (XI_RawKeyPress/Motion)` + kernel `evdev (/dev/input/event*)`. **Works out of the box without `sudo` on UOS / Deepin / Ubuntu X11 desktops**; on Wayland, add your user to the `input` group for hardware-level full-state capture.
- **🌀 2.5D Spherical Head Tracking & Natural Perspective**
  - Uses a Live2D-style spherical projection algorithm: the cat's head, ears, eyes, and whiskers rotate with realistic 2.5D perspective and parallax toward your cursor. The keyboard and mouse are oriented 180° facing the cat, matching your left-hand keyboard / right-hand mouse gaming posture.
- **⚡ Hunting Boba Eyes Mode**
  - Shake your mouse quickly and the cat's pupils dilate from slit pupils into huge round boba eyes with bright highlights—crouching low, wagging its tail faster, and locking onto the cursor without blinking.
- **🐾 Fun Interactive Details**
  - **3-Key "Hind Paw Press"**: Hold 3 or more keys simultaneously and the cat lifts its hind paw to press the 3rd key!
  - **Head / Back Petting**: Gently stroke the cat's head with your cursor to trigger cozy closed eyes `— —` and shimmer particles; leave it idle and it falls asleep with floating `z`s.
  - **Smart Click-Through & Launch at Login**: Transparent areas pass clicks through automatically. Toggle **Full Click-Through (Gaming Anti-Misclick)** anytime or enable **Launch at Login** (Windows Registry / macOS LaunchAgent / Linux XDG Autostart).
- **🎨 4 Skins & Custom Color Combinations**
  - **Cat Skins**: Classic Black Cat (neon yellow-green eyes) / Siamese Cat (warm oat cream + 2.5D caramel mask + chocolate socks + sapphire blue eyes) / American Shorthair (silver tabby & white) / Orange & White Cat (orange crown + white chest & mittens)
  - **Keyboard Themes**: Classic Warm Cream / Midnight Geek Black / Retro Cream Gray (toggle key labels, key bubbles, and mouse click bubbles)
  - **Eye Colors & Scaling**: 6 eye colors, 5 size levels, plus daily keystroke counter and live KPM stats.

### 6-State Preview of All 4 Skins

| Classic Black Cat | Siamese Cat |
| :---: | :---: |
| <img src="assets/preview_black.jpg" width="420" alt="Classic Black Cat preview"/> | <img src="assets/preview_siamese.jpg" width="420" alt="Siamese Cat preview"/> |

| American Shorthair | Orange & White Cat |
| :---: | :---: |
| <img src="assets/preview_shorthair.jpg" width="420" alt="American Shorthair preview"/> | <img src="assets/preview_orangeWhite.jpg" width="420" alt="Orange & White Cat preview"/> |

The 6-state previews are natively rendered by the app: Idle, Top-Left Look, Bottom-Right Look, Hunting, Keyboard/Mouse + 3rd Paw, and Petting Closed Eyes.

---

## 📦 Download & Run

Download pre-built binaries from **[Releases](../../releases)** (built automatically via GitHub Actions):

### 🍎 macOS (Universal: Apple Silicon & Intel · macOS 13+)

> **This macOS update has not been tested on Mac hardware.**

1. Download [SachiCat-macOS-universal.zip](https://github.com/catopia-meow/SachiCat/releases/latest), unzip it, and drag `SachiCat.app` into **Applications** (lives in the top menu bar, no Dock icon).
2. On first launch, click the bottom prompt pill to open **System Settings → Privacy & Security → Input Monitoring**, enable `SachiCat`, then click **Restart SachiCat** from the menu bar to enable global typing & gaming capture.
3. Supports one-click **Launch at Login**. When **Full Click-Through** is enabled, use the menu bar icon or **hold `⌥ Option` and right-click / drag the cat**.

**Live capture**

- Right-click the cat and select **直播采集 → 透明窗口** (Live capture → Transparent window). In OBS, add a **macOS Screen Capture** source, select **Window Capture** as the method, and choose `SachiCat`. If the window is missing, enable **Show fullscreen and hidden windows / applications**.
- If the capture app does not preserve transparency, select **直播采集 → 绿幕窗口（色度键）** (Live capture → Green screen) and apply a green chroma key filter (`#00FF00`) to the source.

**Build from source:**
```bash
chmod +x build.sh
./build.sh
open SachiCat.app
```

### 🪟 Windows (Windows 10 / 11 x64)
1. Download the single-file `SachiCat-Windows-x64.exe` (~220 KB, **zero external runtime dependencies**). Double-click to run; lives in the system tray.
2. Right-click the cat or click the tray icon to switch skins, keyboard themes, scale, **Launch at Login**, or **Full Click-Through**.
3. **4 ways to exit or open the menu while Full Click-Through is active**:
   - Global shortcut: **`Ctrl + Alt + Shift + P`** to toggle click-through;
   - Temporary unlock: hold **`Alt`** and right-click the cat to uncheck or left-drag to move;
   - System tray: click the black cat icon in the **`^` hidden icons area** on the taskbar;
   - Fallback: **double-click `SachiCat.exe` again** to disable click-through and open the settings menu at your cursor.
   > **Cat follows the mouse but not the keyboard in a game?** The game (or its launcher) runs as Administrator, and Windows withholds its keystrokes from normal apps. Right-click the cat → **Restart as administrator**, or tick **Always run as administrator**. SachiCat shows a tray tip when it detects such a game.
   >
   > **Cat disappears in games?** SachiCat now re-asserts always-on-top when a fullscreen/borderless game comes to the front and moves back on-screen after resolution changes. True *exclusive fullscreen* cannot be overlaid by any desktop window — switch the game to borderless/windowed fullscreen.
   >
   > Launching a newer copy of SachiCat hands the desktop pet over to that copy.

**Live capture**

- Right-click the cat and select **直播采集 → 透明窗口（WGC）** (Live capture → Transparent window). In OBS, select `SachiCat` in a **Window Capture** source. Set the capture method to **Modern (WGC)** or **Windows 10 (1903 and up)**, depending on the OBS version.
- If the capture app does not preserve transparency, select **直播采集 → 绿幕窗口（色度键）** (Live capture → Green screen). Capture the `SachiCat` window and apply a green chroma key filter (`#00FF00`). This mode works with the **Automatic** capture method in OBS.

**Build from source (MSVC / MinGW):**
```bat
cd windows
build.bat
```
Or with CMake:
```bat
cd windows
cmake -B build && cmake --build build --config Release
```

### 🐧 Linux & UOS / Kylin (x86_64 & ARM64)
Supports **Ubuntu 20.04+ / Debian 11+ / Deepin / UOS (x86_64 & ARM64) / Kylin / Arch / Fedora**:

1. **No-password install (recommended on UOS / Kylin)**: extract `SachiCat-Linux-x86_64.tar.gz` or `SachiCat-Linux-arm64.tar.gz` and run `bash install.sh` in that folder. It installs into `~/.local` for the current user only (no sudo); `bash uninstall.sh` removes it.
2. **`.deb` package (UOS / Deepin / Ubuntu / Debian / Kylin)**:
   - Download `SachiCat-Linux-amd64.deb` or `SachiCat-Linux-arm64.deb`, double-click it or run:
     ```bash
     sudo apt install ./SachiCat-Linux-amd64.deb
     ```
   - Packages are built on Debian 10 (glibc 2.28), so older UOS 20 / Kylin V10 systems can run them.
   - Kylin refuses to install? Set *Security Center → App Protection / Execution Control* to Off or Warn, or use the no-password install.
3. **Other Linux distros (Arch / Fedora, etc.)**:
   - Extract the tar.gz and run `bash install.sh`, or simply run `./sachicat`.
   - Desktop lagging? SachiCat uses an adaptive 30/20/10 fps by default; the **60fps** option in the menu should stay off on low-end machines.
   - On pure **Wayland** sessions, run `sudo usermod -aG input $USER` once and re-login to enable kernel-level `evdev` capture.
4. **Build from source (including LoongArch)**:
   ```bash
   sudo apt install g++ pkg-config libgtk-3-dev libcairo2-dev libx11-dev libxi-dev
   cd linux && chmod +x build.sh build_deb.sh
   ./build.sh        # Compile binary
   ./build_deb.sh    # Package .deb installer
   ```

---

## 📁 Project Structure

```text
KuroCat/
├── Sources/                   # macOS native Swift source (Universal: Apple Silicon + Intel)
├── Resources/                 # macOS Info.plist & AppIcon.icns
├── build.sh                   # macOS universal binary build script
├── windows/                   # Windows native C++17 (Win32 + GDI+ + Raw Input)
├── linux/                     # Linux & UOS native C++17 (GTK3 + Cairo + XInput2 + evdev)
│   ├── src/                   # KeyMap / InputEngine / CatPhysics / CatRenderer / main.cpp
│   ├── build.sh               # Local build script
│   └── build_deb.sh           # Debian / UOS .deb packager
└── .github/workflows/build.yml # Automated multi-platform release workflow
```

---

## 🔒 Privacy

All keyboard and mouse hardware signals in SachiCat are processed **strictly in local memory in real time** to drive animations and counters—**no keystrokes are ever logged or saved**. SachiCat does not check for updates in the background. The entire codebase is open source and auditable.

## 📄 License

[MIT License](LICENSE)
