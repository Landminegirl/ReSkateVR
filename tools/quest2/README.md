# Quest 2 alignment and one-click launcher

Player download: copy the compiled DLL, launcher and OpenXR loader into an
existing ReSkate installation, connect Link / Air Link, and click LAUNCH VR.
See PLAY.txt. Players do not need a terminal or development tools.

The launcher sends a session-only VR start request and requests a 2048-square
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
