# FSM персонажа и ресурсы состояний

Актуализация lifecycle: UHFSMStateComponent и все наследники используют **void Enter/Exit**, как UHFSMState. Общий отказ/откат входа, AbortEnter, OnEnterFailed, HasEnteredSuccessfully и отдельный ActiveComponents удалены по решению пользователя. State вызывает все настроенные компоненты; каждый проверяет свои данные, освобождает собственные частичные ресурсы и делает return при ошибке. Ошибка одного компонента не откатывает остальные. Внутренние GAS validation/grant/handle проверки и барьер готовности ресурсов для смены Avatar сохранены. Blueprint пользователь настраивает сам: удалить старые узлы OnEnterFailed/HasEnteredSuccessfully и ненужный переход ResourceFailure, если он обслуживал только общий rollback. Более ранние описания rollback ниже исторические.

Актуальный контракт — 2026-11-03. Персонаж не получает базовых способностей при создании/possession. Их единственный источник — активные UNGControlStateDefinition. UNGAbilitySet и DA_PlayerAbilities удалены. CharacterDefinition содержит InputConfig, без базовых GAS grants.

## Граф и владельцы

Граф находится в BP_NextgenoneCharacter:EventGraph: Start **FSM** → Branch IsDead → Alive / Dead; Alive → Dead по **State.Dead**; ResourceFailure → Stop. На Pawn один native CharacterStateMachine (GCFSM replication component), MachineName=FSM, DeathEvent=State.Dead, deferProxyInitialization=true. Отдельный Launch на BeginPlay не нужен: C++ ждёт BeginPlay и готовый ASC. GCFSM replicated context требует Actor, поэтому граф не переносится в ActorComponent.

ASC остаётся единственным на PlayerState. PlayerState.StartLife задаёт OwnerActor/AvatarActor и инициализирует здоровье один раз на новый Pawn. Init-health не является ресурсом Alive и не повторяется при входах в состояние. Второго ASC/здоровья нет.

## Конфиг каждого состояния

FSM_CharacterAlive и FSM_CharacterDead — существующие UHFSMState Blueprint. В Components каждого находится **Character State Resources** (UNGCharacterStateResources), с соответствующим UNGControlStateDefinition:

- **StateTag** — идентичность состояния на Pawn, сохраняющаяся после отвязки ASC.
- **PresentationCue** — необязательный локальный GameplayCue; для Dead — NGDeathRagdollCue.
- **Abilities: TArray<FNGAbilityGrant>** — Ability, InputTag, Level, bActivateOnEnter.
- **Effects** — временные Duration/Infinite GE, StackingType=None. Instant не допускается: его нельзя снять при выходе.
- **MappingContexts** — gameplay IMC и приоритеты, принадлежащие только состоянию.
- **bBlockGameplayInput** — политика через существующий UPlayerInputSubsystem. UI IA/IMC остаются отдельными.

Enter на сервере проверяет конфиг/текущий Avatar, выдаёт все способности, применяет эффекты, сохраняет handles и регистрирует scope. Затем запускает grants с bActivateOnEnter=true. Ошибка приводит к cleanup и существующему OnEnterFailed/ResourceFailure. Клиенты не выдают GAS grants; owning client подключает IMC/input policy. Exit/Abort/Unbind снимают только handles этого состояния. Перекрытие классов/input tags с другим активным scope (родительское состояние, оборудование) считается ошибкой, а не второй копией способности.

Refresh после possession восстанавливает ресурсы активного состояния. После UnPossess обычное состояние не выдаёт их повторно без Controller; Dead может получить ресурсы непосредственно при Enter без Controller. Cleanup старого Pawn не снимает ресурсы нового Avatar. При отмене способностей handles предварительно извлекаются из scope для защиты от повторного cleanup из callbacks.

## Alive и Dead

DA_CharacterState_Alive получает прежние игровые grants: Jump, Dash, Mantle, Interact, Punch с сохранёнными классами/тегами/уровнями. bActivateOnEnter=false: они ждут ввода. При выходе из Alive эти grants снимаются. Equipment Fire/Reload сохраняют прежний scope Combat; он выдаёт прямые FNGAbilityGrant без промежуточного AbilitySet.

DA_CharacterState_Dead содержит NGDeathAbility, InputTag пустой, Level=1, **bActivateOnEnter=true**, bBlockGameplayInput=true. Отдельный NGCharacterDeathStateComponent удалён: смерть использует ту же выдачу и активацию, что любое другое состояние.

Поток: нулевой HP → запрос перехода FSM в Dead и блокировка действий → общий resource adapter выдаёт GA_Death, применяет NGDeadStateEffect (Infinite, тег State.Dead) → активирует GA_Death → HandleDeath → PresentationCue → обычный ALS RagdollStart. Alive.StateTag=State.Alive, Dead.StateTag=State.Dead. Прямой активации/GAS-события из HealthChanged нет. GA_Death проверяет принадлежность spec текущему scope/Avatar и наличие тега GE.

Character больше не хранит bDead, bDeathAbilityActivated, bDeathRagdollStarted и методы протокола активации смерти. IsDead читает состояние его FSM, поэтому старый труп не зависит от ASC новой жизни. На текущем ASC State.Dead принадлежит эффекту Dead и снимается через его handle. GA_Death хранит исходный Avatar на время активации; cue отвергает чужой SourceObject и использует базовый AALSBaseCharacter. Способность принадлежит Dead до Exit/Unbind, её окончание не снимает grants.

В этом пакете presentation немедленный: proxy восстанавливает cue из definition при входе в Dead/BeginPlay, даже без PlayerState. Отложенный montage в HandleDeath сам по себе не синхронизирует момент перехода в регдолл: для этого позже нужны отдельные фазы FSM, например Dying → Dead. Новые фазы здесь не добавлены.

GCFSM обрабатывает события через очередь. При раннем UnPossess серверный ASC сохраняется до готовности ресурсов перехода: выдача, эффекты, вызовы всех bActivateOnEnter. Общий CanReleaseAvatar/OnResourcesReady заменяет death-specific callbacks. StartLife нового Pawn ждёт в PendingLifePawn; затем старые handles снимаются до смены Avatar. Нет polling/timer. До BeginPlay используется BoundASC, даже если PlayerState уже отсоединён. Destroyed EndPlay снимает ожидание. Ненастроенный/остановленный граф или ошибочный auto-activate grant может оставить следующего Pawn в ожидании — fallback в обход FSM отсутствует.

## Косметический регдолл

Реплицируются состояние GCFSM и GAS-тег при привязанном ASC; отдельных death/phase флагов Pawn нет. Положение/скорость/поза тела не синхронизируются. Каждый клиент рассчитывает падение локально. Меш трупа отделяется от capsule; Actor остаётся у места смерти. Для dead Tick обходятся ALS pelvis RPC/remote correction, сохраняется ACharacter::Tick. ALS get-up мёртвого Pawn запрещён. Late join восстанавливает presentation из FSM, а не финальную позицию тела. Точный лут из тела, resurrection, custom-gravity corpse physics и оптимизация множества трупов не входят в пакет. ALS plugin/AnimBP не изменялись.

## Миграция и проверка

Снимок исходных grants и бэкап ассетов: Saved/StateGrantMigration/snapshot.json и Before/. Старые ссылки очищены перед удалением native-типа. Старый setup commandlet теперь редактирует Alive definition и не восстанавливает удалённый DA_PlayerAbilities. Конфиги input/UI сохранены.

C++ пакет собран NEXTGENONEEditor Win64 Development — PASS, UBT exit 0 (Saved/Logs/AgentBuild.log). Новые Abilities заполнены в обоих definition через MCP, InputConfig сохранён. Alive/Dead/Pawn Blueprint скомпилированы; ассеты сохранены и прочитаны обратно. Dead содержит только общий resource adapter. PIE, сетевые и игровые тесты агентом не запускались. Отдельного апрува ждут: повторный вход/выход Alive, отмена held input, откат ошибки Enter, Dead auto-activation, ранний UnPossess/новый Pawn, смерть до BeginPlay, late join/порядок репликации и сохранность здоровья новой жизни.

## IMC принадлежат состоянию — 2026-11-03

IMC_NG_Character (priority 10) перенесён из DA_Input_Character.MappingContexts в DA_CharacterState_Alive.MappingContexts. MappingContexts и StateMappingContexts из InputConfig удалены; каталог-дубль не используется. IA→Native/GAS tag bindings по-прежнему регистрируются один раз Pawn input-компонентом. Alive приобретает lease IMC через существующий resource adapter/UPlayerInputSubsystem; Exit освобождает его. Dead не подключает gameplay IMC. UI FSM сохраняет отдельное владение UI IMC и policy. Вложенные состояния снимают только собственные handles; если родитель держит gameplay IMC, дочернее состояние не удаляет чужой lease.

Изменены только два Data Assets и документация; C++ не менялся, новая сборка не требовалась. Ассеты сохранены и прочитаны обратно через MCP. Бэкап: Saved/StateInputMigration/20261103-192636/. Игровая проверка ввода/переходов не запускалась.

### InputConfig без IMC — 2026-11-03

Завершение текущего пакета: офлайн NEXTGENONEEditor Win64 Development PASS (UBT exit 0). NGSetup -StateLifecycleOnly завершился exit 0, 0 errors / 11 предупреждений ALS/окружения; сохранил StateTag/Cue/DeadStateEffect, скомпилировал и сохранил Alive/Dead/Pawn Blueprint. InputConfig принудительно пересохранён с сохранением 13 native и 3 GAS маршрутов. Логи: Saved/Logs/AgentBuild.log и DeathLifecycleAssets.log. Бэкап: Saved/DeathLifecycleRefactor/20261103-200722/. Игровые/сетевые тесты не запускались.

InputConfig теперь содержит только NativeInputActions/AbilityInputActions и поиск IA по тегу. Структура FNGInputMappingContextEntry находится рядом с ControlStateDefinition (тот же модуль/UScript-путь, без redirects). Input adapter только подписывается на существующую UI/gameplay policy, не приобретает IMC leases. ValidateInputMappings на definition проверяет действия его IMC против маршрутов текущего Pawn при Enter; отсутствие маршрута вызывает ошибку входа и штатный rollback. Отдельная asset-валидация InputConfig не ищет IMC и продолжает проверять маршруты/типы/дубликаты. C++ сборка PASS; runtime-проверка переходов не выполнялась. Бэкап до очистки: Saved/InputConfigCleanup/20261103-193514/.
