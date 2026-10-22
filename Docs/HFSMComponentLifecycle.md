# HFSM component lifecycle

## Diagnostics

Use `ensureMsgf(Condition, TEXT("Message"))` for an unexpected recoverable
failure and an explicit return at the call site. Assertions do not replace
error handling. Expected absence during teardown uses ordinary guards.

## Enter and rollback

`UHFSMStateComponent::Enter()` now returns `bool`. The existing Blueprint
`OnEnter`, `OnTick`, and `OnExit` event signatures are unchanged.

- Validate dependencies before changing external state.
- Return `true` only after successful setup, normally via `Super::Enter()`.
- Before returning `false`, release any partially acquired resources.
- `AbortEnter()` rolls back a successful entry if a later component fails.
  Its default implementation calls `Exit(NAME_None)`.
- Override `AbortEnter()` when rollback must differ from normal exit, such as
  removing a layout immediately instead of playing its fade-out animation.

The state enters components in configured order and tracks successful entries.
A failure aborts those entries in reverse order, skips the remaining components,
and calls the Blueprint event `OnEnterFailed(FailedComponent)`.
Failed states do not run component or state Tick, nor receive a normal OnExit.
The GCFSM graph still owns the transition to a fallback/error state; handle
`OnEnterFailed` to request the appropriate transition for the game.

On success, the state's Blueprint `OnEnter` runs after its components are ready,
followed by `LaunchChildFSMs`. This changes the previous state OnEnter ordering;
initialization required by component entry must exist before activation.
`HasEnteredSuccessfully()` exposes the result to Blueprint.

Normal state exit visits only successfully entered components, in reverse order.
The generic component does not acquire or release player input. The local-player
adapter `UHFSMPlayerStateComponent` removes bindings targeting that component.
Input rollback restores previous modes and mapping-context priority; it does not
implement normal parent-state input ownership or a shared mapping-context stack.
Blueprint OnExit handlers must undo their OnEnter side effects, including
cancelling async work. Blueprint OnEnter itself still has no failure result.

`MountWidgetTo` and the instance method `SetPlayerInputMode` now return success
so their callers can propagate setup failures. Existing Blueprint callers can
ignore the added return pins; custom native overrides of component Enter must
adopt the bool return type.

## Verification

Automation test: `UIModule.HFSM.ComponentActivation`.
It covers failed entry, rollback of entered components and input bindings,
skipping subsequent components, successful activation, and repeated exit.
The test and changed C++ must be compiled and run after the user's build.

In PIE, also check a failure after a successful input/layout component: input
mode and cursor settings should return to their previous values, mounted UI
should disappear immediately, and OnEnterFailed should receive the failed
component. Verify that the graph's fallback transition is configured.
