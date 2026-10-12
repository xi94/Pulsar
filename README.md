<p align="center">
  <img src="PULSAR/PULSAR_Icon_128x128.png" width="96" alt="Pulsar icon">
</p>
<h1 align="center">Pulsar</h1>
 
<p align="center">
  One-click account switching for every Riot game.<br>
  Encrypted, local and fast.
</p>
<p align="center">
  <a href="https://github.com/xi94/Pulsar/releases/latest"><img src="https://img.shields.io/github/v/release/xi94/Pulsar?label=release" alt="Latest release"></a>
  <a href="https://github.com/xi94/Pulsar/actions/workflows/portable.yml"><img src="https://github.com/xi94/Pulsar/actions/workflows/portable.yml/badge.svg" alt="Build status"></a>
  <img src="https://img.shields.io/badge/platform-Windows%20%7C%20macOS-informational" alt="Windows and macOS">
</p>
<p align="center">
  <a href="https://github.com/xi94/Pulsar/releases/latest"><b>Download</b></a>
  &nbsp;·&nbsp;
  <a href="#features"><b>Features</b></a>
  &nbsp;·&nbsp;
  <a href="#build-from-source"><b>Build</b></a>
</p>
<br>
<p align="center">
  <img width="900" alt="Pulsar library" src="https://github.com/user-attachments/assets/f0cab3a2-859f-4ae6-abbb-789c807ad91d" />
</p>
<table align="center">
  <tr>
    <td align="center">
      <img width="420" alt="Account list" src="https://github.com/user-attachments/assets/e138a1b1-2e32-4a67-a626-9ef3527255dc" /><br>
      <sub>Log in to all games</sub>
    </td>
    <td align="center">
      <img width="420" alt="Tray login" src="https://github.com/user-attachments/assets/008b4d94-f8a0-4b45-aca3-2dd1801ab8ca" /><br>
      <sub>Log in from the Tray</sub>
    </td>
  </tr>
</table>
<br>

## Features
 
- **One-click login** — signs in to the Riot Client for you
- **Every Riot game** — LoL, TFT, VALORANT, 2XKO and Runeterra <sub>(Mac: LoL and TFT)</sub>
- **Encrypted vault** — Argon2id + XChaCha20-Poly1305, never leaves your PC
- **Fast search** — <kbd>Ctrl</kbd> <kbd>S</kbd> / <kbd>⌘</kbd> <kbd>S</kbd>, with favorites, notes and regions
- **Private** — auto-lock, clipboard clears after 30s, hidden from screen capture
- **Tray login** — straight from the system tray or menu bar
- **Your look** — 13 themes, accents, fonts and backgrounds
## Download
 
Grab the latest from [**Releases**](https://github.com/xi94/Pulsar/releases/latest).
 
| Platform        | File                   |                                  |
| --------------- | ---------------------- | -------------------------------- |
| Windows 10 / 11 | `Pulsar.exe`           | Install or portable. Auto-updates. |
| macOS 13.3+     | `Pulsar-<version>.dmg` | Apple silicon and Intel.         |
 
> [!NOTE]
> **macOS first launch:** the app isn't notarized yet. Go to **System Settings → Privacy & Security** and click **Open Anyway**, then allow **Accessibility** so Pulsar can sign you in.
 
## Build from source
 
<details>
<summary><b>Windows</b> — Visual Studio 2022+ with <i>Desktop development with C++</i></summary>
  
```bash
cmake -S . -B build -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target Pulsar
```
 
</details>
<details>
<summary><b>macOS</b> — Xcode command line tools and CMake</summary>
  
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --build build --target package   # optional .dmg
```
 
</details>
<details>
<summary><b>Tests and project layout</b></summary>
Build `pulsar_tests`, then run:
 
```bash
ctest --test-dir build --output-on-failure   # add -C Release on Windows
```
 
Platform code is in `src/os/`, `src/render/` and `src/login/`. Everything else is shared.
 
</details>
