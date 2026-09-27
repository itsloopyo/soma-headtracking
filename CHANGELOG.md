# Changelog

## [Unreleased]

### Changed

- Settings move to `CameraUnlock.ini`. Earlier versions of the mod kept these settings in `HeadTracking.ini`, in the same folder. The first time this version starts and finds no `CameraUnlock.ini`, it reads your settings from `HeadTracking.ini` and writes them into `CameraUnlock.ini`. It never changes `HeadTracking.ini`, and does not read it again while `CameraUnlock.ini` exists.
- A setting that the defaults the README shows set to `default` is written as `default` when you never changed it from the default earlier versions used, because `HeadTracking.ini` does not hold it or holds that default. It then follows `Defaults.ini`, so it takes the value `Defaults.ini` gives it, or the built-in value where `Defaults.ini` gives none, which can differ from the default earlier versions used. A setting you changed is written with the value imported for it, or as `default` where that value equals its default at that start.
- `RotationEnabled` and `PositionEnabled` are one setting here, the tracking mode, so both are written as `default` or neither is.
- Comments, and keys the mod never read, are not carried over. Nor are these, where your old file had them:
  - A sensitivity or axis inversion you changed from its default (`[Sensitivity]` `YawMultiplier`, `PitchMultiplier`, `RollMultiplier`, `InvertYaw`, `InvertPitch`, `InvertRoll`, and `[Position]` `SensitivityX`, `SensitivityY`, `SensitivityZ`). Set these in your tracker instead.
  - `[Crosshair] Compensate`. The crosshair always follows the interaction ray now.
  - `[Position] Enabled`, which kept positional tracking off whatever the tracking mode said. Where your old file had it off, the mod starts in the rotation-only tracking mode instead, and the mode hotkey can now turn positional tracking back on.
- An older version of the mod reads `HeadTracking.ini` and never reads `CameraUnlock.ini`, so a setting you change after updating is not in `HeadTracking.ini`.
- Deleting only `CameraUnlock.ini` makes the next start read `HeadTracking.ini` again. To go back to the defaults, replace everything in `CameraUnlock.ini` with the defaults the README shows. Every setting they set to `default` then follows `Defaults.ini`.
- Hotkeys are written as key names, and each hotkey lists every key that triggers it, the Ctrl+Shift chord included: `ToggleKey=End, Ctrl+Shift+Y`. The chords were fixed in code before; now each can be rebound or removed like any other key. The key names are `ToggleKey`, `CycleTrackingModeKey` (was `TrackingModeKey`) and `YawModeKey`.
- The tracking mode hotkey and the yaw mode hotkey save their new state to `CameraUnlock.ini`, so it comes back at the next start. `End` still changes the current session only.
- Settings are named as every head tracking mod on `CameraUnlock.ini` names them: `[Network] UdpPort`, `[General] EnableOnStartup` (was `AutoEnable`), `[General] WorldSpaceYaw`, `[Smoothing] LocalSmoothing` and `RemoteSmoothing`, and `[Position] PositionLimitX`, `PositionLimitY`, `PositionLimitYDown`, `PositionLimitZ` and `PositionLimitZBack`. `PositionLimitYDown` is new: the old `LimitY` set both vertical limits, and is imported into both. `SuppressEyeTracking` and `FieldOfView` stay under `[Camera]`.
- A `FieldOfView` from 0 to 30 used to be raised to 30. In `CameraUnlock.ini` only `0` or a number from 30 to 120 is read; any other value is named in the log and the game's own field is used.

### Added

- A setting set to `default` in `CameraUnlock.ini` takes its value from `Defaults.ini`, which every head tracking mod that keeps its settings in `CameraUnlock.ini` reads. Head tracking mods that keep their settings in another file do not read it, and neither do earlier versions of this mod. Writing a value in place of `default` changes that setting for this game only. When the mod saves a setting that a hotkey changed in game, it writes the new value in place of `default`, so that setting no longer follows `Defaults.ini` in this game until you set it to `default` again.
- `Defaults.ini` is `%AppData%\CameraUnlock\Defaults.ini` on Windows; `$XDG_CONFIG_HOME/CameraUnlock/Defaults.ini` on Linux, or `~/.config/CameraUnlock/Defaults.ini` where `XDG_CONFIG_HOME` is not set, under Wine and Proton too; and `~/Library/Application Support/CameraUnlock/Defaults.ini` on macOS. The mod's log, where it writes one, names the file it read.
- When the mod starts and finds no `Defaults.ini`, it creates one holding the built-in values, unless Windows runs the game as a packaged app. The mod never changes `Defaults.ini` after that.

### Removed

- `[Crosshair] Compensate`. The crosshair always follows the interaction ray.
- `[Position] Enabled`. The tracking mode is what turns positional tracking off: `PositionEnabled=false` in `CameraUnlock.ini`, or the mode hotkey.
- The sensitivity and axis inversion settings. Set these in your tracker app instead.
- With these settings at their defaults the camera moves as it did before.

## [0.0.0] - 2026-09-08

### Added
- Initial release. 6DOF head tracking for SOMA over the OpenTrack UDP protocol:
  your head moves the view while the mouse or controller keeps control of look
  and interaction.
- The crosshair follows the game's own interaction ray, so it marks what you are
  about to pick up rather than sitting at screen centre once the head is off it.
- `[Camera] FieldOfView`: a vertical field of view setting, which SOMA has none
  of in its own options. The game's scripted zooms still work from whatever you
  set, and head tracking moves the view by the same amount on screen through
  them.
- `[Camera] SuppressEyeTracking` (default `true`): SOMA's own Tobii eye tracking
  is held off while head tracking is enabled, so Extended View stops steering the
  view and offsetting the crosshair on top of the head pose.
- Windowed play starts with the game window centred on the monitor it opened on.
  The mod does this once during startup, once the window has held still; a window
  the game already centred is left alone, and so are fullscreen and borderless
  ones. There is no setting for this.
