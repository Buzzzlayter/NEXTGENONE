# UI input migration

The prototype input implementation is adapted from:

- `F:/UNREAL/Source/R5/UI/Settings/Input/InputProcessors/R5PreprocessorManager.*`
- `F:/UNREAL/Source/R5/UI/Settings/Input/InputProcessors/R5InputTypeDetector.*`
- `F:/UNREAL/Source/R5/UI/Settings/Input/InputProcessors/R5GamepadAnalogCursor.*`
- `F:/UNREAL/Source/R5/UI/Settings/Cursors/R5GamepadCursor.*`
- `F:/UNREAL/Source/R5/UI/Settings/Input/R5PlayerInputSubsystem.*`

The donor tree is a reference; implementation changes are made in NEXTGENONE.
The port uses the existing `InputData` enums and `GameSettingsConfig` settings.
No R5 module, game asset, or CommonUI dependency is required.

## Integration

`UPlayerInputSubsystem` registers one `FPreprocessorManager` per local player
and unregisters it on deinitialization. The processor checks the Slate user,
game viewport focus, console state, and simulation mode before handling input.

Configure a UI state's `UHFSMInputComponent` with:

- `bSetupInputMode = true` and either `UI` or `GameAndUI`;
- `bSetupCursorMode = true` and `DefaultCursorMode = Grid` or `Cursor`;
- a mapping context only when needed (`bSetupMappingContext`).

Give a mounted focusable button initial user focus via `SetWidgetToFocus`.
Grid mode relies on the widget tree's normal UMG navigation and focus rules;
it does not infer navigation for arbitrary non-focusable widgets.

`UI` uses UE's UI-only routing, including ignoring gameplay input. Use
`GameAndUI` when unhandled UI actions must reach controller/Enhanced Input
bindings. Binding `ToggleCursorModes` to an action remains a game decision.

In `Cursor` mode, the left stick moves the pointer, the right stick scrolls
vertically, and the platform's virtual accept key generates a mouse click.
D-pad navigation recenters the pointer on the newly focused widget.
`Grid` leaves gamepad navigation and accept events to Slate/UMG.

The subsystem detects keyboard, mouse buttons, wheel, and stick movement in
both directions. Synthetic pointer events do not change the detected device.
Mouse input temporarily disables the gamepad cursor without forgetting the
chosen mode. Game input also disables the cursor while retaining that choice.

Pass a player-owned context (state, component, controller, HUD, pawn, widget,
LocalPlayer, or LocalPlayerSubsystem) to `GetInputSubsystem` and `SetInputMode`.
Passing only a world or GameInstance no longer selects the first player.
See `HFSMContextOwnership.md` for context rules and Blueprint migration.

An optional cursor visual can derive from `UGamepadCursorWidget` and be selected
under Project Settings > Game > InputGameSettings > Gamepad Cursor > Cursor.
Without a configured class, the viewport's existing cursor remains in use.
The native class provides `OnFocusFound` and `OnViewportResized` events.
Original software cursor settings are restored on deactivation.

## Verification after the user's C++ build

1. Enter a UI state with a focused button and Grid mode. Verify D-pad/stick
   navigation, accept, and the hidden pointer.
2. Switch to Cursor. Check all four stick directions, small movements, viewport
   boundaries, slowdown over buttons, accept clicks, D-pad focus, and scrolling.
3. Move/click/wheel a physical mouse, then move either stick in a negative
   direction. Verify one device-change notification per actual switch.
4. Switch mode or leave the screen while holding accept; verify no stuck click.
5. Enter Game mode, then reenter UI; verify cursor behavior and the chosen mode.
6. Alt-tab, open the console, use editor panels, stop/restart PIE, and simulate
   in editor. Check that input is not consumed outside the game viewport and
   that no processor, cursor, or resize delegate is left behind.
7. Repeat with a custom cursor class and resize the viewport. Verify focus and
   DPI events, and that the original mouse cursor returns on device switch.

Compilation and runtime checks have not been run by the agent; the user builds
the project. Split-screen remains to be validated. Pointer radius is a global
Slate setting. Input delegates now belong to the separate local-player state
component; the generic state component has no player input behavior.

Automatic restoration of parent-state input requests, shared mapping-context
ownership, controller disconnect handling, and a Back/Confirm action router are
separate follow-up work; this port does not implement those systems.
