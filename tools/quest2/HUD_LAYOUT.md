# Native HUD safe area (20260929)

Read-only inspection of the supported Skate.exe identified:

- Rime profile keys `ScreenSafeAreaHeight` and `ScreenSafeAreaWidth`, referenced
  by RVA 0x43c6e50 through the native float profile accessor at 0x4658fd0.
- Each fraction is `(1 - profile_area) * 0.5`, stored at +0x1c/+0x20 for left/right
  and +0x24/+0x28 for top/bottom. The helper sets dirty byte +0x30 under the
  object's critical section at +0x50 (imports Enter/LeaveCriticalSection).
- RVA 0x43c7050 consumes those fractions and the float2 viewport size. It computes
  cached pixel insets at +0x78..+0x84, stores dimensions at +0x88, and propagates
  the dirty flag to +0x94. This layout call returns void.

The guarded hook holds the same recursive critical section, temporarily composes
VR area with native fractions, marks layout dirty only when cached insets differ,
and invokes the original. Native fractions are restored before leaving the lock.
100% area reproduces native insets; stopping VR or entering game menus recomputes
native pixel insets on the next layout call. No game profile values are changed.
The hook is installed after supported-image validation and requires a matching
instruction prefix. Unknown builds leave the control unavailable.

This moves HUD anchors inward while retaining text size. Widgets that do not use
the native safe-area layout may remain at their existing positions; physical HUD
coverage and layout still need Quest testing.
