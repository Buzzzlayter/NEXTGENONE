# Предметы и взаимодействие

## Положить предмет в мир

1. Перетащить `/Game/Gameplay/Interaction/BP_ItemPickup` на Gameplay.
2. В Details выбрать **ItemClass**: `ID_Pistol`, `ID_Rifle` или `ID_TestItem` из `/Game/Gameplay/Inventory`.
3. Установить **StackCount**. Для оружия — 1; количество ограничено MaxStackSize определения.

Для нового предмета создать Blueprint от `NGInventoryItemDefinition` и заполнить DisplayName, MaxStackSize, PickupMesh либо PickupSkeletalMesh и PickupTransform. Предмет не обязан быть оружием. Weapon заполняется только для экипируемого оружия. Начальные числовые характеристики задаются фрагментами определения.

**E** вызывает `GA_Interact` через существующий ASC. Подсказка находится в `/Game/Gameplay/UI/WBP_InteractionPrompt`; она уже подключена к Runtime HFSM. При пустых руках оружие автоматически экипируется; иначе добавляется в сумку.

**I → Выбросить** отправляет серверу запрос на выбрасывание всей строки инвентаря. Предмет появляется у земли впереди персонажа, сохраняя количество и stat tags, включая патроны. Если размещение невозможно, предмет остаётся в инвентаре. Физического броска пока нет.

## Интерактивный объект

Для нового интерактивного объекта создать Blueprint от `ANGInteractableActor` и реализовать события NGInteractable. Игровые состояния и их репликацию задавать под конкретную задачу; готового native-переключателя в системе нет.

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
# Распределение логики взаимодействия — 2026-11-07

Interactor во всём interaction API — `APawn*`: интерфейс, GA_Interact, native цели и RPC больше не требуют ANextgenoneCharacter. Компоненты получаются через FindComponentByClass, ASC — через существующий NGPlayerContext/IAbilitySystemInterface. Проверки сохраняют текущий контракт игрока: взаимное possession Controller/Pawn/PlayerState, актуальный Avatar ASC, положительный Health из HealthSet, отсутствие State.Dead и InterruptingTags. Отдельный Pawn без GAS/HealthSet этим изменением автоматически не разрешается к взаимодействию. Для прерывания открытой сессии его lifecycle должен вызывать BindAbilitySystem так же, как существующий персонаж.

AimComponent опционален для поиска: при наличии берётся его view ray, иначе viewpoint Controller. Сервер самостоятельно проверяет eye/control rotation, дистанцию и LOS. Цвет прицела также читает InteractionComponent без cast к проектному Character. PlayerInventory RPC/сессия работают с APawn; drop использует направление гравитации CharacterMovement при наличии, иначе up-вектор Pawn.

После пользовательской полной сборки и перезапуска Editor обновить узлы Blueprint-реализаций интерфейса (GetInteractionPrompt, CanInteract, Interact, SetInteractionFocus): параметр Interactor теперь Pawn. Если графу нужны конкретные функции игрового персонажа, явный cast остаётся в этом пользовательском графе. Ассеты автоматически не пересохранялись.

- `UNGInteractionComponent` хранит фокус/контекст/прерывающие теги и выполняет FindTarget, ValidateTarget и IsCurrentInteractor относительно своего Pawn. Дистанция и CaptureRadius берутся из этого компонента.
- `UNGInteractionAbility` задаёт порядок проверки цели и CanInteract, GAS Commit и повторной проверки после callbacks. Только после этого вызывается Interact.
- `UNGInteractionSubsystem` удалён. Общая база целей `ANGInteractableActor` хранит только локальную защиту выполнения и текущего исполнителя. ExecuteInteraction блокирует вложенный запуск этой же цели на время проверок, Commit и Execute_Interact, в том числе через другого игрока. Исполнитель устанавливается только после успешной подготовки и очищается при выходе. Глобальных списков и нового subsystem нет.
- `ANGItemPickup::Interact_Implementation` добавляет предмет через Inventory, затем расходует pickup и публикует изменение. Локальный bTransferring защищает тот же pickup от повторного входа; отдельно сохраняется общий guard цели. Прежнее правило выбора группы Action/Storage не изменено.
- `UNGPlayerInventoryManagerComponent::DropItemOnServer` владеет drop: сохраняет snapshot, создаёт скрытый pickup, повторно проверяет текущий Pawn/Count/StatStacks, списывает предмет и только затем публикует pickup. ActiveDrops относится к предметам этого инвентаря.

Серверные проверки переноса/доступа к коробке используют InteractionComponent, а открытие коробки требует подтверждённый execution scope самой цели. Rate limits остаются у прежних постоянных владельцев.

ANGItemPickup и ANGInventoryBox наследуют ANGInteractableActor; их BP-наследников вручную переподчинять не требуется. Новые пользовательские цели должны также наследовать ANGInteractableActor: одного добавления интерфейса к произвольному AActor теперь недостаточно для защищённого исполнения.

Старые Blueprint-вызовы subsystem FindTarget/DropInventoryItem больше не являются API. Gameplay-взаимодействие идёт через InteractionComponent.RequestInteract и GA_Interact; inventory drop — через существующий UI/player inventory request. C++-вызовы перенесены. Ассеты и Blueprint-графы этим пакетом не изменялись; наличие старых узлов или произвольных Actor, реализующих только интерфейс, в пользовательских графах не проверено. После полной сборки нужен перезапуск Editor из-за изменения native-иерархии.

Сборка, PIE и сетевые проверки не выполнялись. Нужны адресные проверки pickup/drop (включая callbacks и два игрока на одной цели), открытия/закрытия коробки, transfer, LOS/range, прерывающих тегов и смены Pawn. Этот рефакторинг не считается исправлением незакрытого бага холодного первого переноса.
