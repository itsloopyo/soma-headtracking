# SOMA Head Tracking

![SOMA running with this mod](https://raw.githubusercontent.com/itsloopyo/soma-headtracking/main/assets/readme-clip.gif)

An unofficial head tracking mod for SOMA that moves the view with your head while your mouse or controller keeps control of look and interaction, driven by a webcam, phone, or any OpenTrack compatible tracker, with no VR headset required.

## Features

- **Decoupled look and interaction** - your head moves the view while your mouse or controller still decides what you grab and read
- **6DOF tracking** - yaw, pitch and roll plus positional lean, peek and duck
- **Works with any OpenTrack compatible tracker** - free options available for PC, iOS and Android

## Requirements

- [SOMA](https://store.steampowered.com/app/282140/SOMA/) - a purchased copy. Both shipped executables, `Soma.exe` and `Soma_NoSteam.exe`, are covered by the same build of the mod.
- A tracking source that speaks the OpenTrack UDP protocol: [OpenTrack](https://github.com/opentrack/opentrack/releases) with a webcam, a VR headset through SteamVR, or a phone app. Not bundled.
- Windows 10 or 11, 64-bit.

## Installation

1. Download `SomaHeadTracking-v<version>-installer.zip` from the [Releases page](https://github.com/itsloopyo/soma-headtracking/releases).
2. Extract it anywhere.
3. Double-click `install.cmd`.
4. Configure OpenTrack to output UDP to `127.0.0.1:4242`.
5. Launch the game.

If the installer cannot find your copy of SOMA, point it at the folder holding `Soma.exe` in one of two ways:

```powershell
# Environment variable
$env:SOMA_PATH = 'D:\Games\SOMA'
.\install.cmd
```

```powershell
# Or as a positional argument
.\install.cmd "D:\Games\SOMA"
```

### Manual Installation

If you would rather not run `install.cmd`, extract `SomaHeadTracking-v<version>-installer.zip` anywhere and copy two files into SOMA's folder, the one holding `Soma.exe`:

- `plugins\SomaHeadTracking.asi` goes in as `SomaHeadTracking.asi` - the mod.
- `vendor\ultimate-asi-loader\dinput8.dll` goes in renamed to `version.dll` - Ultimate ASI Loader. A proxy DLL is only loaded if the process already imports that name. `Soma.exe` imports none of the usual proxy names directly, but `SDL2.dll`, which the game loads out of its own folder, imports `VERSION.dll`, so that is the name the loader takes here. If another `.asi` mod has already put an Ultimate ASI Loader proxy next to `Soma.exe` under any name, keep that one and skip this file.

Everything else in the ZIP - `install.cmd`, `uninstall.cmd`, `shared\`, `README.md`, `CHANGELOG.md`, `LICENSE`, `THIRD-PARTY-NOTICES.md` and `licenses\` - does nothing at runtime.

Mod managers do not deploy this mod. Both files have to sit in SOMA's root folder next to the executable, and a mod manager deploys into one fixed subfolder instead, so a managed install puts the files where the loader never looks. Extract by hand.

## Setting Up OpenTrack

In OpenTrack:

1. Set **Output** to `UDP over network`.
2. Set the host to `127.0.0.1` and the port to `4242`.
3. Map yaw, pitch and roll, plus X, Y and Z for positional tracking.
4. Press **Start**, then launch the game.

Center the view in your tracker: OpenTrack's Center bind, SteamVR's reset, or the CENTER button in your phone app.

### VR Headset Setup

1. Connect the headset to the PC over Air Link, Virtual Desktop or a link cable.
2. Start SteamVR.
3. Set OpenTrack's **Input** to the SteamVR tracker.
4. Leave **Output** on `UDP over network`, `127.0.0.1:4242`.

### Webcam Setup

Set OpenTrack's **Input** to `neuralnet tracker`. It reads a plain webcam and needs no markers, no clip and no IR hardware. Leave **Output** on `UDP over network`, `127.0.0.1:4242`.

### Phone App Setup

The mod accepts one thing: the OpenTrack UDP protocol on port `4242`. A phone app is usable here if it sends that protocol itself, or ships a PC-side companion that does. Check your app against that before anything else.

For an app that does send it, what decides the wiring is how much filtering the app does before the packet leaves the phone. An app that filters on-device can point straight at this PC's LAN address (run `ipconfig` to find it) on UDP port `4242`. A raw or lightly filtered feed sent direct will jitter, because the mod's smoothing is sized to take the edge off a clean signal rather than to rescue a noisy one, and that app needs to go through OpenTrack so its filters and curves can clean the feed up first: send from the phone into OpenTrack on a spare port such as `5252`, then out of OpenTrack to `127.0.0.1:4242`.

Test it rather than guessing. Try direct, hold your head still, and route it through OpenTrack if the view drifts or shakes.

I made [Headcam](https://headcam.app) so decent tracking was free for anybody with a phone already in their pocket. It filters on-device, so it can send direct. Any other app that filters enough noise works identically.

A phone on WiFi is a remote connection and gets `RemoteSmoothing`. So does a tracker running on this same PC that sends to your LAN address instead of `127.0.0.1`, because the mod sees a transport and not a machine.

## Controls

Two equivalent binding sets - use whichever your keyboard has:

| Action              | Nav-cluster | Chord           |
|---------------------|-------------|-----------------|
| Toggle tracking     | `End`       | `Ctrl+Shift+Y`  |
| Cycle tracking mode | `Page Up`   | `Ctrl+Shift+G`  |
| Toggle yaw mode     | `Page Down` | `Ctrl+Shift+H`  |

`Page Up` / `Ctrl+Shift+G` cycles tracking mode:

1. Normal head-tracked gameplay
2. Positional tracking disabled, rotational tracking enabled
3. Rotational tracking disabled, positional tracking enabled
4. Back to normal

`Page Down` / `Ctrl+Shift+H` switches which axis head yaw turns about:

1. World-locked (the default). Yaw turns about the world up axis, so turning your head pans across the horizon however far the mouse has pitched the view up or down.
2. Camera-local. Yaw turns about the camera's own up axis, so with the view pitched steeply the same head movement rolls the picture instead of panning it.

The mod comes up in whichever mode `WorldSpaceYaw` selects and the key change lasts until the game closes.

## Configuration

Settings live in `HeadTracking.ini`, next to `Soma.exe`. You write that file yourself: the mod reads it at startup if it is there and runs on the defaults if it is not, and any key you leave out keeps its default. Restart the game after editing it. A value outside the accepted range is clamped or rejected, with a line in `HeadTracking.log` saying which one and what was used instead.

```ini
[Network]
; Port the tracker sends to. 1024-65535.
UDPPort=4242

[Sensitivity]
YawMultiplier=1.0
PitchMultiplier=1.0
RollMultiplier=1.0
InvertYaw=false
InvertPitch=false
InvertRoll=false
; Smoothing is picked per connection from the packet's source address. A
; tracker on this PC sending to 127.0.0.1 gets LocalSmoothing; a phone or
; another machine on your network gets RemoteSmoothing. Whichever applies
; covers rotation and position alike. Both accept 0.0 to 1.0.
LocalSmoothing=0.0
RemoteSmoothing=0.15

[Position]
Enabled=true
SensitivityX=1.0
SensitivityY=1.0
SensitivityZ=1.0
; Movement envelope in meters. Leaning forward gets far more travel than
; leaning back, so pulling away from the screen does not push the camera
; into the back of the player's own body.
LimitX=0.30
LimitY=0.20
LimitZ=0.40
LimitZBack=0.10

[Crosshair]
; Moves SOMA's crosshair onto the interaction ray. Set false to leave the
; crosshair drawn where the game puts it.
Compensate=true

[Camera]
; Vertical field of view in degrees. 0 leaves the game's own field alone.
; Accepted range is 30 to 120; anything outside it is clamped.
FieldOfView=0
; Holds SOMA's own Tobii eye tracking off while head tracking is enabled.
; Set false to leave the game's eye tracking running alongside the mod.
SuppressEyeTracking=true

[Hotkeys]
; Virtual key codes for the three keys in the Controls table.
ToggleKey=0x23         ; End
TrackingModeKey=0x21   ; Page Up
YawModeKey=0x22        ; Page Down

[General]
; true = yaw turns about the world up axis and stays horizon-locked (default)
; false = yaw turns about the camera's own up axis
WorldSpaceYaw=true
; Whether tracking is on when the game starts.
AutoEnable=true
```

### Field of view

SOMA has no field of view slider in its options menu, so `FieldOfView` in the mod's INI is the setting.

The number is the **vertical** field, which is the one SOMA works in: it fixes the vertical field and derives the horizontal one from your aspect ratio, so a wider monitor gets a wider picture rather than a shorter one. The game's own value is 70, so `FieldOfView=85` is a noticeably wider view and `FieldOfView=60` a narrower one. Values below 30 or above 120 are clamped. Leave it at `0` to keep the game's own field.

The game's scripted zooms still zoom from wherever you set the field, and the crosshair stays on the interaction ray at any setting.

### Tobii eye trackers

SOMA has eye tracking of its own, and one part of it moves the camera: Extended View turns the view toward wherever you are looking on screen, by up to 34 degrees at the game's highest `ExtendedMaxAngle` setting, and offsets the crosshair to match. That is a second thing steering the view on top of your head, and the mod's crosshair compensation does not account for it.

So `SuppressEyeTracking` is on by default. While head tracking is enabled the game is told its eye tracker is not tracking, which is the state every player without one is in - Extended View stops, and so does the rest of what the game's eye tracking drives: the larger gaze crosshair, gaze flashlight control, the reactive environment and reactive AI. Nothing is written to the game's settings or to the device, so it all comes straight back the moment you turn tracking off with `End`, and your options screen is untouched either way.

Set `SuppressEyeTracking=false` to keep SOMA's eye tracking running alongside the mod. If you have no eye tracker, the setting does nothing - the hook is installed but the game never asks.

## Troubleshooting

Read `HeadTracking.log`, which the mod writes next to `Soma.exe`. It records whether the mod loaded, whether it matched the executable and installed its hooks, whether it claimed the tracker port, whether any tracker data arrived, and any config value it rejected or clamped. It is rewritten from empty every time the game starts, so what you are reading is always the launch you just made. The session before it is kept alongside as `HeadTracking.prev.log`.

**Mod not loading**

- No log file at all means nothing loaded the `.asi`. Confirm `version.dll` and `SomaHeadTracking.asi` both sit next to `Soma.exe`, in SOMA's root folder, not in a subfolder.
- If you installed through a mod manager, that is the cause. Extract the ZIP into the game folder by hand.
- If the log says `mod dormant`, the mod did not recognise the executable and installed no hooks, so the game runs vanilla. The same line says which case it is: "newer than any build the mod knows about" means the game has been patched, so check the Releases page for an update; "older than the newest build" means the store has not finished updating; "repacked or modified" means the mod will not engage on an executable it cannot identify. Quote the `Build: EXE fingerprint` line above it if you report this - those three numbers are what a new build is added from.

**No tracking response**

- Confirm OpenTrack (or your app) is running with output set to UDP `127.0.0.1:4242` and Start pressed.
- Press `End` (or `Ctrl+Shift+Y`) to toggle tracking on.
- Check your firewall is not blocking UDP port `4242`.
- Tracking is suppressed in menus, loading screens, the pause screen and SOMA's in-world monitor renders. Get into gameplay before judging it.
- If the log says it could not bind the port, it quotes what Windows gave as the reason. Error 10048 is another program listening on `4242`, usually OpenTrack or a game left running - close it and the mod takes the port over within about half a second, no restart needed.
- Error 10013 there means nothing is holding the port and Windows is refusing it anyway, because `4242` falls inside a range reserved on your PC by Hyper-V, WSL or Docker. `netsh int ipv4 show excludedportrange protocol=udp` lists those ranges; set `UDPPort` in `HeadTracking.ini` to a port outside them and send the tracker there.

**The view swings around on its own**

- If you have a Tobii, this is SOMA's own Extended View following your gaze rather than the mod following your head. Check `HeadTracking.log` for the `Eye tracking:` line - if it says the game's eye tracking could not be held off, or you have set `SuppressEyeTracking=false`, the game is still doing it. Turning Extended View off in SOMA's own options settles it either way.

**Jittery or unstable tracking**

- Raise `RemoteSmoothing` in `[Sensitivity]` if the tracker is on another device, or add smoothing in OpenTrack itself.
- Improve your lighting for webcam tracking.
- Phone tracking over WiFi benefits from routing through OpenTrack's filters rather than sending direct.

**Yaw feels wrong when looking up or down**

- Toggle between world-locked and camera-local yaw with `Page Down` (or `Ctrl+Shift+H`). World-locked is horizon-stable; camera-local follows the camera's current up axis.
- If the view moves opposite to your head on an axis, set the matching `InvertYaw`, `InvertPitch` or `InvertRoll` in `[Sensitivity]`, or fix the axis in your tracker's own profile.

**The game window moved when I launched**

- By design, and only when you play windowed: once the game's window has held still for a few seconds after appearing, the mod centres it on the work area of the monitor it opened on. It does this once, during startup - move the window afterwards and it stays where you put it. A window the game centred itself, and a fullscreen or borderless one that already fills the screen, are left where they are.

## Updating

Download the new release and run `install.cmd` again. Your config is preserved.

## Uninstalling

Run `uninstall.cmd`. This removes the mod's files and its two logs. `HeadTracking.ini` is left where it is, because you wrote it - delete it yourself if you want it gone. Ultimate ASI Loader is only removed if the installer put it there; use `uninstall.cmd /force` to remove it anyway.

## Building from Source

Prerequisites: Visual Studio 2022 or newer with the C++ desktop workload, CMake 3.20 or newer, and [pixi](https://pixi.sh).

```powershell
git clone --recursive https://github.com/itsloopyo/soma-headtracking.git
cd soma-headtracking
pixi run build-release
pixi run test
pixi run package
```

`pixi run package` writes the installer ZIP to `release/`. No copy of the game is needed to build.

## Community & Support

- [Discord](https://discord.com/invite/dxyZdyFNT9) - setup help, bug reports, and new-release announcements
- [Lopari](https://lopari.app) - free Windows launcher with one-click install and launch of head-tracking mods
- [Headcam](https://headcam.app) - free app that turns your phone into a head tracker

## License

MIT License - see [LICENSE](LICENSE) for details.

The mod bundles and links third-party components under their own licenses. [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md) names each one and carries its notice; it ships in every release ZIP alongside `LICENSE`, with the individual license texts under `licenses/`.

## Credits

- Frictional Games, developer and publisher of SOMA
- [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) - ASI loader (MIT)
- [OpenTrack](https://github.com/opentrack/opentrack) - head tracking protocol and software (ISC)
- [MinHook](https://github.com/TsudaKageyu/minhook) - function hooking library (BSD-2-Clause)
- [miniz](https://github.com/richgel999/miniz) - zip reading, inside the ASI loader binary (MIT)
- [injector](https://github.com/ThirteenAG/injector) - hooking utilities, inside the ASI loader binary (zlib)
- [cameraunlock-core](https://github.com/itsloopyo/cameraunlock-core) - shared tracking pipeline (MIT)

## Disclaimer

This mod is not affiliated with, endorsed by, or supported by Frictional Games. Use at your own risk.
