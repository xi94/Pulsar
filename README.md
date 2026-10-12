<p align="center">
  <img src="PULSAR/PULSAR_Icon_128x128.png" width="96" alt="Pulsar icon">
</p>

<h1 align="center">Pulsar</h1>

<p align="center">
  <b>A fast, encrypted account switcher for every Riot game.</b><br>
  One click to log in to League, TFT, VALORANT, 2XKO and Runeterra.
</p>

<p align="center">
  <!-- badges unchanged -->
</p>

<p align="center">
  <a href="https://github.com/xi94/Pulsar/releases/latest"><b>Download for Windows / macOS</b></a>
</p>

<p align="center">
  <img width="900" alt="Pulsar library view" src="https://github.com/user-attachments/assets/f0cab3a2-859f-4ae6-abbb-789c807ad91d" />
</p>

<table align="center">
  <tr>
    <td align="center"><img width="420" alt="Tray login" src="NEW_TRAY_SCREENSHOT"><br><sub>Log in straight from the tray</sub></td>
    <td align="center"><img width="420" alt="Account list" src="NEW_LIST_SCREENSHOT"><br><sub>Favorites, notes and regions per game</sub></td>
  </tr>
</table>


## Features

- **One-click login** - launches the Riot Client and signs in for you.
- **Every Riot game** - League of Legends, TFT, VALORANT, 2XKO and Legends of Runeterra. On Mac: League of Legends and TFT.
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

Tests live in `tests/`. Build the `pulsar_tests` target, then run `ctest --test-dir build --output-on-failure` (add `-C Release` on Windows).

## Disclaimer

Pulsar isn't endorsed by Riot Games and doesn't reflect the views or opinions of Riot Games or anyone officially involved in producing or
managing Riot Games properties. Riot Games and all associated properties are trademarks or registered trademarks of Riot Games, Inc.
