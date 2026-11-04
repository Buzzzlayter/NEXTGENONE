# Settings screen and HFSM debug ownership

Status: implementation compiled; offline asset migration completed. No gameplay tests authorized or executed.

Cleanup, 2026-11-03: NGSettingsAssets.cpp and NGSetup -SettingsOnly/-RemoveALSMenu were removed. Migration commands below describe completed historical work, not supported setup steps. Existing settings widgets/runtime classes are preserved; further Blueprint configuration is manual.

## Implementation

`NGSettingsPanel.h/.cpp`: native UMG screen, staged fields with Apply / defaults / Back; machine graphics settings through UGameUserSettings; local persisted sensitivity, Y inversion and master volume through UNGInputPreferences. Graphics: overall quality, supported resolution, window mode, VSync, FPS cap. Overall quality Custom preserves granular quality. Input section documents current core bindings; it is not a key rebinding editor. Sensitivity multiplies PlayerController look input, including gamepad look, before the unchanged ALS camera path. Master volume applies to the current world's audio device and is restored on local Controller BeginPlay.

Lyra references inspected locally: Settings/LyraGameSettingRegistry.cpp, LyraGameSettingRegistry_Video.cpp, LyraGameSettingRegistry_MouseAndKeyboard.cpp, LyraGameSettingRegistry_Audio.cpp. Adopted the separation of machine settings and local preferences, category descriptions and explicit Apply. This is an adaptation to existing UMG/HFSM, not a port of Lyra's CommonUI/GameSettings registry or assets. No new framework dependency.

`NGDebugState.h/.cpp`: a project ALS debug-component subclass routes overlay requests into an HFSM Runtime component. The Runtime component owns ALS_HUD and OverlayStateSwitcher through the existing WidgetMounter, binds to the current Pawn and releases widgets, possession delegates and key bindings on Exit / AbortEnter. No global time dilation is used by the overlay menu. Debug widgets are disabled in Shipping. The original ALS plugin assets/source remain unchanged.

`NGSettingsAssets.cpp`, `NGSetup -SettingsOnly`: offline migration creates WBP_GameSettings and a project copy BP_NGDebugComponent. The copy retains coloring/debug graphs but disables possession-time HUD spawning and inherits native overlay request routing. Character SCS is changed to the copied component while preserving settings, graphs and AnimBP. FSMState_GameSettings loses only the accidental HealthPanel mounter, receives the settings mounter and UI input mode; Runtime receives debug UI ownership/navigation. HUD gets Settings and Back transitions, retaining the existing Game FSM and other transitions. Modified assets are backed up under Saved/RefactorBackup/Settings before migration.

## Connected navigation

Esc in Runtime -> Settings event -> FSMState_GameSettings. The screen's Back button / Esc emits Back -> Runtime. Runtime Exit closes inventory/debug/interaction UI through existing components. Settings blocks movement/look and ends active weapon input; it does not pause the networked world. Screen destruction discards staged fields. Settings are local and do not change server gameplay state.

## Pending verification

NEXTGENONEEditor Win64 Development was compiled with the editor closed. NGSetup -SettingsOnly compiled and saved the UI assets. These steps did not start PIE. Actual screen appearance, keyboard/gamepad focus, repeated transitions, respawn binding, inventory-to-settings sequencing, video/audio persistence and a packaged build remain unverified. Gameplay checks require separate explicit user approval.

Limitations: no key remapping, individual audio buses or display-confirmation countdown; Apply confirms the chosen video mode immediately. Native WidgetTree fields are generated at runtime: an authored replacement must preserve the named controls or override the screen. In the editor, resolution changes may affect standalone/editor windows differently from packaged play. No runtime success claim is made before approved tests.
## Files and delivered build

New source: Source/NEXTGENONE/Gameplay/UI/NGSettingsPanel.h/.cpp, NGDebugState.h/.cpp, NGSettingsAssets.cpp.
Changed source: Source/NEXTGENONE/Gameplay/Player/NextgenonePlayerController.h/.cpp and NGSetupCommandlet.cpp.
New assets: Content/Gameplay/UI/WBP_GameSettings.uasset and BP_NGDebugComponent.uasset.
Changed assets: Content/Blueprints/BP_NextgenoneCharacter.uasset, BP_NextgenoneHUD.uasset; Content/Blueprints/States/FSMState_GameSettings.uasset and FSMState_GameRuntime.uasset.
Docs: this file, Docs/Architecture/GameplayFoundation.md, Docs/Testing/MultiplayerCombat.md.

Final NEXTGENONEEditor Win64 Development build: UBT exit 0 / PASS, no compiler diagnostics. Final NGSetup -SettingsOnly: exit 0, 0 errors, 14 warnings from ALS assets / deprecated APIs and the environment. No unconnected Event transition warning remains. Asset graph logging records Settings -> settings-state and Back -> runtime-state, with the original Runtime Event connection preserved. This is serialized graph/Blueprint compilation evidence, not runtime testing.

Manual next step: open the project normally. WBP_GameSettings is under /Game/Gameplay/UI; its mounter is already present in FSMState_GameSettings. No manual widget wiring is required. Runtime/visual checks still require separate approval.
Preservation note: Git currently also reports modified original ALS DebugComponent, ALS_HUD and OverlayStateSwitcher assets. Their last-write timestamps are 21:39:46 Moscow, before this offline migration (the project debug copy was created at 21:42:40). They were preserved, not reverted. The migration explicitly saves only the project copy and project Character/HUD/state assets; it does not save the original plugin assets. Therefore “plugin unchanged” above describes this migration's write scope, not a clean plugin working tree.
## Update: removed the Tab menu — 2026-11-01

This supersedes the earlier Runtime debug-HUD/overlay ownership description. Runtime now owns only Settings navigation; it no longer creates ALS_HUD or OverlayStateSwitcher, and saved references to both classes were cleared. The original plugin assets remain intact. DebugHUDClass/OverlayClass and the old helper type are serialized compatibility only, without an active spawning path.

IMC_Debug inspection confirmed Tab -> DebugToggleHudAction, Q -> demo overlay, and several debug shortcuts conflicting with gameplay input. The controller now uses a project copy /Game/Gameplay/Input/IMC_NextgenoneDebug. The migration removes the 11 known ALS demo/debug actions, preserving non-legacy custom mappings. The current copy has zero remaining mappings; default character and combat/ability mapping contexts are unchanged. No Tab menu is rebound elsewhere.

Esc -> Settings -> Diagnostics now embeds /Game/Gameplay/UI/WBP_DebugSettings (UNGDebugSettingsPanel): debug camera, ALS traces, collision/debug shapes and animation-layer colors. It uses existing ALS debug functionality and immediately applies these development switches; Apply/Back staging remains for normal graphics/input/audio settings only. The panel also displays current movement state, HP, speed, weapon, magazine and reserve, resolving the currently possessed Pawn on refresh. The section is hidden in Shipping.

Demo overlay props, debug mesh swapping, slow motion and selecting other debug characters were not moved into gameplay settings. Weapon presentation remains owned by GAS/equipment. ALS diagnostic flags retain the original plugin's process-local static behavior; they are not replicated gameplay settings and can affect multiple worlds in single-process PIE.

Files: NGDebugState.h/.cpp, NGSettingsPanel.h/.cpp, NGSettingsAssets.cpp, NGSetupCommandlet.cpp; new NGDebugSettingsPanel.h/.cpp. New assets: WBP_DebugSettings and IMC_NextgenoneDebug. Updated assets: BP_NextgenonePlayerController, FSMState_GameRuntime, WBP_GameSettings. Backups: Saved/RefactorBackup/Settings.

Verification: NEXTGENONEEditor Win64 Development PASS / UBT exit 0, no compiler diagnostics; NGSetup -RemoveALSMenu exit 0, 0 errors / 13 ALS/environment warnings. Logs: Saved/Logs/NGRemoveALSMenuBuild.log and NGRemoveALSMenuAssets.log. No gameplay/PIE/network tests run. Actual disappearance of the menu, control/focus behavior, diagnostics and respawn refresh still await explicit user-approved runtime checks.