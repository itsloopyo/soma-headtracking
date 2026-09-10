# Changelog

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
