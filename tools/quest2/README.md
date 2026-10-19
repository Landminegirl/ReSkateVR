# OpenXR alignment, head camera and Play / Launch VR

Player download: copy the compiled DLL, launcher and OpenXR loader into an
existing ReSkate installation, connect Link / Air Link, and click LAUNCH VR.
See PLAY.txt. Players do not need a terminal or development tools.

PLAY disables VR for that session and preserves saved display options.
LAUNCH VR sends a session-only VR start request and requests a 2048-square
window without changing saved launcher display preferences. The runtime loads
the player's profile, applies the shared Balanced VR preset once, and
preserves subsequent tuning. Future VR starts enable VR with that saved tuning.
The in-game Balanced VR button uses the same preset and also enables VR.

Depth stereo resamples the symmetric camera source into each runtime eye's
asymmetric frustum. The submitted frusta match the mapped images. Invalid
frusta keep the symmetric image and submission instead of submitting zero FOV.
Camera records include the FOV correction sent to the game. The effect of that
correction on the game's actual rendered view still needs headset validation.
Depth search supports 12-48 samples; v4 defaults to 24 for lower GPU work. Alternate-eye, ReShade side-by-side
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
Physical headset testing and sustained frame-rate measurements remain outstanding.

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

## v4 HUD, L3 and reconstruction performance

HUD area size (50-100%) composes native safe-area insets so HUD anchors move
inward while text remains readable. 100% exactly restores the native insets.
This uses a guarded native layout hook, described in HUD_LAYOUT.md, and never
changes the scene projection. Widgets ignoring native safe areas may not move.

Left-stick click switches VR view mode on release. Xbox XInput pads and Quest
Touch controllers are supported; direct HID Sony L3 is not implemented. A
both-sticks chord cancels view switching, retaining Touch recenter. Focus loss,
menus, disabled VR or a disabled switch setting cancel a pending click; holding
a stick across resume cannot accidentally switch views. Controller choices and
view mode persist with the other VR settings.

Default eye reconstruction dimensions are 75% of the source image and search
quality is 24 samples. Both are adjustable, independently of scene FOV, IPD,
depth strength or frame pairing. 100% / 48 restores v3 quality. This reduces
reconstruction workload, not the cost of the base game's scene render. Hardware
frame-rate gains remain unmeasured. Eye-chain resizing waits for both copy and
stereo GPU work before destroying resources. Logs include eye dimensions and
sample count so a headset test can compare actual rates.

Checks cover click hold/release, chord cancellation, focus/menu interruption,
native-area identity and inset math, eye dimensions, existing projection, head
camera and launch behavior. Native hooks and HUD widget coverage still require
physical game/headset validation.

## v5 HUD defaults and head-turn stability

HUD area now starts at 50% and supports 10-100%. Existing profiles receive the
50% value once on the next Launch VR, then retain subsequent changes.

The old pairing path collected camera writes from a 60 Hz engine thread at
45-51 presents/s in the user's log, sometimes finding no new write or several.
A fixed present delay cannot establish the orientation of an interpolated render
view. The new observer reuses the existing guarded native shader-view hook and
matches its main-view world matrix against recent issued VR camera transforms.
It converts observed orientation back through the issued rig-to-tracking transform,
uses the observed native FOV, and hands that record to the colour snapshot at
Present. Captured camera metadata stays attached to kept colour/depth. Once
native pairing is established, a missing observation holds the kept frame rather
than assigning newer simulation metadata to old pixels. Stale capture falls back
to legacy pairing with a warning. Capture-to-Present ordering and multi-threaded
renderer behavior still need verification in the actual game.

Immediately before depth reconstruction the Present thread locates views for the
current OpenXR frame. The shader transforms target-eye rays into the source-camera
orientation, projects them into the source image, then performs depth parallax.
Only warped pixels are submitted with the target orientation. This is a rotational
warp; it does not synthesize new scene motion or implement positional timewarp.
Exposed rays outside the source image become black instead of streaking border
pixels. Very fast turns, source-pose mismatch, and HUD pixels embedded in the scene
can retain artifacts. The Late head rotation toggle provides an opt-out.

The submitted pose and frustum describe the image: see the
[OpenXR frame submission guide](https://github.com/KhronosGroup/OpenXR-Guide/blob/main/chapters/frame_submission.md)
and [projection-layer specification](https://registry.khronos.org/OpenXR/specs/1.1/html/xrspec.html#XrCompositionLayerProjectionView).

Native tests exercise fixed-landmark alignment under 50 Hz source / 90 Hz target
rotation with pitch and roll, and removal of an 80-degree player rig from observed
tracking orientation. The existing tests cover camera/eye projection and controls.
Local shader compilation and Python reference tests pass. Windows C++ compilation
and those native tests must pass in the release workflow before delivering a player
ZIP. Physical headset jitter reduction is not established by these tests.

## v6 view centering and menu interaction

Changing view mode or head-centred tracking now requests a recenter. The camera
resets HMD anchor, rigid eye origin, heading, outside/bail/grab state, boom/lift,
turn state and motion timing. Menu pointer drags cancel before recentering.
Right-stick click recentres once per press, including menus. Left-stick click
retains first/third-person switching; a concurrent right click cancels that gesture.

The menu-button gesture sends Start only after a short release. At 0.6 seconds it
opens ReSkate once and suppresses both Back/filming input and a short-release event.
Held buttons across focus loss require release before arming again.

A right-hand OpenXR aim action casts a -Z ray into the actual theatre quad.
The optional pointer draws a controller-origin beam and crosshair as tiny
alpha compositor quads. Their GPU descriptors/allocators are fence protected;
layer count respects the runtime limit, and visual failures skip optional layers.
The menu panel recentres on opening. See the
[OpenXR quad definition](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrCompositionLayerQuad.html).

ReSkate gets pointer input on its ImGui thread. Native game menus get absolute
mouse motion/button events only while the game window is foreground. Right trigger
is reserved for pointing only while menus and the pointer are enabled. Trigger
holds across menu/context entry do not click until released. Ray/panel loss, focus
loss, recenter, stopping VR and teardown release owned presses. Native menu widget
compatibility and actual pointer visuals still need headset testing; gamepad navigation
remains available, and Controller menu pointer is an opt-out.

Regression coverage adds menu threshold/hold/focus behavior, single-press recenter,
trigger drag cancellation, panel edges/backface/rotation, beam placement and repeated
tracking-origin resets. The v5 late-head-rotation shader is unchanged.


## v8 setup and controls

Launcher Settings > VR saves a single square side, independently of standard
PLAY settings. The launch path reapplies it after argument parsing. The runtime
continues to reject exclusive fullscreen/maximize and constrains square resizing.
The first VR launch opens a five-page guide; closing cancels startup and only
finishing acknowledges it. It can be replayed in the launcher or headset menu.

ADDONS > VR > CONTROLS selects VR controllers + gamepad or Regular gamepad only.
Twelve tracked-controller inputs (including short/hold menu gestures) map to game buttons,
VR shortcuts, D-pad shift or Voice talk. The click input for the right-hand
ray is separate and consumes its gameplay action in menus. Bindings and voice
mode are profile values and preset fields; reset bindings does not alter comfort.
Changes require releasing controls so held inputs cannot activate a new mapping.

The voice worker combines VR controller Voice talk with the existing keyboard/controller
PTT gate, then applies hold/toggle activation while VR is enabled. A release is
required after binding/source/mode/session changes. Existing host policy, focus,
menu, peer and microphone-volume gates remain in force. Open mic stays separate.
Steam capture and headset UX are not established by the pure routing/latch tests.


## v9 menu pointer, source framing and pacing

Right-stick vertical input becomes an ImGui mouse wheel under the active ray;
normal right-stick navigation events are suppressed in that mode to avoid double
scrolling. Dead-zone and elapsed-time scaling keep it consistent across refresh
rates. Off-panel rays emit no scroll. Without ray mode, regular navigation stays.

Native pointer injection validates the projected pixel against WindowFromPoint's
root owner before moving and checks cursor/foreground ownership again before
submitting the validated move and press together as an ordered SendInput batch. Off-desktop, occluded or other-window targets are rejected; no ClipCursor
trap is used. A drag releases on invalid targets and requires release to rearm.
This specifically addresses square clients larger than the physical monitor.

The optional Sharper source framing uses correction 1 only when measured native
FOV pairing is live in depth stereo. Actual measured FOV still drives submitted
image projection. The old calibration remains saved and returns when this is off
or measured pairing is unavailable. Render FOV remains the overscan margin. A source
coverage estimate and log fields help compare pixel utilization; fast-turn holes
are still possible. Projection regression cases confirm identical target rays for
the old/new symmetric source FOVs. It is not a physical image-quality validation.

Headset presentation pacing sets the observed primary swapchain's Present interval
to zero only while VR tracking/pacing is active. OpenXR still paces the frame;
standard play and Present(TEST) are unchanged. This avoids a desktop V-Sync wait,
but does not uncap the game's frame limiter or synthesize 90 scene frames. The
latest user's source log was roughly 60 presents/s at a 72 Hz headset. 90 Hz
comfort/artifact improvement still requires hardware testing.

Input batch ordering follows [Microsoft SendInput documentation](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput).


## v10 startup input and default framing

Sharper source framing defaults on. A one-time VR.SharperDefaultV10 profile migration
also enables it for older VR profiles; subsequent manual changes remain saved.
Virtual XInput pad0 now supplies GetCapabilities as well as GetState, so discovery
can recognize it before OpenXR input becomes active. Real device capabilities,
other slots, invalid requests and standard/gamepad-only launch choices are retained.
The launcher grants foreground permission before the child startup callback can
hide it or show the startup splash. No repeated focus-stealing or synthetic click
is used. These changes target initial pause-menu input but still need headset tests.

The inspected 90 Hz run was ~80 presents/s at2165square (camera60/s). Sharper framing
was active and headset presentation pacing was off. This is consistent with a frame
budget issue, not evidence that 90 Hz artifacts are resolved. The public change list
is CHANGES_FROM_ORIGINAL.md; it credits original VR work separately from our changes.

## v11: VR chat and native pause-pointer ownership

CHAT appears below the flat pause/ReSkate menu in VR; it is disabled offline.
An open VR chat uses the main ray-interactive theater panel instead of the passive
hand chat texture. Existing multiplayer chat transport/history, 200-byte limits
and typing notices are reused. The keyboard edits a bounded UTF-8 buffer, and
failed queue requests preserve drafts. Desktop T/Enter input remains available.

Native-menu rays now extend legacy raw-mouse registration overrides and cursor
clip/recenter guards beyond overlay-only menus. Existing foreground/window/monitor
target checks and release-before-arm behavior remain. Hardware confirmation of
the initial refocus workaround is pending.


## v12: menu chat access and native capture initialization

The original MultiplayerChat.available flag hides passive chat and T input while
s.game_menu is true. v11 incorrectly used it for the explicit VR chat button,
which disabled/closed chat while paused. A separate session_available flag now
reports the transport session independently of passive visibility. The legacy
available semantics remain for desktop/passive chat. Chat targets scale from
270x66 to 720x176 pixels (360x88 at 1080 high); offline has a visible explanation.

Native pointer ownership now covers SetCapture and releases game-owned capture
on its window thread. A posted, foreground-checked one-time activation handoff
reasserts the current active state after the VR pointer is ready. It does not
post deactivation, use SetForegroundWindow, or switch desktop windows. Failed
posts/lost foreground are retryable; completed handoffs are not looped. The log
records delivery. Physical headset confirmation remains necessary.

vr_chat_ui tests run actual ImGui button press/release while the session is
connected and paused, including offline disabling and scaled sizes. Native math
checks cover activation-post deduplication, foreground loss, cancellation and
stale HWND requests, in addition to the existing VR/input/text checks.
