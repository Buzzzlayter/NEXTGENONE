# NEXTGENONE: Gameplay Foundation

**Текущий статус, 2026-11-01:** объединение игровых сущностей и удаление Voxel завершены после закрытия редактора пользователем. C++ переименован в `Nextgenone*`, добавлен общий GameMode; исходники, зависимости, ассеты и артефакты сборки NgxVoxel удалены. Полная сборка прошла, миграция Blueprint завершилась с exit 0. Единственная рабочая карта проекта — `/Game/Maps/Gameplay`. Игровые тесты этого рефакторинга не запускались; прежние сетевые PASS относятся к версии до переименования. Подробности — раздел 14.

Baseline: 2026-10-31, утверждён пользователем 2026-11-01. GAS и Combat Foundation реализованы. Ниже сохранено исходное обоснование; фактическая реализация и уточнения перечислены в разделе 13. Результаты запуска: [MultiplayerCombat.md](../Testing/MultiplayerCombat.md).

Исследование: [LyraAnalysis.md](../Research/LyraAnalysis.md). Ветка: `research/gameplay-foundation`. Существующие незакоммиченные изменения пользователя сохранены. Оригинальный ALS plugin и defaults карт не изменялись реализацией.

## 1. Аудит исходного состояния до реализации (2026-10-31)

Проверены исходники, Config, `.uproject`, список assets через Asset Registry, CDO и отдельные Blueprint-графы через работающий MCP редактора. Сетевой gameplay в этой работе не запускался.

| Область | Установленный факт | Значение для интеграции |
|---|---|---|
| Модули | `.uproject`: NEXTGENONE, NgxVoxel, UIModule, ALSv4_Community | Памятка об одном модуле устарела; новый модуль пока не нужен |
| Движок | EngineAssociation 5.8; локальный Build.version: 5.8.0 | API проверять по `S:/UE_5.8`, а не по Lyra 5.4 |
| Карта и режим | DefaultEngine.ini выбирает ALS_DemoLevel и ALS_GameMode_SP | Это defaults; World Settings конкретной карты могут переопределять режим |
| ALS GameMode CDO | Pawn ALS_CharacterBP_C, Controller ALS_Player_Controller_C, обычные Engine.PlayerState/GameStateBase/HUD | GAS и существующий HUD сюда ещё не подключены |
| Старый проектный GameMode | BP_NextgenoneGameMode наследует GameModeBase; Pawn BP_NextgenonePawn, стандартный Controller, HUD BP_NextgenoneHUD | Второй путь запуска с UI и voxel Pawn; нельзя считать его ALS-путём |
| Character | ALS_CharacterBP → `/Script/ALSV4_CPP.ALSCharacter` | Нужен собственный производный персонаж с GAS hooks |
| Компоненты ALS CDO | CharMoveComp, capsule, mesh, HeldObjectRoot, StaticMesh/SkeletalMesh для held object, MantleComponent, DebugComponent | Не добавлять вторую locomotion/camera систему; проверить конфликт held objects с оружейным visual |
| Input | DefaultInput.ini включает EnhancedPlayerInput/EnhancedInputComponent; ALSPlayerController реально привязывает actions | Это подтверждённая интеграция, не только включённый plugin |
| UI lifecycle | BP_NextgenoneHUD BeginPlay запускает GCFSM Game → FSMState_GameRuntime; EndPlay без подключённой логики в прочитанном графе | Нужен аккуратный shutdown подписок и сохранение HFSM-пути |
| UI presentation | UIModule: HFSMState, UIWidgetMounterComponent, UILayoutTemplate/LayoutTemplatesSorter, PlayerInputSubsystem | Сохраняем монтаж, focus, local-player context и input modes |
| UI events | Есть BlueprintAssignable input delegates. В C++ UIModule не найдено использование GameplayMessageSubsystem | Наличие router plugin не означает готовой интеграции UI через message bus |
| Ability panel | WBP_AbilityPanel — обычный UserWidget; прочитанный EventGraph содержит неподключённые события | Имя ассета не доказывает существование GAS |
| Tags | DefaultGameplayTags.ini содержит UI.Layer.* и UIModule.Placeholders.* | Combat tags добавлять с отдельными назначениями |
| Сеть ALS | ALSBaseCharacter реплицирует acceleration/control rotation, состояния и использует montage/ragdoll RPC | Это движение/presentation, а не подтверждение сетевого combat |
| Оружие/GAS | Поиск C++ не нашёл ASC/health/weapon foundation; `/Game` Asset Registry поиск Weapon пуст | Полноценный оружейный пак не подтверждён; ALS held props не считать боевой системой |
| Старый Pawn | ANextgenonePawn — отдельный voxel/debug Pawn; input напрямую вызывает voxel mutations и Chaos fields | Его инструменты не являются серверным damage pipeline и не подходят для grenade без отдельной интеграции |

Основные пути аудита: `Source/NEXTGENONE/NextgenonePawn.{h,cpp}`, `NEXTGENONE.Build.cs`, `Source/NgxVoxel/`, `Source/UIModule/Public/Components/`, `Source/UIModule/Private/Components/`, `Source/UIModule/Private/Input/`, `Plugins/ALSV4_CPP/Source/ALSV4_CPP/{Public,Private}/Character/`, `Config/Default{Engine,Input,GameplayTags}.ini`.

Подключённые в `.uproject` плагины: ALSV4_CPP, ModelContextProtocol, Terminal, EditorToolset, ProceduralMeshComponent, GCFSM; ModelingToolsEditorMode выключен. GameplayMessageRouter присутствует в Plugins и имеет EnabledByDefault=true. Его runtime-модуль не является текущей зависимостью UIModule. GAS в NEXTGENONE.Build.cs пока отсутствует.

Граница аудита: не прочитаны все графы всех imported assets, World Settings каждой карты и все defaults HFSM-компонентов. Утверждение «во всём проекте системы нет» из отрицательного текстового поиска не делается. Полный weapon pack вне workspace, путь к которому в задании оставлен placeholder, также не проверен.

## 2. Решения и причины

1. **ASC и HealthSet на собственном PlayerState.** OwnerActor=PlayerState, AvatarActor=текущий ALS Pawn. ASC переживает respawn; жизненное состояние сбрасывается отдельной серверной операцией. PlayerState не является сохранением профиля между серверами/сессиями.
2. **Собственные ALS Character и PlayerController как тонкие производные классы.** Их код только связывает существующие callbacks с gameplay-компонентами и input. ALS plugin, movement component, camera и AnimInstance сохраняются.
3. **Первый срез: одно оружие и реплицируемые structs в компоненте.** Mutable ammo/timing не живут внутри Ability. Для одного слота не нужны FastArray и сетевой UObject. Когда потребуется переносимый loot, runtime item станет отдельным владельцем предметного состояния; миграция описана ниже.
4. **Стрельба выполняется на сервере через GAS.** Начать с ServerOnly execution, одиночного выстрела на нажатие. Локальный preview звука/отдачи допускается отдельно; здоровье, ammo и confirmed hit не предсказываются. Это осознанное упрощение для проверки authority и lifecycle. LocalPredicted GAS activation можно добавить после базовых сетевых тестов, без смены владельцев состояния.
5. **UI получает snapshot и delegates.** Используем существующий HFSM и local-player контекст; не вводим CommonUI/MVVM и обязательный глобальный message bus. GameplayMessageRouter остаётся доступным для будущих независимых listeners, но не становится источником состояния или сетевым транспортом.
6. **Сначала код в NEXTGENONE.** Gameplay не зависит от UIModule или ALS API, кроме интеграционных классов. Существующие зависимости NEXTGENONE не переписываем; новые слои не расширяют их без необходимости. Выделение модуля/плагина — после проверенного потребителя и понятной границы.

Альтернатива ASC на Pawn проще для одноразового персонажа, но усложняет устойчивые player-level grants и будущие possession/respawn сценарии. PlayerState выбран с явной ценой: порядок репликации, очистка per-life effects и осторожность с relevancy/частотой обновлений.

## 3. Владельцы и репликация

| Данные | Владелец runtime | Кто изменяет | Кому передаём |
|---|---|---|---|
| Ability specs и player grants | ASC на PlayerState | Сервер | Владеющему клиенту через GAS |
| Health/MaxHealth | HealthSet на PlayerState | Серверные GE | Реплицируемые атрибуты, доступные наблюдателям согласно relevancy PlayerState |
| DeathState текущей жизни | HealthComponent на Pawn | Сервер | Всем relevant клиентам, RepNotify |
| Текущее оружие, definition ID, equip revision | CombatComponent на Pawn | Сервер | Relevant клиентам для held visual |
| MagazineAmmo и версия ammo snapshot | CombatComponent, приватная часть state | Сервер | OwnerOnly |
| NextAllowedFireTime, обработка повторных команд | CombatComponent | Только сервер | Не требуется публичная репликация |
| Weapon/grenade параметры | Definition data assets/defaults | Авторинг, сервер читает доверенные данные | Стабильная ссылка/ID; данные должны быть в cook |
| Spread preview, recoil, UI subscriptions | Клиентские adapters | Локально | Не реплицируются как gameplay truth |
| Активный Pawn и possession | Engine Controller/GameMode | Сервер | Штатная engine replication |
| Будущий raid inventory | InventoryComponent на PlayerState с owner-only entries | Серверные транзакции | Владельцу; другим — только equipped/world-loot projection |

Mixed означает режим репликации Gameplay Effects; это не автоматическая настройка всех атрибутов и не OwnerOnly для любого поля. Health/MaxHealth получают явный RepNotify с GAS macro. В первом срезе это означает видимость здоровья relevant наблюдателям; если дизайн требует скрытого здоровья, позже нужен отдельный owner-only путь, а не надежда на Mixed.

PlayerState должен иметь корректный owner Controller для GAS owning connection. Частоту его обновлений измерять; не копировать Lyra 100 Hz без профилирования. Не вводить Iris/ReplicationGraph как условие старта.

## 4. Lifecycle ASC и respawn

Проектные типы: `ANGPlayerState`, `UNGAbilitySystemComponent`, `UNGHealthSet`, `ANGCharacter : AALSCharacter`, `ANGPlayerController : AALSPlayerController`, `UNGHealthComponent`, `UNGCombatComponent`.

Сервер создаёт PlayerState с default-subobject ASC/HealthSet. Character::PossessedBy после Super вызывает общий идемпотентный TryInitializeGameplay. Клиент делает то же из OnRep_PlayerState, а при смене Controller — RefreshAbilityActorInfo/повторную попытку из OnRep_Controller. BeginPlay — дополнительная попытка, не единственный вход. Owning input readiness отдельно повторяется после создания InputComponent/получения Pawn; simulated proxy не требует локального Controller.

Bind готов, когда существуют нужные PlayerState/ASC и Pawn; он вызывает InitAbilityActorInfo(PS, Pawn), подключает health/combat listeners и выдаёт локальное OnGameplayReady. При повторном вызове на той же паре не сбрасывает HP, не выдаёт вторые grants и не создаёт дубли подписок. Способности выдаются только сервером. Клиент ждёт репликации specs и не обращается к отсутствующей способности как к готовой.

UnPossessed/EndPlay снимают локальные callbacks. Сервер отменяет per-life/equipment abilities и timers, снимает их handles и временные эффекты. ASC отвязывается **только если его AvatarActor всё ещё равен уходящему Pawn**. Иначе старый EndPlay может разрушить новый bind.

Respawn в тестовом GameMode: удалить старые equipment/life grants, убрать life-scoped effects и death blocking, создать/possess новый Pawn, один раз инициализировать Health через server init GE и выдать life/equipment sets. Persisting player grants не выдаются повторно. Новый Pawn получает новый LifeId; запоздалые callbacks/запросы старой жизни отклоняются. Старый труп не должен управлять ASC нового персонажа.

В реальном raid автоматический respawn не предполагается; тестовый respawn нужен для проверки lifecycle. Reconnect, seamless travel, spectator и backend persistence — отдельные сценарии, ещё не реализованные.

## 5. Ability sets, effects и tags

Минимальный AbilitySet: список ability classes с input tag и level, список начальных effects; выдача возвращает отдельный handles record. HealthSet создаётся один раз на PlayerState и не снимается с weapon set. Grants разграничены по scope: player, life, equipment. Снятие набора не вызывает ClearAllAbilities и не откатывает instant GE.

Минимальные tags:

- `Input.Combat.Fire`, затем `Input.Combat.Reload`, `Input.Combat.Grenade`, `Input.Interaction`.
- `Ability.Combat.Fire`, `Ability.Combat.Reload`, `Ability.Combat.ThrowGrenade`.
- `State.Dead`, `State.Reloading`, `State.Equipped.Weapon`.
- `Data.Damage` — SetByCaller magnitude для damage GE.
- `GameplayCue.Combat.Fire`, `GameplayCue.Combat.Impact` — только если реализация использует GameplayCue вместо собственного cosmetic event.

Combat tags регистрировать native в одном месте; существующие UI tags сохранить. Не добавлять tags-зеркала каждого числового поля. Server gameplay проверяет Health/DeathState и наличие equipped state, даже если клиентские tags временно запоздали. Dead tag на ASC должен иметь выбранный механизм репликации (например, серверный duration GE с granted tag), а не случайный локальный loose tag.

Effects первого среза: InitHealth, Damage (instant, положительный конечный Data.Damage в IncomingDamage meta attribute), DeadState (серверный blocking effect). HealthSet в PostGameplayEffectExecute превращает IncomingDamage в уменьшение Health и очищает meta attribute. Clamp Health в [0, MaxHealth], MaxHealth >= 1; отклонять NaN/Inf и отрицательный входной damage до применения. Не принимать magnitude с клиента. Custom execution/context пока не нужен для фиксированного damage без armor/falloff.

## 6. Weapon foundation и серверный выстрел

`UNGWeaponDefinition`: range, damage, fire interval, magazine capacity, optional visual mesh/socket/transform. Параметры валидируются, а defaults обеспечивают тестовое оружие без внешнего weapon pack. Неизменяемые definition assets не содержат текущие ammo или last-fire time.

`UNGCombatComponent`: один equipped slot; authoritative mutable state с magazine ammo, NextAllowedFireTime, equip revision и LifeId. Replicated presentation state содержит definition/visual и revision; owner-only ammo snapshot позволяет UI восстановиться после открытия окна/потери packets. Смена оружия сначала отменяет текущий fire/reload, снимает старые equipment grants, затем атомарно устанавливает новый state и выдаёт новые grants. SourceObject ability spec указывает на combat component; при активации ability получает только текущее оружие и проверяет актуальность grant/revision.

Первый выстрел:

1. Enhanced Input Started вызывает activation соответствующей granted ability через ASC. Fire — ServerOnly; клиент не вызывает отдельный RPC с damage/HitResult. Один запрос — один выстрел; автоматический огонь позже управляется серверным timer.
2. Сервер проверяет актуальные owner/avatar и жизнь, possession, granted spec, equipped weapon, Health>0, отсутствие Dead/Reloading, ammo>0 и свой NextAllowedFireTime. Ограничение частоты находится в компоненте и переживает EndAbility/re-activation; повторная активация не обходит cooldown.
3. Для первого среза направление берётся из серверного ControlRotation, origin — из доверенного eye/weapon anchor Pawn. Не использовать клиентский camera location как начало авторитетного trace. Дальность и damage читаются из server definition.
4. Сервер делает hitscan, игнорируя shooter/его held actors. Для third-person aim допустим первый trace из eye к aim point, затем из weapon anchor к этому point с проверкой близкого препятствия. До появления корректного muzzle socket используется заданный fallback anchor, одинаковый на headless server. Он не зависит от клиентской анимационной кости.
5. После проверок сервер подтверждает расход одного патрона и устанавливает NextAllowedFireTime, выполняет trace и применяет damage GE к найденному ASC. Промах тоже расходует патрон. Commit failure не должен оставлять частично списанное состояние: порядок commit и weapon mutation определяется одной серверной операцией без latent разрыва.
6. Сервер публикует cosmetic fire result (shot ID, origin, impact, normal, surface при наличии), owner-only ammo и обычную attribute replication. Точные HP/полный GE spec в cosmetic packet не нужны. Ability завершает действие и очищает callbacks/tasks на всех путях.

Клиент может немедленно показать recoil/sound preview, но confirmed hit marker появляется только от сервера. При добавлении preview требуется ShotId для suppression двойного эффекта на владельце; до этого проще показывать только confirmed cosmetics. Cosmetic multicast может быть unreliable: выпадение искры допустимо, выпадение HP/ammo/death состояния — нет. Late join восстанавливает equipped/death state из replication, а старые трассеры не воспроизводит.

Нет lag compensation/rewind: на 100–200 ms сервер проверяет текущий мир и последнее полученное направление. Это может ухудшать попадания по движущимся целям. Добавление client aim samples потребует отдельного ограниченного/проверяемого протокола; не расширять сейчас RPC до произвольных target data. Семантика ServerOnly GAS execution не заменяет перечисленные игровые проверки и ограничения частоты запросов.

Collision channel WeaponTrace настраивается в проекте и проверяется на стенах, capsule и mesh; стандартный Visibility не считать автоматически подходящим. Для первого среза выбрать capsule hit без headshots. Если позднее используется bone-based muzzle/mesh hit на dedicated server, отдельно обеспечить обновление skeletal transforms на сервере.

## 7. Урон, смерть и ALS presentation

На первом серверном переходе HP>0 → HP<=0 HealthComponent устанавливает Dead state ровно один раз, блокирует боевые действия, отменяет fire/reload, останавливает серверное движение и уведомляет GameMode. HealthSet хранит числовое здоровье; компонент хранит жизненный переход. Damage после смерти не выдаёт второй death event.

DeathState реплицируется независимо от коротких ability/cosmetic событий, с идемпотентным OnRep. Клиент не решает, умер ли другой игрок, по тому, что увидел impact. Правила collision трупа, unpossess и задержка удаления задаются сервером; респавн не ждёт anim notify клиента.

ALS adapter получает EquipChanged/FireConfirmed/DeathStarted и обновляет overlay/held visual или вызывает доступный cosmetic hook. Gameplay не читает ALS AnimInstance как источник ammo/ready/dead state. Существующий ALS ragdoll имеет сетевые RPC: нельзя без проверки вызывать его из OnRep на всех клиентах и повторно рассылать события. Для первого теста достаточно надёжного replicated death indicator и блокировки движения; ragdoll подключать отдельным проверенным адаптером.

## 8. Input и существующий UI

В производном PlayerController сначала вызывается ALS Super::SetupInputComponent, затем добавляются только combat bindings. Причина: ALS очищает ActionEvent/Value/Debug bindings при своём setup. Повторная настройка удаляет только собственные handles и не вызывает глобальный ClearAllMappings. Combat mapping context добавляется/снимается для конкретного LocalPlayer.

Started — одиночное нажатие; Completed/Canceled очищают held state. Переход в UI-only, потеря focus, unpossess и teardown также очищают combat input/preview timers. UI запрет ввода — удобство пользователя, а не серверная защита. Сервер всё равно проверяет допустимость действия.

Существующий UI получает read-only snapshot HP/MaxHP/DeathState/ammo/equipped и делегаты изменений от pawn-bound presenter. При создании widget: подписаться и прочитать текущий snapshot; при смене Pawn: отвязать старое и подключить новое; при Exit/EndPlay: снять подписки. Никакого per-frame polling и поиска первого PlayerController. Owning player берётся из существующего HFSM context.

`UUIWidgetMounterComponent` продолжает размещать widgets в UILayoutTemplate/placeholder. Gameplay-компоненты не создают widgets и не знают UI layer tags. Presenter живёт в интеграционном коде NEXTGENONE, зависит от gameplay и при необходимости UIModule; обратной зависимости UIModule от gameplay нет. Для первого HUD можно использовать Blueprint read-only delegates без нового универсального presentation framework.

## 9. Inventory и interactions: границы будущего развития

Inventory пока только проектируется. Будущий raid item имеет server-generated ItemId, DefinitionId и mutable state. InventoryComponent на PlayerState владеет carried items и owner-only FastArray. Equipped slot ссылается на ItemId; ammo переносимого оружия принадлежит item instance, а CombatComponent управляет только активными действиями и presentation. При этой миграции поле ammo из первого prototype state удаляется, а не становится второй копией источника истины.

Loot actor/контейнер владеет предметом после drop/death transfer; transfer — атомарная серверная операция с проверкой текущего owner, доступности места и отсутствия повторного запроса. PlayerState не сохраняет уже переданный corpse loot. Stash/backend persistence, вес, сетка, nested containers и торговля — отдельные задачи.

Interaction сначала нужен для equip/pickup. Клиент локально обнаруживает target и показывает option. Сервер получает intent с target/option ID, заново проверяет distance, LOS, state, permissions, конкурентное владение и выполняет действие. Для простого pickup достаточно interface и одной ability; не нужен ASC на каждом ящике. Длительное действие позже получает server timer и правила отмены при движении/уроне/потере цели. UI option не является разрешением на серверную мутацию.

## 10. Grenades после проверенного оружия

GrenadeDefinition: projectile class/default primitive visual, speed, fuse, radius, damage, cooldown. Ability запрашивает действие; сервер проверяет жизнь, inventory count/cooldown, место spawn и сам создаёт replicated actor с ProjectileMovement. Только сервер запускает fuse и применяет radial damage через тот же GAS damage effect.

Взрыв выполняется один раз, делает overlap уникальных victims, при выбранных правилах проверяет LOS/falloff и применяет GE на authority. Клиенты получают movement и cosmetic explosion; они не спавнят второй damage projectile. Count хранится в server state, не в ability instance. Chaos/voxel destruction из старого debug Pawn сюда автоматически не подключается: его replication/authority ещё не решены.

## 11. План изменений после архитектурной проверки

Предлагаемая раскладка в существующем `Source/NEXTGENONE/`:

| Каталог | Первый потребитель |
|---|---|
| Gameplay/AbilitySystem | ASC, AbilitySet, базовая Ability, native tags |
| Gameplay/Attributes | HealthSet и damage/init/dead effects |
| Gameplay/Combat | WeaponDefinition, CombatComponent, Fire ability |
| Gameplay/Health | HealthComponent и death state |
| Integration/ALS | Character/Controller subclasses, presentation hooks |
| Integration/UI | Только необходимая привязка snapshot/delegates |
| Game | PlayerState и тестовый GameMode |

Build.cs: добавить GameplayAbilities, GameplayTags, GameplayTasks; ALSV4_CPP только для производных интеграционных классов. `.uproject`: явно включить GameplayAbilities. Не добавлять LyraGame, GameFeatures, CommonUI, OnlineSubsystem или message-router dependency ради одного выстрела.

Создать проектный Character Blueprint с собственным C++ parent и существующими ALS mesh/AnimBP/settings. Безопасное наследование от plugin Blueprint и подстановка собственного C++ parent требуют отдельного подхода: нельзя одновременно назначить два parent. Практический путь — одна проектная копия ALS_CharacterBP с сохранением graph/defaults и reparent на ANGCharacter, затем compile/проверка ссылок; оригинальный plugin asset не менять. Это точечная копия, а не массовая миграция контента.

Проектный Controller Blueprint наследуется от ANGPlayerController и сохраняет ALS camera manager и IMC defaults. Тестовый GameMode указывает эти Pawn/Controller, ANGPlayerState и существующий BP_NextgenoneHUD. Новую конфигурацию проверять на отдельной combat test map в этом же проекте; defaults старых карт не переключать до smoke tests. Сборка после GAS foundation, затем после weapon foundation. Grenades — после успешной сборки и проверки оружия.

## 12. Проверка и текущий статус

Первоначальный исследовательский этап выполнен 2026-10-31. Реализация и фактические проверки добавлены 2026-11-01; подробные результаты и ограничения приведены в [MultiplayerCombat.md](../Testing/MultiplayerCombat.md). Матрица ниже остаётся перечнем сценариев, а не утверждением, что каждый из них пройден.

После реализации нужен `Docs/Testing/MultiplayerCombat.md` с фактическими результатами, датами и логами. Минимальная матрица:

- Сборка Editor Development через ue-build; при открытом редакторе — подходящий Live Coding path с чтением реального результата. Новые reflection types и constructors могут требовать полного restart/offline build.
- PIE: Play As Client, два клиента, отдельный dedicated server; затем повторить с отдельными процессами. Наличие двух окон без authority-проверок недостаточно.
- Для каждого клиента: ASC Owner=его PlayerState, Avatar=его Pawn, grants без дублей; simulated proxy корректно получает HP/death/equipment.
- Equip, fire, промах, попадание, пустой магазин, spam activation, смерть во время fire/reload, respawn при задержанном удалении старого Pawn, late join.
- HP меняется от серверного GE; попытка локально изменить HP не меняет сервер и второго клиента. Ammo не становится отрицательным, fire rate нельзя обойти EndAbility или повторным equip.
- Стена рядом с оружием блокирует trace; dedicated server не зависит от локальной камеры/анимационного tick.
- UI open/close, UI-only input, focus loss, смена Pawn, restart PIE: нет дублированных listeners, stuck fire и shared first-player context.
- 0/100/200 ms и отдельный packet-loss сценарий: фиксировать именно введённые one-way delay/RTT настройки, потери и наблюдаемый результат. Проверить согласованность конечного HP/ammo/death; отсутствие rewind остаётся известным ограничением.
- ALS movement, mantle, camera, overlays и существующий HFSM HUD проходят отдельные regression smoke checks.

До реализации ключевые решения для ревью: ASC на PlayerState; ServerOnly fire в первом срезе; component-owned weapon state с будущим переносом в item runtime; проектные ALS subclasses и один составной GameMode; UI через snapshot/delegates без новых framework dependencies.

## 13. Реализованный Combat Foundation — 2026-11-01

Код находится в `Source/NEXTGENONE/Gameplay/`. Имена фактических типов: `ANGPlayerState`, `UNGAbilitySystemComponent`, `UNGHealthSet`, `UNGAbilitySet`, `FNGGrantedHandles`, `ANGCharacter`, `ANGPlayerController`, `UNGWeaponDefinition`, `UNGCombatComponent`, `UNGFireAbility`, `ANGCombatTestGameMode`, `UNGSetupCommandlet`.

Уточнения утверждённого baseline:

- Для минимального среза health/death bridge находится в производном ANGCharacter: отдельный HealthComponent пока не добавлен. Единственный источник HP — HealthSet на PlayerState. `bDead` на Pawn — реплицируемое состояние перехода, а не второй запас здоровья.
- ASC Mixed, HP/MaxHP с явным RepNotify; OwnerActor=PlayerState, AvatarActor=текущий Pawn. BeginPlay/PossessedBy/OnRep_PlayerState/OnRep_Controller и controller OnRep_Pawn повторяют идемпотентный bind. Сервер и owning clients проверены сетевыми отчётами.
- Player grants выдаются один раз на PlayerState; equipment grants снимаются по handles при unequip/death/unpossess/EndPlay. Повторный bind не выдаёт набор повторно. InitHealth GE применяется один раз для нового серверного Pawn, а не при каждом клиентском bind.
- `State.Dead` — native tag, сервер выставляет/очищает его через UE 5.8 `SetLooseGameplayTagCount(..., EGameplayTagReplicationState::TagOnly)`. Отдельный duration DeadState GE не понадобился. `GA_Fire` блокируется этим tag и дополнительно проверяет server Pawn/HP.
- Старый/dead Pawn не возвращает ASC через IAbilitySystemInterface. Teardown меняет avatar только при совпадении с уходящим Pawn. Damage context несёт конкретный hit Pawn как SourceObject; HealthSet отвергает damage, если он больше не совпадает с avatar. Таким образом GE для старого тела не повреждает новую жизнь на том же PlayerState.
- Health init и damage — native GameplayEffect classes. Damage использует `Data.Damage` → IncomingDamage meta attribute → Health; конечные/положительные величины проверяются сервером. Для применения damage использовать `UNGCombatComponent::ApplyDamageToPawn`, который формирует необходимый context.
- Один weapon slot в CombatComponent. Public equipment state: bEquipped, Revision, Definition. MagazineAmmo — OwnerOnly; unequip не пополняет магазин. PlayerState::NextAllowedFireTime переживает EndAbility, equip/unequip и смену Pawn. Это ограничитель игрока для текущей конфигурации с одним оружием.
- ServerOnly `UNGFireAbility` вызывает production FireOnServer из выданного spec с CombatComponent в SourceObject. Клиент не присылает HitResult или damage. FireOnServer атомарно проверяет state/ammo/time, списывает ammo, делает trace и применяет GE. Это одиночный выстрел на Started; reload и automatic fire не реализованы.
- Первый trace из серверного eye определяет aim point, второй из фиксированного Pawn anchor проверяет близкие препятствия. Используется Visibility с явным Block у capsule в BeginPlay, потому что скопированные Blueprint defaults перекрывали constructor response. Отдельный WeaponTrace channel остаётся будущим улучшением, а текущий выбор ограничивает возможность независимо настроить visibility и оружейную collision.
- Тестовая WeaponDefinition — стабильный default subobject Character, без внешнего asset pack; mesh — engine cube. Можно назначить authored WeaponDefinition data asset через TestWeapon. Definition содержит mesh/socket/transform, CombatComponent публикует cosmetic events для адаптера weapon pack. Полноценные анимации/ретаргетинг не добавлялись.
- Read-only HP/death/equipment/ammo getters и BlueprintAssignable delegates подготовлены для существующих UI widgets. Существующий BP_NextgenoneHUD используется игровым GameMode; новый UI framework не вводился. Health panel подключена позднее, см. раздел 16; ammo widget пока не привязан.
- `/Game/GameplayFoundation/BP_NGCharacter` и `BP_NGController` — точечные копии ALS Blueprint с сохранением графов/defaults и собственными C++ parents. AnimBP остаётся существующим ALS. Карта `/Game/GameplayFoundation/Maps/CombatTest` имеет свой GameMode override; GlobalDefaultGameMode, GameDefaultMap и EditorStartupMap не переключались.
- Editor-only NGSetup commandlet создаёт только эти тестовые assets, отказывается перезаписывать существующие, компилирует новые копии и сохраняет только собственные packages. Blueprint loading тестового GameMode перенесён в InitGame после регистрации native CDO.

Из зависимостей добавлены только GameplayAbilities, GameplayTags, GameplayTasks и ALSV4_CPP для производных типов. UnrealEd/Kismet/BlueprintGraph/AssetTools используются условно для editor setup. LyraGame, GameFeatures, CommonUI, новый inventory framework и grenade system не добавлены.

Тестовый RPC harness активируется только в non-Shipping с `-NGCombatSmoke` и отдельным NGCombatTestGameMode. Он проверяет реальные client→server GAS/RPC requests и client replication reports. Production игровой ввод: E — equip, U — unequip, ЛКМ — одиночный fire; команды NGEquip/NGUnequip/NGFire доступны для development-проверки. Никаких self-running tests в обычном запуске карты нет.

## 14. Объединение сущностей и удаление Voxel — завершено

Единая цепочка игровых классов:

- `ANextgenoneGameMode` — единственное место настройки Pawn, PlayerController, PlayerState и HUD.
- `ANextgenoneCharacter` — прежний GAS/ALS Character, переименованный без создания второй реализации.
- `ANextgenonePlayerController` — прежний ALS/GAS Controller, переименованный без изменения поведения.
- `ANextgenonePlayerState` — прежний единственный GAS PlayerState, вынесенный из NGAbilitySystem в отдельные NextgenonePlayerState.h/.cpp.
- GameState остаётся стандартным GameStateBase: отдельного дублирующего GameState не добавлено.
- `ANGCombatTestGameMode` наследует NextgenoneGameMode и содержит только harness; копия настройки игровых классов из него удалена.

Для старых native имён NGCharacter/NGPlayerController/NGPlayerState добавлены ClassRedirects. NGSetup commandlet перенёс существующие BP_NGCharacter/BP_NGController в `/Game/Blueprints/BP_NextgenoneCharacter` и `BP_NextgenonePlayerController` через AssetTools RenameAssets. Существующий BP_NextgenoneGameMode перепривязан к общему C++ GameMode; его Pawn, Controller, PlayerState и HUD приведены к одной цепочке. Сохранены исходные ALS-графы, AnimBP и существующий UI HUD.

Пользователь отдельно поручил полностью убрать voxel. Удалены `Source/NgxVoxel/`, NextgenonePawn.h/.cpp и VoxelPawnMovementComponent.h/.cpp. Из .uproject удалены runtime-модуль NgxVoxel и его явное включение ProceduralMeshComponent; из NEXTGENONE.Build.cs удалены NgxVoxel и использовавшиеся удалённым прототипом GeometryCollection/FieldSystem/Chaos dependencies. Сам движок и сторонние плагины физически не удаляются.

По уточнению пользователя связанную с voxel карту удалили целиком: `Content/Maps/DevLevel.umap` и `DevLevel_HLOD0_Instancing.uasset`. Удалены BP_VoxelWorld, BP_VoxelStructure, BP_NextgenonePawn и Content/Voxel. До удаления сделаны резервные копии затронутых ассетов в `Saved/RefactorBackup/`; это не активный Content.

Бывшая CombatTest перенесена в `/Game/Maps/Gameplay`. В её World Settings назначен BP_NextgenoneGameMode; этот же режим и карта назначены в DefaultEngine.ini как project defaults. Отдельная копия карты для тестов больше не нужна: runner использует `/Game/Maps/Gameplay?game=/Script/NEXTGENONE.NGCombatTestGameMode`. Redirector-ассеты прежних Blueprint/map путей исправлены и удалены через AssetTools. Демо-карты исходных сторонних пакетов не являются рабочими картами проекта и в эту очистку не входят.

После закрытия редактора выполнена полная UBT сборка NEXTGENONEEditor Win64 Development: PASS, без compiler diagnostics. Затем `NGSetup -Consolidate` завершился с exit 0, 0 errors и 24 warnings. Логи: `Saved/Logs/NGConsolidationBuild.log`, `Saved/Logs/NGConsolidation.log`. Поиск сериализованных ссылок в активном Content не нашёл NgxVoxel, BP_Voxel*, Content/Voxel и старый Pawn. Удалены также старые NgxVoxel DLL/PDB/patches и его Intermediate каталоги. Это сборка и обработка assets, не игровой тест.

Автотесты, PIE и сетевые прогоны этого рефакторинга не запускались. Для нового этапа тестирования требуется отдельный апрув пользователя.

## 15. Удаление ThirdPerson template — 2026-11-01

По отдельному поручению пользователя полностью удалены Content/ThirdPerson (ThirdPersonMap, BuiltData, BP_ThirdPersonCharacter, IMC_Default и IA_Jump/Look/Move) и Source/ALSv4_Community с шаблонными Character/GameMode. Убраны запись модуля в .uproject и устаревшие TP_ThirdPerson redirects. В проекте остаются два собственных runtime-модуля: NEXTGENONE и UIModule. Плагин ALSV4_CPP, его анимации и используемые основной игрой Blueprint сохранены.

До удаления поиск ссылок в Content и Content плагина ALS находил ThirdPerson/module paths только внутри удаляемого шаблона. Удалённые файлы перенесены в `Saved/RefactorBackup/RemovedThirdPerson_*` вне активного Content/Source. После закрытия редактора пользователем завершено удаление пяти заблокированных ассетов и устаревших артефактов сборки шаблонного модуля. Полная UBT-сборка NEXTGENONEEditor Win64 Development завершилась PASS, без compiler diagnostics; лог — `Saved/Logs/ThirdPersonRemovalBuild.log`. Поиск по активному Content не нашёл ссылок на удалённые пути. PIE и игровые тесты не запускались.

## 16. WBP_HealthPanel → GAS — 2026-11-01

Панель сохраняет пользовательский дизайн и событие `SetData(InPercent)`, обновляющее `pb_health`. Существующий UIWidgetMounter в FSMState_GameRuntime уже монтирует именно WBP_HealthPanel в UIModule.Placeholders.Action, слой Content; монтаж не изменялся.

В Construct панель получает своего OwningPlayer, подписывается на OnPossessedPawnChanged и привязывается к текущему NextgenoneCharacter. BindHealthPawn снимает прежнюю подписку, подключает OnHealthChanged и сразу читает Health/MaxHealth. Начальное значение и события передают в SetData ограниченное [0,1] отношение Health/MaxHealth; при MaxHealth<=0 или отсутствии нужного Pawn — 0. Если GAS ещё не готов, последующий OnHealthChanged из TryInitializeGameplay обновляет начальный snapshot.

ReleaseHealthPawn и ReleaseHealthBindings снимают только собственные callback-подписки; Destruct освобождает обе привязки. Смена Pawn перепривязывает панель. Нет Tick/polling, GetPlayerController(0), локальной копии здоровья или записи в GAS. Панель использует уже существующий Character bridge от реплицируемого GAS HealthSet.

Изменён только WBP_HealthPanel.uasset и эта документация; C++/HUD/layout не менялись. Blueprint скомпилирован и сохранён через Editor MCP. Резервная копия исходного виджета: Saved/RefactorBackup/HealthPanel/. PIE, урон, respawn и multiplayer runtime-проверки панели не запускались — требуется отдельный апрув на тестирование.
