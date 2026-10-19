# ReSkateVR — OpenXR update: changes from the original VR build

Built on **cesarcc2’s ReSkateVR** and the work of the **ReSkate contributors**.
This list compares our update with the original VR branch at commit
[`b19a2c9`](https://github.com/Landminegirl/ReSkateVR/commit/b19a2c920364223fffc874fd2350806aac8fdcdf).
OpenXR support, the original VR camera/stereo modes and the underlying game/mod
remain the original projects’ work. The changes below are our additions and revisions.

## Installation and launching

- Added separate **PLAY** and **LAUNCH VR** buttons, so the same installation can
  start in desktop mode or VR without editing files.
- Packaged a drag-and-drop Windows download with the launcher, VR runtime and
  OpenXR loader. Players do not need PowerShell, Python or a compiler.
- Added a dedicated launcher **VR settings** page with a saved square-resolution
  slider. VR and standard PLAY keep separate display preferences.
- Locked VR to a square window, including after resizing. Fullscreen and maximize
  requests are blocked to avoid accidentally breaking the VR view.
- Improved startup virtual-controller discovery and launcher-to-game foreground
  permission handoff, targeting controllers not responding until refocusing.

## Camera, tracking and image clarity

- Corrected eye-image alignment for the headset’s actual OpenXR eye projections.
- Reworked head-centred tracking to use a stable player eye origin rather than
  animated neck/head movement, reducing the offset pivot when looking around.
- Removed push-animation yaw from the head camera to address kick/push snapping.
- Reset headset origin and follow-camera state when switching first/third person,
  addressing the view becoming offset after a switch.
- Added pairing with the renderer’s actual camera/FOV and late head-rotation
  reprojection to address head-turn jitter separately from game animation rate.
- Added checks to keep scene colour, depth and camera metadata together. Old pairs
  are discarded across resolution changes, menu transitions and recentering.
- Added **Sharper source framing**, now enabled by default. With measured camera
  FOV available, it reduces wasted source-image area while preserving the intended
  headset viewing scale. It can be disabled to restore the previous framing.
- Added source-image coverage and frame-rate diagnostics, adjustable eye
  reconstruction resolution and stereo search quality.
- Added optional headset presentation pacing to avoid an extra desktop V-Sync wait
  while OpenXR is pacing the session.

## HUD and menus

- Added game-HUD controls to the VR menu.
- Added a **HUD area** slider, defaulting to **50%**, adjustable from **10–100%**.
  It brings indicators towards the centre while preserving text size.
- Added a right-controller ray/laser and pointer for interacting with menu panels.
- Added configurable click input and trigger clicking/dragging, including fixes for
  ReSkate’s menu releasing VR clicks through desktop-mouse polling.
- Added right-thumbstick up/down scrolling for the menu under the ray.
- Added guards against off-panel and out-of-window ray clicks, including projected
  targets landing on another desktop program when the VR window is larger than
  the monitor. Invalid targets release dragging and require release before rearming.

## Controls and voice

- Added a default **left-stick click/release** shortcut to switch first/third person.
- Added default **right-stick click** recentering for gameplay and the flat menu panel.
- Changed the menu button: short press opens the game menu; hold opens ReSkate’s
  menu instead of sending the filming-mode button.
- Added default **right index trigger + left stick** D-pad input, retaining the
  thumbrest alternative. The hold remains active through the menu it opens.
- Added saved, remappable actions for face buttons, triggers, grips, stick clicks
  and short/hold menu gestures. The ray-click button is separately configurable.
- Added an explicit **Regular gamepad only** option alongside VR controllers plus
  gamepad, while keeping headset tracking enabled.
- Added a bindable VR **Voice talk** action and **Hold to talk / Toggle to talk**
  modes. In VR, the chosen mode also works with existing keyboard/gamepad PTT binds.
- Kept the existing voice permission, host, focus, menu and microphone gates;
  input/session changes clear the talk latch. Voice chat still needs enabling in
  **ONLINE → VOICE**, and microphone selection still uses Steam voice settings.

## Tutorial and usability

- Added a first-VR-launch tutorial covering controls, menu interaction, HUD,
  comfort/performance settings, voice chat and regular controllers.
- Added **VR TUTORIAL** directly above **CHANGELOGS** in the launcher, with replay
  access from launcher settings and the in-game VR menu too.
- Generalized user-facing VR wording to OpenXR headsets and tracked controllers.
  This does not mean every headset/controller layout has been tested.
- Saved personal VR tuning and button mappings to the profile, with reset options.

## Current limitations and testing

- **90 Hz stutter/tearing is not confirmed fixed.** Rendering load and resolution
  still matter. The tested setup has been smoother at 72 Hz; use a refresh rate the
  game can sustain. Lowering the square source resolution can help.
- Stereo is still reconstructed from one game view. Near-object edges and fast
  turns can expose reconstruction artifacts or missing image areas.
- The startup controller/focus changes still need in-headset confirmation. A
  successful Windows build and automated checks do not establish comfort or
  compatibility with every runtime/controller.
- Experimental player packages keep binary auto-update and built-in Spotify
  Connect disabled. Source and dependency licences are supplied with the builds.
