# HFSM context ownership

## Machine owner

The object used to launch the GCFSM root is the machine's explicit context.
GCFSM already passes that context to child states. `GetContextOwner()` exposes
it on `UHFSMState` and `UHFSMStateComponent`; there is no second mutable owner.

Generic components use `UHFSMStateComponent`. They require a valid context and
world, but do not look up a player, bind input, or configure local presentation.
Their `GetWorld()` reads the owning state's context. The existing Blueprint
`World` property is populated by Init/Enter, not inferred from UObject Outer.
Generic transition helpers are no longer marked BlueprintCosmetic, so they
can be used by server-side gameplay graphs.

## Local-player adapter

`UHFSMPlayerStateComponent` extends the generic component for local UI/input.
During Init it resolves the explicit context and captures UE's
`FLocalPlayerContext` for that activation. Player access remains tied to that
LocalPlayer until exit, even if the owning pawn/HUD changes controller.
Input delegates bind only to the resolved controller's InputComponent and
are removed from that same component on exit/rollback.

Layouts, widget mounters, resolution observers and HFSM input components use
this adapter. Layouts and viewport-fallback widgets use `AddToPlayerScreen`,
so their owner also determines the player screen they are mounted into.
Resolution events are filtered by the owner's viewport.

`UHFSMContextLibrary` provides the shared resolution rules:

| Context | Player source |
| --- | --- |
| HFSM/GCFSM state | Original machine context |
| HFSM component | Owning state's context |
| Local-player state component | Captured LocalPlayer for its activation |
| PlayerController | That controller's LocalPlayer |
| Pawn | That pawn's current PlayerController |
| HUD | That HUD's owning PlayerController |
| PlayerState | That PlayerState's PlayerController |
| Widget | That widget's owning LocalPlayer/PlayerController |
| ActorComponent | Its owning actor, using the rules above |
| LocalPlayer | That exact player |
| LocalPlayerSubsystem | That subsystem's LocalPlayer |
| World, GameInstance, arbitrary UObject | No implicit player |

An unpossessed pawn, AI pawn, remote controller, invalid owner or unsupported
context cannot silently acquire another player's UI/input. A local-player
component fails entry when its context cannot identify a local player.

## Migration

- Launch UI machines on the relevant HUD, controller, widget or LocalPlayer.
- Keep actor/world/session machines on their actual gameplay owners and use
  generic components there. World/session context objects must provide GetWorld.
- For custom player services, add an explicit adapter rather than searching
  the world's controllers. A GameInstance does not identify one local player.
- Replace Blueprint wiring that passes GetWorld into GetInputSubsystem or
  SetInputMode with the state/component itself or the intended player context.
  These nodes default their context pin to self and retain the WCO pin name.
- Reparent generic Blueprint components that use input events to
  HFSMPlayerStateComponent. Existing layout, widget mounter, input and resolution
  component subclasses inherit the player adapter automatically.
- Rebuild C++ and recompile affected Blueprint assets after changing native
  parent classes. The agent does not modify those assets or run the build.

## Verification

`UIModule.HFSM.ComponentActivation` covers lifecycle and player-binding rollback.
`UIModule.HFSM.ContextOwnership` covers explicit player resolution, HUD/widget
contexts, rejection of world/unpossessed-pawn fallback, activation-scoped player
identity, isolated binding cleanup with two controllers, and generic world-owned
components that do not manage input. These tests have not been compiled/run.

After the user's build, verify the actual UI graph in PIE for one player, then
two local players. Check screen ownership, input routing, focus, viewport resize,
repeated entry/exit, and teardown. Full split-screen gamepad support is not
claimed: the cursor implementation still uses Slate's global pointer radius,
and some viewport input settings are shared.
