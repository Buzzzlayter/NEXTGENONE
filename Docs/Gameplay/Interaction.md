# Предметы и взаимодействие

## Положить предмет в мир

1. Перетащить `/Game/Gameplay/Interaction/BP_ItemPickup` на Gameplay.
2. В Details выбрать **ItemClass**: `ID_Pistol`, `ID_Rifle` или `ID_TestItem` из `/Game/Gameplay/Inventory`.
3. Установить **StackCount**. Для оружия — 1; количество ограничено MaxStackSize определения.

Для нового предмета создать Blueprint от `NGInventoryItemDefinition` и заполнить DisplayName, MaxStackSize, PickupMesh либо PickupSkeletalMesh и PickupTransform. Предмет не обязан быть оружием. Weapon заполняется только для экипируемого оружия. Начальные числовые характеристики задаются фрагментами определения.

**E** вызывает `GA_Interact` через существующий ASC. Подсказка находится в `/Game/Gameplay/UI/WBP_InteractionPrompt`; она уже подключена к Runtime HFSM. При пустых руках оружие автоматически экипируется; иначе добавляется в сумку.

**I → Выбросить** отправляет серверу запрос на выбрасывание всей строки инвентаря. Предмет появляется у земли впереди персонажа, сохраняя количество и stat tags, включая патроны. Если размещение невозможно, предмет остаётся в инвентаре. Физического броска пока нет.

## Интерактивный объект

Использовать `/Game/Gameplay/Interaction/BP_InteractiveObject` или его дочерний Blueprint. Настроить Mesh, InteractionText, bEnabled, bOneShot. **OnInteractedOnServer** — серверная игровая логика; **OnStateChanged** — визуальная реакция на реплицируемый bActive. Не рассчитывать, что серверный Blueprint event сам выполняется на клиентах: сетевое представление строится от реплицируемого состояния.

Для собственного Actor реализовать интерфейс **NGInteractable**: GetInteractionPrompt, CanInteract, Interact, SetInteractionFocus. Включить Replicates и Block для trace channel **Interaction**. CanInteract оставлять проверкой без побочных эффектов; Interact изменяет состояние только на сервере и повторно проверяет доступность. Focus — локальное оформление. CustomDepth сам по себе не создаёт обводку: нужен соответствующий материал/постпроцесс.

`BP_WeaponPickup`, `ANGWeaponPickup`, `GA_PickupWeapon` и `UNGPickupWeaponAbility` удалены. Для оружия и остальных предметов используется универсальный `BP_ItemPickup` с ItemClass и общая `GA_Interact`/`UNGInteractionAbility`. IA_PickupWeapon пока используется существующими DA_Input_Character и IMC_NG_Character; это input asset, не отдельная способность. Старый input-тег остаётся совместимым маршрутом общей интеракции до перенастройки ассетов. Сборка и игровые проверки удаления не выполнялись.
## Файлы реализации

Новые C++: `Gameplay/NGInteraction.h/.cpp`, `Gameplay/NGItemPickup.h/.cpp`, `Gameplay/NGInteractionPrompt.h/.cpp` (все внутри Source/NEXTGENONE).

Изменённые C++: `Gameplay/NGAbilitySystem.h/.cpp`, `Gameplay/NGWeaponPickup.h/.cpp`, `Gameplay/NextgenoneCharacter.h/.cpp`, `Gameplay/NextgenonePlayerState.h/.cpp`, `Gameplay/NextgenonePlayerController.h/.cpp`, `Gameplay/NGSetupCommandlet.cpp`; внутри `Gameplay/Inventory`: `NGGameplayTagStack.h`, `NGInventoryItemDefinition.h`, `NGInventoryItemInstance.h/.cpp`, `NGInventoryManagerComponent.h/.cpp`, `NGInventoryPanel.h/.cpp`.

Конфигурация: `Config/DefaultEngine.ini` (канал Interaction). Документация: этот файл, `Docs/Architecture/GameplayFoundation.md`, `Docs/Testing/MultiplayerCombat.md`.

Новые ассеты: `Content/Gameplay/Interaction/BP_ItemPickup.uasset`, `BP_InteractiveObject.uasset`; `Content/Gameplay/Inventory/ID_TestItem.uasset`; `Content/Gameplay/Abilities/GA_Interact.uasset`; `Content/Gameplay/UI/WBP_InteractionPrompt.uasset`.

Обновлённые ассеты: `Content/Gameplay/Abilities/DA_PlayerAbilities.uasset`, `Content/Blueprints/States/FSMState_GameRuntime.uasset`, `Content/Gameplay/Weapons/BP_WeaponPickup.uasset`. Другие изменения, существовавшие в рабочем дереве, не относятся к этому перечню.

Полная офлайн-сборка NEXTGENONEEditor Win64 Development: UBT exit 0, PASS, 44.7 с, без диагностик компилятора. Лог — Saved/Logs/NGInteractionBuild.log. Это результат компиляции, а не игровых тестов.
## Center reticle and wider capture — 2026-11-01

Character now has a native Aim (UNGAimComponent). It computes the owning player's screen-center view ray using the constrained view rectangle, exposes GetViewRay / GetAimPoint / GetAimTarget / HasBlockingHit, and broadcasts OnAimUpdated when its local Visibility trace result changes. On the server for a remote Pawn, GetViewRay uses eyes/control rotation rather than a client camera. This presentation component does not apply damage or replicate a client hit as authority.

WBP_InteractionPrompt draws a centered circular point through its native parent. Logic and editable style live in Character.Aim: bShowReticle, ReticleSize, IdleColor, InteractionColor, UnavailableColor, TraceDistance. White is idle, green means focused interaction is available, amber means unavailable. Inventory/settings, death and loss of the local Pawn hide the point. The existing Runtime mounter owns widget lifetime; no Controller AddToViewport path was added.

Character.Interaction is the single range configuration used both by local focus and the server GA_Interact. Default MaxDistance is now 350 cm (clamped to 50..500). CaptureRadius is 45 cm (0..100): exact center hits take priority; otherwise nearby replicated interactables are selected by distance from the view ray, with range, forward-view and line-of-sight validation. Both camera-to-target and authoritative eye-to-target occlusion are checked for assisted focus. CaptureRadius=0 restores strict center-ray acquisition. These are world-space assistance units, not UI pixels. Pickup collision volumes remain unchanged and do not block bullets.

The old ability MaxDistance property remains for asset compatibility but current Characters use Interaction.MaxDistance. Existing explicit Character Blueprint component overrides are preserved; tune the component in Class Defaults if a saved override still uses the previous 250 cm range.

Weapon hit validation still uses the existing server-authoritative eye/muzzle traces. The local AimPoint is a presentation/extensibility API, not a client-provided shot target. Exact camera/muzzle parallax alignment is outside this change.

Changed source: NGAimComponent.h/.cpp (new), NextgenoneCharacter.h/.cpp, NGInteraction.h/.cpp, NGInteractionPrompt.h/.cpp. No new UI asset or ALS/AnimBP change is required. Gameplay/visual/network checks await separate user approval; compilation is reported separately.
Build/asset result for this change: NEXTGENONEEditor Win64 Development PASS, UBT exit 0 without compiler diagnostics. NGSetup -AimOnly exit 0, 0 errors / 14 ALS/environment warnings. Saved Character defaults report MaxDistance=350, CaptureRadius=45 and Aim present; prompt native tick=1. Logs: Saved/Logs/NGAimBuild.log and NGAimAssets.log. NGSetupCommandlet.cpp was also updated for this targeted asset refresh; Character, WBP_InteractionPrompt and GA_Interact assets were compiled/saved with backups. No gameplay tests run.
# Компактные виджеты (2026-11-04)

`UNGInteractionPromptMounter` удалён. Используйте два обычных `UUIWidgetMounterComponent` в UI FSM, каждый со своим WidgetClass и PlaceholderID:

- Blueprint от `UNGInteractionPrompt`: TextBlock `InteractionText` (Is Variable). Текст и видимость обновляются по `OnFocusChanged` компонента интеракции Pawn. AvailableFormat/UnavailableFormat используют `{0}` для текста предмета; клавиша не зашита в C++.
- Blueprint от `UNGReticleWidget`: Image `AimDot` (Is Variable). Цвет и видимость берутся из `UNGAimComponent`, оформление и размер задаются в UMG. Старый ReticleSize компонента больше не управляет размером этого виджета.

Компактный корень: SizeBox/контейнер по содержимому; Canvas на весь экран не требуется. Положение и выравнивание задаёт layout/placeholder. В старом Blueprint подсказки удалить AimDot и полноэкранный Canvas, если они были созданы вручную. Старый специализированный мounter убрать из состояния и заменить обычным. C++ не создаёт дерево виджетов и не добавляет их во viewport.

Blueprint настраивает пользователь. Сборка и игровые проверки этого изменения агентом не запускались.
