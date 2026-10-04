<p align="center">
  <img src="PULSAR/PULSAR_Icon_128x128.png" width="96" alt="Pulsar icon">
</p>

<h1 align="center">Pulsar</h1>

<p align="center">
  An account manager for the Riot Client. Pick an account and sign in and the selected game will automatically launch.
</p>

<p align="center">
  <a href="https://github.com/xi94/Pulsar/releases/latest"><img src="https://img.shields.io/github/v/release/xi94/Pulsar?label=release" alt="Latest release"></a>
  <a href="https://github.com/xi94/Pulsar/actions/workflows/portable.yml"><img src="https://github.com/xi94/Pulsar/actions/workflows/portable.yml/badge.svg" alt="Build status"></a>
  <img src="https://img.shields.io/badge/platform-Windows%20%7C%20macOS-informational" alt="Windows and macOS">
</p>

https://github.com/user-attachments/assets/f0b59da9-8df0-4898-887f-5817401db01f

## Features

- **One-click login** - launches the Riot Client and signs in for you.
- **Every Riot game** - League of Legends, TFT, VALORANT, 2XKO and Legends of Runeterra.
- **Encrypted vault** - locked by your master password (Argon2id, XChaCha20-Poly1305). Nothing leaves your computer.
- **Fast search** - find any account with Ctrl+S (⌘S on Mac), with favorites, notes and regions.
- **Private** - auto-lock, copied passwords clear after 30 seconds, and the window hides from screen capture.
- **Tray login** - log in from the system tray or the Mac menu bar.
- **Your look** - 13 themes, accent colors, fonts and backgrounds.

## Download

Get the latest version from [Releases](https://github.com/xi94/Pulsar/releases/latest).

| Platform        | File                   | Notes                                          |
| --------------- | ---------------------- | ---------------------------------------------- |
| Windows 10 / 11 | `Pulsar.exe`           | Install it or run it portable. Updates itself. |
| macOS 13.3+     | `Pulsar-<version>.dmg` | Universal: Apple silicon and Intel.            |

**First launch on macOS:** the app isn't notarized yet, so macOS blocks it once. Open **System Settings > Privacy & Security** and click
**Open Anyway**. Pulsar then asks for the **Accessibility** permission, which it needs to sign in to the Riot Client.

## Build from source

**Windows** - Visual Studio 2022 or newer with *Desktop development with C++*:

```bash
cmake -S . -B build -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target Pulsar
```

**macOS** - Xcode command line tools and CMake:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --build build --target package    # optional: builds the .dmg
```

Platform code lives in `src/os/<platform>`, `src/render/<api>` and `src/login/<platform>`. Everything else is shared.

## Disclaimer

Pulsar isn't endorsed by Riot Games and doesn't reflect the views or opinions of Riot Games or anyone officially involved in producing or
managing Riot Games properties. Riot Games and all associated properties are trademarks or registered trademarks of Riot Games, Inc.
