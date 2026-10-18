# Quest 2 alignment, head camera and Play / Launch VR

Player download: copy the compiled DLL, launcher and OpenXR loader into an
existing ReSkate installation, connect Link / Air Link, and click LAUNCH VR.
See PLAY.txt. Players do not need a terminal or development tools.

PLAY disables VR for that session and preserves saved display options.
LAUNCH VR sends a session-only VR start request and requests a 2048-square
window without changing saved launcher display preferences. The runtime loads
the player's profile, applies the shared Quest 2 balanced preset once, and
preserves subsequent tuning. Future VR starts enable VR with that saved tuning.
The in-game Quest 2 balanced button uses the same preset and also enables VR.

Depth stereo resamples the symmetric camera source into each runtime eye's
asymmetric frustum. The submitted frusta match the mapped images. Invalid
frusta keep the symmetric image and submission instead of submitting zero FOV.
Camera records include the FOV correction sent to the game. The effect of that
correction on the game's actual rendered view still needs headset validation.
All 48 depth-search samples are retained. Alternate-eye, ReShade side-by-side
and the no-depth fallback retain their original projection paths.

## Checks and build

The native VR tests exercise the production projection helper against expected
eye rays, identity mappings, invalid inputs and source coverage; they also check
the shared starting preset. Four Python reference checks and D3DCompile of all
three stereo shaders provide additional checks.

The local Windows build uses MSVC 19.51, with C4702 suppressed only in the generated
project for three unchanged upstream files: game_ui_state.cpp, spotify_web.cpp
and youtube_music.cpp. BUILD_LOCAL.ps1 reproduces this developer build.
The GitHub workflow uses the upstream VS 2022 preset, packages the verified
Khronos OpenXR 1.1.63 loader, and uploads a ready-to-copy artifact.

Headset optics, installed-game compatibility, Link behavior and runtime performance
remain untested. This is single-view depth reconstruction and can retain artifacts.
Binary auto-updates and built-in Spotify Connect are disabled for the test build.
Base source: b19a2c920364223fffc874fd2350806aac8fdcdf on cesarcc2/ReSkateVR's vr branch.

## Head camera v2

A HeadOrigin samples the native eye position relative to player locomotion once,
then follows that rigid player frame. It is resampled on recenter, teleport, or
mode changes. Animated eye position and head-bone yaw no longer drive this mode.
The HMD's tracked pose remains independent and unfiltered. The v3 player rig
turns with the trajectory root, calibrated once against the torso heading.
Subsequent animated torso turns and invalid samples cannot reset that heading.
Tick and render camera writers both use the raw head snapshot and body frame;
standard first person retains its normal spring smoothing. This still needs
physical testing through skating maneuvers.

The camera-only profile migration zeroes both board and on-foot offsets,
enables positional tracking and a level horizon, and disables automatic outside
cameras. It preserves saved depth, FOV and frame pairing. The frame delay still
pairs source images; it no longer adds future-pose prediction in this mode.
Existing source-to-eye projection is retained. Do not label pitch distortion or
performance resolved until physical testing confirms it.

Native tests cover stationary rotation through near-vertical pitch and roll,
orthonormal camera axes, direct lean, animation isolation, locomotion, recenter
resampling, and preservation of existing stereo calibration during migration.
Physical Quest 2 testing and sustained frame-rate measurements remain outstanding.

## v3 launcher and game HUD

Both PLAY and LAUNCH VR remain available after installation. A session-only
explicit 0/1 request overrides the saved VR enabled state without saving that
choice over the user's calibrated profile. The selected action survives mod
update and merge prompts. Tests exercise saved display options and profile
calibration preservation for both choices.

ADDONS > VR > Show game HUD uses the existing native game UI visibility
control for scores, objectives and indicators. It is enabled when the native
UI control is available. The HUD remains part of the reconstructed source
image; this does not extract it into a separately rendered headset layer.

Regression tests cover torso motion, missing headings and real trajectory
turns during pushes. Pitch distortion and frame pacing remain subject to
headset testing and the single-view renderer's limits.
