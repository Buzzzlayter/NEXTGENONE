# NEXTGENONE — проектная памятка для Codex

UE **5.8** C++ проект.

## Структура
- Editor: HFSMComponentCustomization ?????????? IPropertyTypeCustomization ??? UHFSMStateComponent: ??????????? inline selector + ???? ? ????? ??????. CustomizeChildren ???????? ???????? instanced-??????? ????? EnumerateConstRawData ? ????????? ?? editable-???? ????? AddExternalObjectProperty (?? GetNumChildren ? ?? CDO). Global row extension ?????: ???????????? ?? ????? ??????. Blueprint ?????????? ? Content Browser, native-????? ??????????? ????? SourceCodeNavigation. ??? Source/UIModule/Private/Editor/, ?????? WITH_EDITOR; UnrealEd/PropertyEditor ?????? Target.bBuildEditor. ???????????? ?????????? ?????? ???? ? ?????????????? instanced-?????. ????? ????????????? ?????? ???????? ??????????? ????????? (??????????? delegates ?????? ????????? lambda); ???? ??????????? ?? ?????????/?? ?????????? ???????.
- Combat presentation больше не опрашивается в Character.Tick. Обновления — Equipment/ADS events, OnOverlayStateChanged после ALS demo-held-object, конец BeginPlay, RagdollEnd после Super, project mantle TimelineFinished после оригинального MantleEnd и mantle cancel. Проверка IsDead в Tick сохранена для обхода сетевого ALS RagdollUpdate. ALS plugin не изменён; сборка/runtime событийного восстановления ещё не проверены.
- Встроенный Combat.TestWeapon и куб из конструктора Character удалены. Equip не создаёт предметы: CurrentItem должен быть выбран через слот/RequestEquipItem и присутствовать в инвентаре. StarterItems на Combat задаётся пользователем, native список пуст. WeaponDefinition не содержит зашитых /Game-путей к Fire/Reload/ID_TestWeapon; пустые Fire/Reload используют существующие native fallback классы, Projectile/Aim имеют native defaults. Не возвращать автоматическую выдачу тестового оружия.
- HitReaction.ApplyImpact общий для пули/кулака. Смертельный импульс reliable multicast сохраняется в компоненте того же Pawn до ALS-регдолла и потребляется ReleaseToRagdoll после отделения меша; при уже активном регдолле применяется сразу. DeathImpulseMultiplier/DeathPushSpeed на HitReaction, BodyImpactImpulse отдельно у WeaponDefinition и GA_Punch. Живой knockback кулака сохранён. Поза тела не синхронизируется, late join/relevancy не восстанавливают импульс. Сборка/визуальный тюнинг не проверены; Docs/Gameplay/BulletBodyImpact.md.
- Прыжок: только AbilityInputActions Input.Jump → GA_Jump (Alive grant). Native Jump-маршрут и override Character.JumpAction_Implementation удалены; bJumpInputHeld отсутствует. GA сохраняет подъём из приседа/живого ragdoll на одном нажатии (ragdoll get-up только с authority через ALS multicast). Старые Blueprint-вызовы ALS JumpAction пользователь удаляет вручную, InputConfig переносит из Native в Ability. Сборка/runtime переноса не проверены.
- ADS: UNGAimAbility (LocalPredicted, источник — Combat equipment) выдаётся оружием вместе с Fire/Reload. Способность удерживает Infinite UNGAimingEffect с State.Weapon.Aiming до WaitInputRelease/отмены; GE реплицирует тег и simulated proxies. Combat.IsAiming читает тег текущего Avatar, BindAbilitySystem подписывает ALS/UI на изменения. bAiming/ServerSetAiming удалены. RequestAim — только совместимый input→GAS переходник для старого Native Input.Aim; предпочтительный конфиг — AbilityInputActions Input.Aim (только один маршрут). Новые GE/GA в Gameplay/Abilities/NGAimAbility.*; AimAbilityClass задаётся в WeaponDefinition, в Alive их повторно не выдавать. Разброс не менялся. Сборка/сеть переноса не проверены.
- Aim: общий GetTargetingOrigin проецирует начало camera ray вперёд к Pawn (подход Lyra); клиентский aim trace и серверный shot trace используют его одинаково. Сервер проверяет также sphere sweep по ALS GetThirdPersonTraceParams. Точного восстановления ALS Camera AnimBP на сервере нет; исходный plugin не изменён, клиентский origin остаётся ограниченно доверенным. Детали/ограничения: Docs/Gameplay/BallisticsAndAmmo.md.
- Стрельба игрока: Combat.RequestFire передаёт луч NGAimComponent.GetViewRay и revision экипировки через ServerRequestFire. Сервер валидирует луч и синхронно запускает ServerOnly GA_Fire; RequestedShotView действует только внутри этой активации. Для remote Pawn FireOnServer без свежего луча отклоняется, latent-задержки GA_Fire не поддержаны. Цель — серверный trace камеры, старт снаряда пока прежний якорь капсулы. Лимиты камеры на AimComponent; Docs/Gameplay/BallisticsAndAmmo.md. Сборка/runtime этого пакета не проверены.
- UI прицела и интеракции разделены: UNGReticleWidget (BindWidget Image AimDot) и UNGInteractionPrompt (BindWidget TextBlock InteractionText). Оба компактные, дерево/размер/позицию задаёт пользователь в UMG/layout, монтируются обычным UUIWidgetMounterComponent. UNGInteractionPromptMounter удалён. Настройка: Docs/Gameplay/Interaction.md.
- `Source/NEXTGENONE/Gameplay/` разбит по подсистемам: Character, Player, GameModes, Abilities, AbilitySystem, Combat, Interaction, UI, Networking, Editor; существующие Input, Inventory, StateMachine сохранены. Include-пути проектных классов — от корня модуля (`Gameplay/Combat/NGCombat.h` и т.п.). Editor содержит authoring/commandlet-файлы в том же модуле, не отдельный Editor-модуль.
- FSM персонажа: native CharacterStateMachine (GCFSM replication component) на Pawn; граф должен находиться в Pawn Blueprint, потому что GCFSM replicated context требует Actor. Core StateDefinition в GameplayFoundation, HFSM resource adapter в NEXTGENONE/Gameplay/StateMachine. Сетап: `Docs/Gameplay/CharacterStateMachine.md`. MachineName по умолчанию пуст — без Blueprint-сетапа машина не запущена. Здоровье фиксирует смерть, FSM Dead через общий Character State Resources выдаёт и активирует NGDeathAbility из UNGControlStateDefinition.Abilities (bActivateOnEnter); отдельный death-state компонент и UNGAbilitySet удалены; способность включает обычный локальный ALS-регдолл. По решению пользователя поза/движение тела не синхронизируются; сетевой компонент якоря удалён. Character не хранит death/phase флаги и протокол GA_Death: IsDead читает FSM StateTag; DeadStateEffect выдаёт GAS State.Dead, GA_Death вызывает PresentationCue обычного ALS-регдолла. Proxies восстанавливают cue из FSM даже после отвязки PlayerState. ASC удерживается при раннем UnPossess до общей готовности ресурсов перехода (CanReleaseAvatar/OnResourcesReady); StartLife следующего Pawn возобновляется через PendingLifePawn. Presentation немедленный; отложенные montage требуют отдельных фаз FSM. GA_Death находится только в Dead definition, без InputTag; runtime/сеть пакета ещё не проверены.
- Ввод персонажа — GameplayInputComponent на Pawn (общий в GameplayFoundation, ALS/Combat адаптер в NEXTGENONE). Controller не подключает старые ALS/Combat bindings. Перед игровым запуском обязательно назначить CharacterDefinition/InputConfig и UI IA/IMC по `Docs/Gameplay/CharacterInputSetup.md`. Context/policy handles проходят через существующий UPlayerInputSubsystem; UI инвентаря/настроек блокирует Gameplay. CharacterDefinition содержит InputConfig, без базовых GAS grants. UNGControlStateDefinition содержит Abilities (FNGAbilityGrant), Effects и input policy; общий resource adapter выдаёт/снимает их по Enter/Exit/Unbind. DA_PlayerAbilities удалён; обычные способности находятся в Alive, GA_Death — в Dead с bActivateOnEnter. По умолчанию Pawn без способностей. Gameplay IMC включает состояние через ControlStateDefinition.MappingContexts: IMC_NG_Character назначен Alive (priority 10); Из UNGGameplayInputConfig полностью удалены MappingContexts/StateMappingContexts; он хранит только IA→тег. Связь IMC с маршрутами Pawn проверяет ControlStateDefinition.ValidateInputMappings при входе состояния. UI IMC остаются у UI FSM.
- Mantle: GA_Mantle — только ServerOnly GAS-обёртка (Commit/start/end/cancel). Единственный executor — BP_ALSMantleExecutor на Pawn, наследник UNGALSMantleExecutor : UALSMantleComponent. Геометрия/движение/Timeline/монтаж/кривые и выбор overlay исполняются исходным ALS; проектный наследник содержит только GAS/network bridge и запрос автозахвата. MantleAdapter и самостоятельный Root Motion Source/cue удалены. Настройки находятся в BP_ALSMantleExecutor, не в GA_Mantle. Плагин/AnimBP не менялись. Сетап и ограничения: Docs/Gameplay/MantleAbility.md; runtime/сеть восстановления ещё не проверены.
- HFSM lifecycle: UHFSMStateComponent и все наследники — void Enter/Exit, без общего отказа/rollback и AbortEnter. State вызывает весь Components, проверки и частичная очистка локальны в компонентах. OnEnterFailed/HasEnteredSuccessfully/ActiveComponents удалены; старые Blueprint-узлы пользователь убирает вручную. GAS handles/валидация/ресурсная готовность для ASC handoff сохранены.
- HFSM: UHFSMPlayerStateComponent удалён; UI/layout/inventory наследуют общий UHFSMStateComponent. GetPlayerController/GetLocalPlayer доступны у State и StateComponent и разрешают игрока из контекста владельца без first-player fallback. Blueprint input events автоматически подключаются на Enter и снимаются на Exit/Abort; каждый Blueprint-приёмник имеет изолированный transient EnhancedInputComponent в стеке своего локального Controller. Привязка однократная на Enter, снятие на Exit/Abort; без bHasBlueprintInputEvents и refresh в Tick. После появления/смены Controller нужен повторный вход. IMC/policy по-прежнему принадлежат UPlayerInputSubsystem. Точный ClassRedirect старого промежуточного типа сохранён. Сборка/авторинг прошли; runtime не проверен. Контракт: Docs/Gameplay/CharacterInputSetup.md.
- Общие GAS-классы и конфиги персонажа/ввода — `Source/GameplayFoundation/` (Runtime). `NEXTGENONE` зависит от него; обратных зависимостей на NEXTGENONE/ALS/UI нет. Единственный ASC на PlayerState — проектный `UNGPlayerAbilitySystemComponent`, наследник общего `UNGAbilitySystemComponent`. Детали: `Docs/Architecture/GameplayFoundationModule.md`.
- Модули проекта: основной `NEXTGENONE` (Runtime) — `Source/NEXTGENONE/`; UI — `Source/UIModule/`. Шаблонный модуль `ALSv4_Community` и `Content/ThirdPerson` полностью удалены. Реальная ALS-интеграция находится в плагине `Plugins/ALSV4_CPP` и сохранена.
- Единая игровая цепочка: `ANextgenoneGameMode`, `ANextgenoneCharacter`, `ANextgenonePlayerController`, `ANextgenonePlayerState` в `Source/NEXTGENONE/Gameplay/`. `ANGCombatTestGameMode` наследует основной режим и добавляет только тестовый harness.
- Перенесённое ядро инвентаря Lyra — `Source/NEXTGENONE/Gameplay/Inventory/`, владелец — существующий PlayerState. Боезапас хранится в stat tags item instance; текущая экипировка остаётся в NGCombatComponent. Второй ASC/Character/инвентарь не добавлять. Инвентарь UI: UNGInventoryStateComponent наследует UUIWidgetMounterComponent и использует его WidgetClass/PlaceholderID/Layer и lifecycle монтажа. CloseEvent (FName) запрашивает переход FSM; окно демонтируется базовым мounter при Exit/Abort. Отдельный InventoryWindowMounter удалён; проектный Controller и input-системы компонент не использует. Blueprint placement/input/policy/focus настраивает пользователь (старый инстанс из Runtime убрать); provenance и ограничения — `Docs/Research/LyraPort.md`.
- Рабочая карта проекта: `/Game/Maps/Gameplay`, Blueprint режима: `/Game/Blueprints/BP_NextgenoneGameMode`. Отдельная combat-карта удалена переносом в Gameplay; тестовый runner выбирает свой режим через URL override.
- Voxel удалён: нет `NgxVoxel`, старого voxel Pawn, voxel-ассетов и `DevLevel`. Не восстанавливать эту подсистему без поручения пользователя.
- Таргеты: `Source/NEXTGENONE.Target.cs`, `Source/NEXTGENONEEditor.Target.cs`.
- Включённые плагины (`NEXTGENONE.uproject`): `ModelContextProtocol`, `Terminal`,
  `EditorToolset`, `ModelingToolsEditorMode`.

- NGSettingsAssets.cpp удалён. NGSetupCommandlet оставлен только как явный dispatcher -ImportWeaponVFX / -WeaponVFXOnly. Одноразовые setup/migration флаги (SettingsOnly, RemoveALSMenu, Consolidate, StateLifecycleOnly, MantleAbilityOnly и создание gameplay assets) удалены; исторические команды в Docs не повторять. Blueprint пользователь настраивает сам.

## Движок и сборка
- По указанию пользователя от 2026-11-04 сборку выполняет пользователь. Агент не запускает UBT/Live Coding без отдельного запроса.
- Движок UE **5.8**, установлен в `S:\UE_5.8` (резолвится из реестра по EngineAssociation).
- **Тулчейн — **14.50.35717**.

### Батники в корне (self-contained, движок берут из реестра)
- `Build.bat` — собрать `NEXTGENONEEditor Win64 Development`.
- `RunEditor.bat` — запустить редактор с проектом.
- `GenerateProjectFiles.bat` — пересоздать .sln/проектные файлы.
- `DeepClean.bat` — удалить `Binaries`/`Intermediate`/`DerivedDataCache`/`Saved` (+ `Binaries`/`Intermediate`
  в плагинах) и `*.sln`/`*.slnx` в корне. `Content`/`Config`/`Source` не трогает.
  Откажется работать при открытом редакторе, спрашивает подтверждение. После — `GenerateProjectFiles.bat`.

## Верификация для агента (компиляция C++ + логи)
Цель: агент видит **реальный** вывод компилятора и логи редактора текстом, а не угадывает API.
Оформлено двумя скиллами; скрипты лежат рядом с ними в `.Codex/skills/ue-build/`.

### Скилл `ue-build` — компиляция C++ → текст
- `Verify-Build.ps1` — полная офлайн-сборка UBT (редактор **закрыт**). Печатает `RESULT` +
  ошибки `файл(строка): error Cxxxx` + warnings; полный вывод в `Saved/Logs/AgentBuild.log`.
  Запуск: `powershell -NoProfile -ExecutionPolicy Bypass -File .Codex/skills/ue-build/Verify-Build.ps1`
  (`-Game` — game-таргет; `-Config Debug|DebugGame|Shipping`).
  **Exit:** `0`=PASS (код компилируется; сюда же link-lock при открытом редакторе),
  `3`=BLOCKED (Live Coding активен, **ничего не собралось** — не ошибка кода), иное=реальная ошибка.
- `Trigger-LiveCoding.ps1` — при **открытом** редакторе шлёт Ctrl+Alt+F11 (best-effort, SendKeys,
  крадёт фокус). Результат компиляции читается из лога (см. ниже). UBT при активном Live Coding
  собирать отказывается (`Unable to build while Live Coding is active`).
- В MCP **нет** инструмента «выполнить консольную команду» → headless-запуск самой компиляции
  невозможен; компиляцию инициирует UBT (редактор закрыт) либо Ctrl+Alt+F11 (редактор открыт).

### Скилл `ue-logs` — логи редактора → текст (редактор открыт, через MCP)
- Тулсет `EditorToolset.LogsToolset`, вызов `GetLogEntries`.
  **ВАЖНО:** всегда передавай `category:""` для поиска по всем категориям — дефолт `"LogsToolset"`
  это несуществующая категория и даёт ошибку.
- Ошибки/warnings/ассерты: `{category:"", pattern:"(?i)error|warning|fatal|assert", maxEntries:50}`.
- Результат Live Coding: `{category:"LogLiveCoding", pattern:"", maxEntries:30}`.
- Список категорий: `GetLogCategories {filter:"..."}`; детальность: `SetVerbosity {category, verbosity}`.
- Редактор закрыт (MCP недоступен) → лог прошлой сессии `Saved/Logs/NEXTGENONE.log` (Read/Grep).

## Интеграция Codex ↔ UE (главное)
Связь идёт через плагин `ModelContextProtocol` — Epic-native **MCP по HTTP на
`127.0.0.1:8000/mcp`**, авто-старт включён, сервер зарегистрирован как `unreal-editor`.
- Управление редактором — через навык `ue` (discovery → `call_tool`); `mcp__unreal-editor`
  уже в allow-листе, так что вызовы идут без подтверждений.
- **MCP отвечает только когда редактор запущен** (порт 8000 слушает). Если закрыт —
  сначала `RunEditor.bat`.
- Можно просить вживую: ставить/двигать актёры, править блюпринты/материалы/Niagara,
  PCG-графы, запускать PIE, читать лог редактора (`Saved/Logs/NEXTGENONE.log`).

## Git и бэкап (важно)
- Remote: `github.com/Buzzzlayter/NEXTGENONE`, ветка `main` (солопроект, коммиты прямо в main).
- Бинарники — через **Git LFS** (`.gitattributes`): `.uasset`, `.umap`, `.fbx`, текстуры и т.д.
- `.Codex/settings.local.json` — личные права (allow/deny/ask), в `.gitignore`, НЕ бэкапится.
- В коммитах отключена приписка Клода (`attribution.commit: ""`) — автор только пользователь.

## Апрув перед тестированием (правило пользователя, 2026-11-01)
- Не начинать этап тестирования без отдельного явного апрува пользователя на этот этап.
- Это относится к автотестам, PIE, сетевым проверкам, smoke/regression и прогонам с latency/packet loss.
- После реализации сообщить о готовности к тестированию и ждать апрува. Общее поручение реализовать функциональность не заменяет отдельный апрув на тестовый этап.
- Не расширять и не повторять тестовые прогоны инициативно за пределами согласованного объёма.

## Экономная работа и консолидация (правило пользователя, 2026-11-01)
- Экономить токены, вычисления и время пользователя. Брать одну ограниченную задачу с конкретным результатом; новые идеи фиксировать отдельно, не расширять реализацию инициативно.
- Для кода сначала использовать локальные файлы/CLI. MCP применять адресно для редакторских задач; сохранять найденные схемы инструментов и возвращать только нужные поля, без повторных больших discovery/schema dumps.
- Собирать согласованный пакет C++-изменений после статического просмотра. Не запускать сборку после каждой мелкой правки; повторять её при ошибке или последующих необходимых изменениях C++. Не повторять успешные проверки без новой причины.
- Заранее согласовать одно окно закрытого редактора на пакет работ. Не запускать редактор, серверы или длительные командлеты без необходимости. Не путать сборку/авторинг с доказанной работой в игре.
- Тестирование — исключительно по отдельному апруву и в согласованном объёме. Обычный игровой запуск пользователя не является разрешением агенту на тесты.
- Пока приоритет — консолидация существующей основы: владельцы состояния, конфигурация, зависимости, устранение дублей. Не переносить новые подсистемы из Lyra по собственной инициативе.
- Избегать новых жёстко заданных /Game-путей, клавиш и чисел в runtime. Пользовательские настройки держать в существующих definition/config/input assets; одноразовые миграции отделять от игрового кода. Не создавать новую универсальную framework-систему ради рефакторинга.
- Пользователь быстрее выполняет визуальную работу в Blueprint/UMG/анимациях. Давать короткий контракт: класс/ассет, нужные поля/события, ожидаемый результат. Агент отвечает за C++, GAS, сеть и серверные инварианты.
- Итог краткий: что изменено, что реально проверено, что осталось. Производительность MCP/игры не объявлять установленной причиной тормозов без замеров.
