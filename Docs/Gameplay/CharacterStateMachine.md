# FSM персонажа и временные ресурсы

## Создано через MCP — 2026-11-03

Дополнение: после пользовательского запуска лог сообщил `assign a valid GameplayInput.CharacterDefinition`. Через MCP созданы /Game/Gameplay/Definitions/DA_Character_Default и DA_Input_Character, Definition назначен на GameplayInput Pawn, его AbilitySets ссылается на существующий DA_PlayerAbilities. Затем InputConfig заполнен Gameplay actions/mappings для клавиатуры и геймпада, UI получил отдельные contexts; подробности и раскладка в CharacterInputSetup.md. Повторный запуск игры агентом не выполнялся. Дополнительный Blueprint Launch на BeginPlay не добавлялся: штатный C++ launch ожидает успешную инициализацию GAS.

В BP_NextgenoneCharacter:EventGraph добавлена GCFSM entry **FSM** (это фактическое имя стартового узла, MachineName также FSM). Branch по IsDead сразу выбирает Dead для уже мёртвого Pawn, иначе Alive. Alive → Dead через событие **State.Dead**; использован уже зарегистрированный native tag, новый словарь тегов не требуется. Ошибка входа в любой из состояний посылает ResourceFailure и ведёт в FSM Stop.

Созданы в /Game/Gameplay/States:

- FSM_CharacterAlive и FSM_CharacterDead — наследники UHFSMState с Character State Resources и обработкой OnEnterFailed.
- DA_CharacterState_Alive и DA_CharacterState_Dead — пустые временные GAS scopes; Dead блокирует Gameplay input и не требует ASC.

CharacterStateMachine настроен: MachineName=FSM, DeathEvent=State.Dead, deferProxyInitialization=true. Все три Blueprint скомпилированы и пять ассетов сохранены через MCP. Проверены фактические соединения пинов (оба входа в Dead, оба входа в Stop), ссылки компонентов на Definition и сохранность прежних ALS held-object веток. DSL-чтение MCP не отображает повторное использование shared-узлов полностью, поэтому оно не использовалось как единственное доказательство связей.

Оригинальный BP перед изменением сохранён и скопирован в Saved/FSMGraphBackup/20261103-Character/BP_NextgenoneCharacter.uasset. Plugin ALS/AnimBP и C++ не менялись. PIE/сетевые/игровые тесты не запускались.

На первоначальный момент авторинга GameplayInput.CharacterDefinition не был назначен; это исправлено дополнением выше. Раскладка InputConfig теперь заполнена по CharacterInputSetup.md. Сам граф уже создан — повторять нижеприведённые шаги создания существующих ассетов не нужно; они остаются контрактом для новых Pawn.

2026-11-02. Первоначально реализован C++ каркас. Последующий Blueprint-сетап описан выше. Пока CharacterStateMachine.MachineName не задан, новый граф не запускается. CharacterDefinition/base GAS и UI FSM работают отдельно.

## Где находится граф

**В BP_NextgenoneCharacter (Pawn Actor), не в ActorComponent.** Исходники GCFSM проверены: Replicated MakeFSM требует Actor context и UGCFSMReplicationComponent на этом Actor; восстановление графа на proxy ищет entry point на Actor. Компонентный graph context без модификации плагина сюда не подходит.

На Character добавлен native **CharacterStateMachine**, производный от UGCFSMReplicationComponent. Не добавлять второй обычный GCFSMReplicationComponent. Компонент запускает граф на сервере после готовности GAS и BeginPlay, обновляет локальные ресурсы по lifecycle, при EndPlay освобождает ресурсы и останавливает ticker через pause. Вызывать UGCFSM.Stop вручную для активной машины нельзя: в плагине это assert, поскольку Stop ожидает уже завершённый state.

Используется штатная репликация GCFSM: сервер хранит актуальные GUID активных FSM; начальное состояние приходит через COND_InitialOnly, дальнейшие переходы — reliable multicast. Клиент не выбирает игровые переходы. Машина принадлежит Pawn и не останавливается при UnPossess. Simulated proxy не ожидает чужого Controller или LocalPlayer. Новый код не модифицирует GCFSM/ALS плагины.

## Минимальный Blueprint-сетап

1. На CharacterStateMachine задать MachineName, например **Character**, и DeathEvent, например **Character.Event.Death**. Тег добавить в существующий словарь Gameplay Tags.
2. В Blueprint Pawn создать GCFSM graph с Start тем же именем **Character**. Машину запускает C++; не добавлять второй Launch из Event BeginPlay. На Start/вложенных машинах использовать Replicated по настройкам узлов GCFSM.
3. Создать два Blueprint состояния на **UHFSMState**: Alive и Dead. Подключить Alive → Dead через событие с полным именем тега **Character.Event.Death**. Если граф стартует уже для мёртвого Pawn, стартовая ветка должна сразу выбирать Dead по IsDead.
4. Создать два Data Assets класса **NGControlStateDefinition**. У обычного Alive можно оставить ресурсы пустыми: пять базовых способностей уже даёт CharacterDefinition. У Dead также оставить AbilitySets/Effects пустыми, bBlockGameplayInput=true. Это состояние должно работать и после отсоединения ASC.
5. В Components каждого UHFSMState добавить **Character State Resources** и назначить соответствующий Definition. Не использовать UI-specific HFSMPlayerStateComponent/HFSMInputComponent в сетевых состояниях персонажа: они требуют LocalPlayer.
6. Для ошибок входа настроить OnEnterFailed: серверный переход в безопасное состояние/лог. HFSM откатывает успешно вошедшие компоненты, но сам не выбирает новый transition после ошибки.

Базовые состояния переиспользуются разными Pawn Blueprint. В графе остаются только переходы; выдача/снятие ресурсов находится в C++. UI граф не переносится на Pawn и не зависит от его жизни.

## Definition состояния

`GameplayFoundation/Public/Character/NGControlStateDefinition.h` не зависит от HFSM, UI или ALS:

- AbilitySets — временные способности/эффекты состояния.
- Effects — дополнительные временные GE.
- MappingContexts — Gameplay IMC/priority, подключаемые только owning client.
- bBlockGameplayInput — блокировка всего Gameplay-ввода, совместно с UI policies; режим экрана/фокус это поле не меняет.

Эффекты только Duration/Infinite со **StackingType=None**. Instant нельзя отменить; aggregated stacking может вернуть общий handle для разных scopes, и его удаление затронуло бы чужой эффект. Такие конфиги отклоняются. Одноразовое лечение/урон должно быть отдельным явным серверным действием/ability, не временным ресурсом.

Способности и input tags, уже доступные через CharacterDefinition, equipment или родительское состояние, повторно выдавать нельзя. Проверка предотвращает дубликаты. Для изменения доступности уже существующей способности использовать block/cancel tags и GE, а не вторую копию spec.

## Input состояния

В GameplayInputConfig добавлен **StateMappingContexts** — каталог IMC для временных состояний. Их actions участвуют в editor-валидации, но эти IMC не активируются базовым input-компонентом. IA → Native/GAS tag маршруты всех предполагаемых состояний перечисляются один раз в том же InputConfig. Definition активного состояния подключает нужные IMC из этого каталога.

Это не создаёт вторую обработку ввода. Один GameplayInputComponent обслуживает actions, а states управляют доступными mappings. Новое поведение камеры/movement всё ещё требует своего native-handler/адаптера; IMC сам не реализует свободную камеру или possession. Gameplay/UI IA и IMC остаются раздельными.

Состояние получает context/policy handles у существующего UPlayerInputSubsystem. На клиенте до получения possession ввод откладывается; lifecycle Refresh после OnRep_Controller, готовности ASC и SetupPlayerInputComponent применяет актуальные ресурсы без Tick/ожидающего таймера. При выходе или UnPossess собственные handles освобождаются. Вложенный UI сохраняет свою блокировку независимо от выхода игрового состояния.

## GAS lifecycle и старое тело

На сервере NGCharacterStateResources выдаёт resources на текущий ASC Avatar. Снятие базовых и временных ресурсов происходит до замены ActorInfo. PlayerState.StartLife дополнительно освобождает ресурсы прежнего Avatar до переключения на новый. При UnPossess FSM тела сохраняется, а ресурсы ASC/локального input отсоединяются. Поздний cleanup со старым Avatar не удаляет effects нового Avatar; обход штатного lifecycle приводит к warning и не считается поддержанным сценарием.

Смерть по-прежнему фиксируется существующим bDead на Pawn, не новым состоянием здоровья. OnRep_Dead/серверный death path уведомляет FSM через настроенный тег; owning/remote клиенты получают граф через GCFSM, а bDead остаётся независимым долговечным источником факта смерти.

**Ragdoll и GA_Death в этот пакет не входят.** В ALS RagdollStart отключает replicate movement, а SetActorLocationDuringRagdoll получает pelvis target от IsLocallyControlled. После UnPossess этот путь нельзя объявлять серверной синхронизацией тела. Нужен отдельный ограниченный пакет server-authoritative corpse/ragdoll adapter. Dead-state этого каркаса не должен требовать ASC после его отсоединения; ability сама по себе не заменяет реплицируемое состояние тела.

## Что подтверждено

Полная сборка NEXTGENONEEditor Win64 Development — PASS. C++ классы/линковка подтверждены; Blueprint-граф пока не создан этим пакетом, его запуск и сеть не проверены. Не запускались PIE, два клиента, late join/relevancy, dormancy, UnPossess/respawn и rollback на реальных gameplay событиях. Эти проверки требуют отдельного апрува после авторинга графа и configs.
