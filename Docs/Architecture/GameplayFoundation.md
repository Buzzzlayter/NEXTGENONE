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

## 17. Первая movement ability и расширение в Blueprint — 2026-11-01

Это уточнение существующего baseline, а не замена архитектуры. ALSV4_CPP сохраняется целиком; изменения находятся в модуле проекта. Реализована `UNGJumpAbility` (`NGJumpAbility.h/.cpp`); следующими возможностями эта реализация автоматически не становится.

### Реализованный маршрут прыжка

`ALS JumpAction → ANextgenoneCharacter::JumpAction_Implementation → Input.Jump → ASC → UNGJumpAbility → ACharacter::Jump / StopJumping`.

- GAS execution: LocalPredicted, InstancedPerActor. CharacterMovement отвечает за перемещение и сетевую коррекцию, ALS — за существующую анимацию. Нового RPC для перемещения и новой системы движения нет.
- CanActivate проверяет текущий avatar, смерть, ALS Grounded/Standing/MovementAction=None и CanJump. CommitAbility поддерживает стандартные GAS cost/cooldown, если они назначены в производном классе.
- WaitInputRelease завершает способность при отпускании. Отмена/EndAbility останавливает удержание прыжка на сохранённом Pawn; позднее завершение не ищет нового avatar на PlayerState.
- StartLife выдаёт fallback-класс JumpAbility один раз на PlayerState. Если PlayerAbilitySet уже содержит Input.Jump, fallback не выдаётся. Сам Ability Set должен содержать ровно одну запись для прыжка; произвольные пользовательские дубли внутри набора не исправляются автоматически.
- ASC предоставляет BlueprintCallable AbilityInputTagPressed/Released. Для новых Enhanced Input actions вызывать Pressed на Started, Released на Completed и Canceled. Dynamic Spec Source Tags назначаются полем InputTag в Ability Set. Не использовать каждый Triggered tick для повторной активации.
- Унаследованный ALS JumpAction может вызываться повторно: Character отслеживает край нажатия. Stand-up/get-up shortcuts и JumpPressedDelegate для существующего mantle component сохранены. Успешное залезание меняет ALS state и блокирует обычный прыжок.
- При teardown текущего avatar очищается InputPressed и отменяются активные способности также на клиенте. Чужой новый avatar старый Pawn не очищает.

Залезание **пока остаётся существующим ALS mantle**, а не отдельной GAS-абилкой. Не добавлять параллельное перемещение из GA_Mantle поверх ALS mantle: при его переносе потребуется один владелец запуска/окончания, проверки ledge и сетевого движения. Автоматический mantle тоже должен подчиняться этим правилам.

### Как делать много способностей

Повторное использование строится на GameplayAbility, Ability Sets, Gameplay Effects, tags и ability tasks. Количество способностей само по себе не требует нового framework. Простая способность из уже доступных задач обычно собирается в Blueprint; новый способ сетевого движения или новый тип взаимодействия требует кода один раз, после чего его используют многие Blueprint-абилки.

| Возможность | Владелец результата | Что настраивается в Blueprint | Основная сложность |
|---|---|---|---|
| Прыжок | CharacterMovement | Производный NGJumpAbility: cost/cooldown, activation requirements; параметры прыжка у CharacterMovement | Предсказание, отпускание, отмена |
| Деш | CharacterMovement / movement task | Длительность, скорость, cost/cooldown, montage, FX | Предсказуемая траектория, коллизия, коррекция; не Tick + SetActorLocation |
| Залезание | Один mantle executor | Проверки/параметры ledge, montage, правила блокировки | Синхронизация движения с ALS, движущиеся поверхности, gravity frame |
| Поднятие оружия | Серверная interaction/equipment логика | GA_Interact/Pickup, montage, UI prompt | Сервер проверяет дистанцию, доступность и одновременный подбор |
| Стрельба | Существующий CombatComponent + GA_Fire | WeaponDefinition, cosmetics, montage/overlay | Авторитет сервера, патроны и limiter уже в текущем foundation |

Практический рецепт для автора Blueprint:

1. Создать Blueprint от GameplayAbility для новой механики либо от готового native-класса для его настройки. `GA_Jump` создавать от NGJumpAbility; его C++ управляет Activate/End, поэтому Event ActivateAbility не является точкой замены алгоритма прыжка. Для нового алгоритма использовать отдельный GameplayAbility.
2. Назначить execution policy осознанно: Fire остаётся ServerOnly; локально предсказываемое движение требует соответствующего movement executor. LocalPredicted сам по себе не делает произвольные Blueprint перемещения корректными по сети.
3. Настроить blocked/required tags, cost/cooldown, CommitAbility, завершение и отмену на всех ветках. Не дублировать HP, ammo или cooldown в переменных Blueprint.
4. Добавить способность в NGAbilitySet с InputTag. PlayerAbilitySet назначается в производном NextgenonePlayerState, выбранном PlayerStateClass основного GameMode. Для замены прыжка подходит Input.Jump; добавлять вторую native выдачу не нужно.
5. В существующем controller/input graph передавать Started/Completed/Canceled в ASC текущего Pawn. Для прыжка маршрут ALS уже добавлен в C++; второй Space binding не нужен. Новые Input.* tags зарегистрировать через Gameplay Tags settings.
6. Для оружейных способностей сохранять equipment grants и снятие по handles. Для постоянных способностей — player grants. Новая жизнь не выдаёт ещё одну копию постоянного набора.

Материалы/меши/анимации оружия из Lyra можно рассматривать отдельно после проверки зависимостей и условий использования. Импорт оружия не должен приносить вторую inventory/equipment систему Lyra или заменять текущий ASC. В этом проходе Lyra и weapon pack не импортировались.

### Границы GCFSM, GAS и UI

- **Общий flow игры:** будущая GCFSM на GameState или другом реплицируемом Actor; переходы с authority. В локальном GCFSM `UGCFSMState::MakeFSM` требует replicated Actor context и UGCFSMReplicationComponent. GameMode не даёт клиентам реплицируемого контекста, HUD локален. Реплицируемая дочерняя FSM не должна иметь NonReplicated parent.
- **Flow игрока/UI:** существующая HFSM с контекстом конкретного LocalPlayer: окна, инвентарь, сумка/подсумки, экран сундука, фокус и input mode. Это представление реплицируемых данных, а не владелец предметов. UHFSMInputComponent уже умеет mapping/input mode; UIWidgetMounter — монтировать виджеты.
- **Действия персонажа:** GAS. FSM миссии задаёт разрешения через tags/effects и посылает события; не требуется вводить GCFSM state на каждую способность. Конкретная server-side блокировка gameplay во время загрузки/синематики ещё не реализована. Одного переключения input mode недостаточно для серверного запрета.
- **Квесты:** сервер меняет прогресс от проверенных gameplay-событий; GCFSM оркестрирует последовательность. Прогресс/награды хранятся в данных, а не только в текущей исполняемой ноде. UI подписывается на изменения и читает начальный snapshot.
- **Синематики:** сервер задаёт фазу, идентификатор и время старта; клиент локально показывает Sequence/переход. Награды и квестовые изменения не должны повторяться из клиентского Sequence event. Late join должен восстанавливать актуальное состояние, а не повторять весь входной эффект.
- **Загрузочные экраны:** локальная готовность загрузки отличается от общей фазы матча. Реплицируемая FSM не работает как единственный механизм экрана до подключения или во время отсутствия World.

Репликация FSM не заменяет репликацию содержимого сундуков/инвентарей. Общая FSM не должна открывать одно и то же окно каждому клиенту: окно получает локальную цель взаимодействия, а сервер проверяет запросы к ней. Эти подсистемы в данном проходе не создавались.

### AnimBP и необычная гравитация

Существующий ALS AnimBP не заменён и не разрезан. Для прыжка его менять не требуется. По мере добавления контента выделять действия через существующие montage slots/overlays, а самостоятельные большие группы поз — через animation layers/linked graphs там, где это уменьшает связанность. Способность инициирует действие/монтаж и обрабатывает окончание; AnimBP читает движение/позу. Он не выдаёт урон, предметы или quest rewards. При ASC на PlayerState montage должен работать с актуальным mesh avatar; не запускать один и тот же montage одновременно через два независимых RPC/ALS/GAS маршрута.

Уменьшение/увеличение силы тяжести и произвольное направление gravity — разные задачи. В ALSBaseCharacter встречаются Velocity.Z и yaw-only расчёты; ALSMantleComponent строит traces, высоту ledge и offsets по мировой Z. Для ходьбы по стенам/сферам потребуется адаптация единственного movement executor, ориентации капсулы/камеры, mantle, IK и ragdoll к gravity frame. GAS не устраняет эти допущения. Новый прыжок не содержит ручного world-Z импульса, но это **не подтверждение поддержки произвольной гравитации ALS**.

### Статус

Код первой movement ability и tag input добавлен; ALS plugin, AnimBP и пользовательские Blueprint не изменены. Editor Development UBT compilation выполнена; runtime/PIE/сетевые сценарии ожидают отдельного апрува. Проверка нового прыжка должна охватить также сохранённый mantle, crouch/get-up, death/respawn и поглощение отпускания кнопки UI/focus loss.

## 18. Редактируемые ability assets: Dash, Mantle, Pickup — 2026-11-01

Этот раздел описывает следующий проход реализации и уточняет исторический статус раздела 17. В `/Game/Gameplay/Abilities` созданы и скомпилированы `GA_Jump`, `GA_Dash`, `GA_Mantle`, `GA_PickupWeapon`, `GA_Fire`, `GE_DashCooldown`, `DA_PlayerAbilities`. Это реальные Blueprint/Data Asset, используемые игровыми классами. Нативный PlayerState загружает default ability set, если пользовательский PlayerAbilitySet не назначен. Input.* tags предотвращают повторную fallback-выдачу при наличии соответствующей записи. GA_Fire выдаётся только экипировкой, не постоянным player set.

| Кнопка | Действие | Где настраивать |
|---|---|---|
| Space | Прыжок через GAS | GA_Jump; физика прыжка — CharacterMovement |
| Q | Деш вперёд по плоскости гравитации | GA_Dash: Speed=1600, Duration=0.18; GE_DashCooldown: 0.8 секунды |
| V | Попытка залезания перед персонажем | GAS — GA_Mantle; ALS traces/curves/animations — BP_ALSMantleExecutor (актуализировано 2026-11-03) |
| E | Подбор оружия под прицелом | GA_PickupWeapon.MaxDistance=250; BP_WeaponPickup.Definition |
| ЛКМ | Один выстрел | GA_Fire; DA_TestWeapon |
| F / U | Выдать/экипировать тестовое оружие / убрать | Существующий debug equip/unequip; E освобождена под pickup |

`/Game/Gameplay/Input/IMC_Abilities` содержит изменяемые Q/V/E bindings и соответствующие IA_Dash, IA_Mantle, IA_PickupWeapon. Controller использует runtime-копию mapping context; файл ассета не изменяется во время игры. Space сохраняет ALS input route. ЛКМ/F/U пока добавляются controller нативно.

### Деш

UNGDashAbility использует LocalPredicted и ApplyRootMotionConstantForce. Направление — вперёд от Character, спроецированное на плоскость gravity, без клиентского teleport RPC. CharacterMovement обрабатывает коллизию и сетевую коррекцию. После уточнения пользователя деш разрешён только при InAir/IsFalling; на земле не активируется. Task сохраняет набранную скорость полёта через MaintainLastRootMotionVelocity, вместо ограничения до walking MaxSpeed. Gravity остаётся включена, вертикальная скорость прыжка/падения не обнуляется. Направления WASD и отдельный вертикальный рывок не реализованы.

Cooldown — duration GameplayEffect на ASC PlayerState с тегом Cooldown.Movement.Dash. EndAbility, повторное нажатие и переэкипировка не снимают cooldown. State.Action.Dash и State.Action.Mantle блокируют конфликтующие действия, включая FireOnServer. Есть Blueprint hook OnDashStarted(Direction) и сетевой GameplayCue.Movement.Dash для будущего FX; готовый Niagara/монтаж в этом проходе не добавлен. OnDashStarted вызывается на predicting owner/server, а не на всех simulated proxies. Для сетевого FX использовать GameplayCue.

### Залезание

Текущий контракт 2026-11-03: после сообщения пользователя о неработающем варианте самостоятельный Root Motion Source, traces и cue из абилки удалены. GA_Mantle — тонкая GAS-обёртка, один BP_ALSMantleExecutor : UALSMantleComponent на Pawn выполняет исходные проверки, движение, Timeline, montage и выбор assets. Проектный наследник содержит только lifecycle/network bridge; отдельный MantleAdapter не возвращён. Настройки находятся в component Blueprint, ALS plugin/AnimBP сохранены. Сборка PASS, authoring exit 0; runtime/сеть ещё не проверены. Подробности: [MantleAbility.md](../Gameplay/MantleAbility.md). Следующие абзацы описывают исторический вариант с отдельным адаптером.

Новый UNGMantleAdapterComponent выполняет probes на authority: стенка, верхняя walkable поверхность, место под капсулу, скорость поверхности, наличие ALS correction curve. GA_Mantle — ServerOnly/ServerOnlyTermination. Commit выполняется после успешного probe и до начала движения. Один reliable multicast запускает существующий ALS timeline/MantleStart на сервере, владельце и proxies.

В `/Game/Gameplay/Components/BP_ALSMantleExecutor` скопирован существующий ALS component Blueprint с его графами/asset selection и переподчинён проектному UNGALSMantleExecutor. У компонента сохранены исходные настройки из Character. Его старый Server_MantleStart переопределён как no-op: клиентская ledge-цель не должна управлять серверным перемещением. Native ALS auto tick/direct JumpPressed маршрут отключены на экземпляре, а автоматический falling catch вызывает ту же GAS-абилку на сервере с интервалом 0.15 секунды. Оригинальный ALS plugin не изменён.

В `/Game/Blueprints/BP_NextgenoneCharacter` заменён только класс существующего SCS mantle-компонента на копию; исходные Character graphs и AnimBP сохранены. Перед изменением создан файл Saved/RefactorBackup/Abilities/BP_NextgenoneCharacter_20261101_152341.uasset. При death/unpossess/cancel останавливается сохранённый executor старого Pawn; timeline не должен включить ходьбу мёртвому Pawn. Есть шестисекундный watchdog. Смена Space→V для ручного mantle намеренная: Space больше не запускает прямой ALS mantle в обход GAS.

### Подбор и стрельба

`/Game/Gameplay/Weapons/BP_WeaponPickup` — размещаемый actor с `/Game/Gameplay/Weapons/DA_TestWeapon`, engine cube mesh и реплицируемыми Definition/availability. Серверная ability делает visibility trace от серверного eye/control rotation, проверяет расстояние и пустой слот. Оружие забирается синхронно на game thread, actor становится недоступен/невидим для всех и late join. Заполненный слот не заменяется. Unequip не освобождает владение слотом: drop/swap ещё не реализованы.

CombatComponent остаётся единственным владельцем экипировки/патронов. Pickup не создаёт второй inventory. FireAbilityClass в WeaponDefinition позволяет назначить производную GA_Fire; RequestFire ищет ability по Input.Combat.Fire, поэтому Blueprint-класс работает без точного сравнения с native class. Серверный fire limiter по-прежнему хранится на PlayerState и не сбрасывается при смене weapon ability.

### Работа автора Blueprint

Открывать указанные GA_* и менять Class Defaults. Эти классы наследуют native Activate/End, поэтому пустой Event Graph не означает отсутствие логики. Не добавлять параллельные Jump, LaunchCharacter, MantleStart, ApplyDamage и отдельные Server RPC в их Event ActivateAbility. Для прыжка доступны OnJumpStarted/OnJumpStopped; для деша — OnDashStarted и GameplayCue. Новые механики, отличные от этих native алгоритмов, можно создавать отдельными GameplayAbility и включать в Ability Set.

Для настройки подбора вручную разместить BP_WeaponPickup на Gameplay и направить взгляд на него. Автоматически карта и project defaults в этом проходе не изменялись. Для настройки ALS mantle assets использовать копию BP_ALSMantleExecutor в /Game, а не plugin asset. Lyra content не импортировался.

### Проверка и ограничения

UBT NEXTGENONEEditor Win64 Development: PASS; лог Saved/Logs/NGAbilityFoundationBuild.log. Создание/компиляция/сохранение ассетов командлетом NGSetup -AbilitiesOnly: exit 0, 0 errors, 14 warnings исходных ALS/Python/deprecation/окружения; лог Saved/Logs/NGAbilityAssets.log. В ходе создания исправлен native cooldown constructor: GE component создаётся через CreateDefaultSubobject, а не NewObject внутри конструктора. Первый неуспешный запуск сохранён как NGAbilityAssets_FirstFailure.log.

Это не runtime или сетевой результат. PIE, gameplay autotests, smoke и latency/loss не запускались; требуется отдельный пользовательский апрув. Непроверены предсказание/коррекции деша, совпадение направлений при быстром повороте, lifecycle/отмена mantle, гонка pickup двух игроков, cooldown после respawn и UI focus-loss. Mid-mantle late join не восстанавливает точное время анимации из snapshot: текущий старт основан на multicast, итоговая позиция реплицируется. Mantle разрешён только при стандартном направлении gravity; движущиеся поверхности быстрее 10 см/с отвергаются. Полноценная поддержка arbitrary gravity, directional dash, drop/swap, inventory, гранаты, квесты и server game-flow FSM не добавлены.

После открытия редактора в логах NGAbilityEditor.log и NEXTGENONE.log зафиксированы PIE-запуски и падение в ALSMantleComponent::BeginPlay: при миграции SCS-компонента потерялась ссылка MantleTimeline. Агент не вызывал PIE; эти падения являются наблюдёнными ошибками, а не успешной проверкой. В миграции сохранён собственный nested Timeline нового template, существующий ассет исправлен командлетом; native executor восстанавливает отсутствующую ссылку до вызова ALS BeginPlay. Остановка отменённого mantle обращается к фактическому Timeline executor. Финальная сборка PASS и повторная обработка ассета exit 0 / 0 errors; NG_MANTLE_TIMELINE: MantleTimeline присутствует в NGAbilityAssets.log. После исправления повторный игровой запуск не выполнялся, устранение падения в PIE ещё требует подтверждения. Editor MCP после падения недоступен; редактор оставлен закрытым.

Файлы этого прохода:

- Новые пары `.h/.cpp`: `Source/NEXTGENONE/Gameplay/NGDashAbility`, `NGMantleAbility`, `NGWeaponPickup`.
- Изменены пары `.h/.cpp`: `NGAbilitySystem`, `NGJumpAbility`, `NGCombat`, `NextgenoneCharacter`, `NextgenonePlayerState`, `NextgenonePlayerController`; изменён `NGSetupCommandlet.cpp`.
- Изменён `Content/Blueprints/BP_NextgenoneCharacter.uasset`; 14 новых ассетов перечислены выше в `Content/Gameplay/Abilities`, `Components`, `Input`, `Weapons`.
- Обновлены `Docs/Architecture/GameplayFoundation.md`, `Docs/Testing/MultiplayerCombat.md`.
- `NGTestGameMode.cpp` в рабочем дереве также содержит предыдущее изменение assertions под Jump grant; в этом проходе он не запускался. Пользовательские изменения вне перечисленного не откатывались. Файлы плагина ALS, основной карты и project defaults не редактировались.

## 19. GA_Punch: кулаки, GAS-урон и отталкивание — 2026-11-01

В `/Game/Gameplay/Abilities` добавлены `GA_Punch` и `GE_PunchCooldown`. В существующий DA_PlayerAbilities добавлена только запись Input.Combat.Punch; другие grants и пользовательский выбор класса не заменяются. Перед дополнением набора сохранена копия в Saved/RefactorBackup/Abilities/DA_PlayerAbilities_BeforePunch_*.uasset. PlayerState также имеет idempotent native fallback для этого input tag.

ЛКМ использует существующий input action: при bEquipped=true — RequestFire, без экипированного оружия — GA_Punch. Убирать оружие можно U. Удар разрешён живому текущему avatar на земле, с контроллером, без оружия и конфликтующего действия ALS. Во время State.Action.Punch заблокированы прыжок, деш, mantle, pickup и стрельба; позднее получение оружия отменяет результат удара при проверке попадания.

GA_Punch имеет ServerOnly execution и ServerOnlyTermination. После CommitAbility ждёт Windup и повторно проверяет сохранённый Pawn/текущий ASC/avatar. Затем сервер делает один sphere sweep впереди груди по Visibility и дополнительную проверку прямой видимости. Первый блокирующий объект останавливает удар: одна цель за активацию, без прохождения сквозь стены или толпу. Клиент не передаёт жертву, урон или силу толчка. Числовые настройки проверяются на finite и ограничиваются на сервере.

Урон применяется существующим UNGCombatComponent::ApplyDamageToPawn через UNGDamageEffect/HealthSet. WBP_HealthPanel получает изменение HP через существующую подписку; второй системы здоровья нет. Мёртвые/устаревшие Pawn не получают новый урон в следующую жизнь.

Для пережившей попадание текущей цели сервер отменяет активные способности с asset tag Ability.Type.Movement (Jump/Dash/Mantle), затем вызывает LaunchCharacter с заменой скорости и ForceNetUpdate. Отталкивание исполняется CharacterMovement с коллизией и сетевыми коррекциями. Root-motion task деша не должен перекрывать импульс. Это перемещение живого Character, а не симуляция/бросок ragdoll-тела; при смертельном ударе corpse не запускается и DisabledMovement не снимается. Направление толчка вычисляется от атакующего по плоскости gravity цели, подъём — против её gravity.

Настройки GA_Punch → Class Defaults:

| Поле | Значение |
|---|---|
| Damage | 20 |
| Reach | 140 см движения центра сферы; эффективная дальность также включает HitRadius и капсулу жертвы |
| HitRadius | 28 см |
| Windup | 0.12 секунды до серверной проверки |
| KnockbackSpeed | 650 см/с |
| KnockbackLift | 140 см/с |
| PunchMontage | Опциональный монтаж, по умолчанию не назначен |

GE_PunchCooldown по умолчанию длится 0.55 секунды и выдаёт Cooldown.Combat.Punch на ASC PlayerState. EndAbility, повторный клик и переэкипировка не удаляют этот эффект. Даже промах расходует cooldown. Время начала удара привязано к серверу, без client prediction и rewind.

CombatComponent добавляет read-only OnPunchStarted(Direction) и OnPunchHit(Target, ImpactPoint), доставляемые unreliable multicast для косметики. Назначенный PunchMontage проигрывается через существующий mesh/AnimBP на клиентах; полноценная анимация кулаков не включена. Для UI/FX можно подписаться в Character Blueprint на Combat delegates. Cosmetics/AnimNotify не применяют урон и не определяют серверное попадание; при добавлении монтажа настраивать Windup отдельно и использовать подходящий skeleton/ALS slot. Не заменять AnimBP.

Проверка этого прохода: полная UBT NEXTGENONEEditor Win64 Development PASS, без compiler diagnostics (Saved/Logs/NGPunchBuild.log). NGSetup -PunchOnly создал и скомпилировал два Blueprint и дополнил Ability Set: exit 0, 0 errors, 10 warnings окружения/ALS Python names (Saved/Logs/NGPunchAssets.log). Карта не загружалась, PIE/автотесты/сетевые тесты не запускались. Runtime, фактический knockback owning client и его коррекции, прерывание mantle/dash, работа настроенного montage и UI остаются непроверенными до апрува.

Изменённые файлы: новые `Source/NEXTGENONE/Gameplay/Abilities/NGPunchAbility.h/.cpp`, `Content/Gameplay/Abilities/GA_Punch.uasset`, `GE_PunchCooldown.uasset`; дополнен `DA_PlayerAbilities.uasset`. Изменены `NGAbilitySystem.h/.cpp`, `NGCombat.h/.cpp`, `NGDashAbility.cpp`, `NGJumpAbility.cpp`, `NGMantleAbility.cpp`, `NGWeaponPickup.cpp`, `NextgenonePlayerState.cpp`, `NextgenonePlayerController.cpp`, `NGSetupCommandlet.cpp` и эти архитектурные/тестовые документы. ALS plugin, AnimBP, WBP_HealthPanel и project defaults не изменялись.

## 20. Перенос Lyra inventory и пистолета/автомата — 2026-11-01

По новому прямому поручению пользователя инвентарь включён в scope этого прохода. Фактический порт и список файлов зафиксированы в [LyraPort.md](../Research/LyraPort.md). Это реализация, не новое архитектурное исследование.

Из локальной Lyra 5.4 перенесены Fast Array inventory/tag stacks, item definition/instance/fragments и equipment-associated GAS ability base, адаптированные к UE 5.8. Инвентарь принадлежит существующему NextgenonePlayerState, реплицируется только владельцу и переживает смену Pawn. CombatComponent управляет текущей экипировкой и grants по handles; GA_Fire/GA_Reload наследуют перенесённый equipment ability base и используют Combat как SourceObject. Existing ASC/HealthSet/owner-avatar lifecycle сохранены.

Поле MagazineAmmo на Combat удалено: магазин/резерв — stat tags item instance. Переключение не создаёт новый предмет и не пополняет магазин. Сервер проверяет наличие предмета у своего владельца, ammo и PlayerState-wide fire limiter. Перезарядка — серверная ability с проверкой сохранённого предмета/текущего avatar на завершении. Старые corpses не могут менять item новой экипировки через запоздалый reload.

Импортированы 42 реальных weapon resource assets Lyra в Content/Weapons, созданы DA_Pistol/DA_Rifle, ID_Pistol/ID_Rifle/ID_TestWeapon, GA_Reload и WBP_InventoryPanel. 1/2 выбирают стартовый пистолет/автомат; ПКМ задаёт aim state и ALS Aiming, ЛКМ — semi/auto fire, R — reload, U — unequip. Rifle/PistolTwoHanded overlays и тот же ALS HeldObjectRoot/AnimBP используются для поз; оружейные fire/reload sequences запускаются на mesh оружия. Персонажные монтажи требуют проверки skeleton/retarget и настройки в definitions, не объявлены автоматически совместимыми с ALS.

Окно I/Esc подключено добавленным Inventory Window component в существующем FSMState_GameRuntime, с backup исходного asset. Используется существующий UIWidgetMounter и PlayerInputSubsystem; без отдельного placeholder — штатный owning-player viewport fallback монтера. При закрытии/выходе из Runtime освобождаются подписки, widget и input state. Кнопки предметов посылают server equip request с проверкой membership. Дизайн панели — простой список, подлежащий пользовательской настройке, не инвентарь с grid/split/drag/drop.

Новых game-framework plugin dependencies нет; добавлены зависимости штатных UE-модулей NetCore/UMG/Slate/SlateCore и editor-only AssetRegistry/UMGEditor. UIModule и ALS plugin не модифицировались, основной GameMode/map/defaults не переключались. Новая готовность подтверждена сборкой и компиляцией ассетов; gameplay/network проверки ожидают отдельного апрува. Полный provenance, ручные действия, ограничения и пути логов — в LyraPort.md.

## 21. Universal interaction and inventory world transfer — 2026-11-01

`UNGInteractionSubsystem` is a world subsystem coordinating authoritative interaction and inventory/world transfers. `UNGInteractionComponent` on the existing Character supplies local focus, read-only UI events and an owned intent RPC. E now sends `Event.Interaction.Request` to the PlayerState ASC; `GA_Interact` executes ServerOnly, commits GAS cost/cooldown and delegates to the subsystem. The legacy pickup input tag is an alias; the default Ability Set contains one interaction grant. `ANGWeaponPickup` and its old ability are compatibility adapters, not separate pickup implementations.

The client supplies only a proposed actor. Authority checks the current controller/Pawn/PlayerState/ASC avatar, alive state, replicated target/interface, bounded distance, view cone and line of sight. Requests carry the expected Pawn; stale-life requests are rejected. The interaction throttle lives on PlayerState and survives Pawn replacement. Target and transfer guards prevent reentrant native pickup; consumed objects become unavailable before inventory observers run. Custom Blueprint Interact implementations must retain authority-only mutation and their own availability checks.

`ANGItemPickup` / `/Game/Gameplay/Interaction/BP_ItemPickup` accepts an inventory ItemClass and StackCount. Visuals come from the definition (PickupMesh/PickupSkeletalMesh/PickupTransform), then weapon definition, then a cube. Fresh placed pickups initialize definition fragments. Dropped objects instead restore a server-owned FNGItemState snapshot (definition, count and every gameplay-tag stat), preserving magazine/reserve and other charges. Inventory remains the existing PlayerState inventory, with owner-only replication.

Drop requests use the owning PlayerController and verify inventory membership on authority. A deferred, unavailable world actor is created first; source count/stats are rechecked before the debit, then the pickup is published and inventory observers notified. Full-stack removal uses the existing equipment-removal callback. Partial drops of stateless stacks are supported by the native API; partial stateful stacks are rejected until an explicit stat-splitting policy exists. The current UI drops the entire stack. Drop placement uses the Character movement gravity direction, a wall sweep and ground trace; placement failure leaves inventory intact. Dropped pickups are stationary, with no physics throw. No persistent save format or chest-container implementation is introduced.

`Interaction` is ECC_GameTraceChannel1, default Block; pickup volumes block this channel only. Bullet and Pawn channels remain ignored by pickups. `INGInteractable` supplies prompt, availability, interaction and local focus hooks. `/Game/Gameplay/Interaction/BP_InteractiveObject` is a replicated switch example: bActive RepNotify + OnStateChanged for presentation and OnInteractedOnServer for authoritative Blueprint logic. Mesh appearance remains designer-owned.

`WBP_InteractionPrompt` is mounted through a component appended to the existing FSMState_GameRuntime. It subscribes to the current Pawn focus and rebinds on possession; the inventory window suppresses focus. No changes to ALS plugin, AnimBP or the main map/default GameMode were made for this feature. Modified assets were backed up under Saved/RefactorBackup/Interaction before migration. Asset authoring commandlet completed with 0 errors and 10 environment/ALS Python warnings; gameplay/network behavior is pending separately approved testing.
## 22. Settings screen and state-owned debug UI — 2026-11-01

FSMState_GameSettings uses WBP_GameSettings (native UNGSettingsPanel) instead of the accidentally assigned HealthPanel. The existing Game FSM in BP_NextgenoneHUD now connects Runtime --Settings--> Settings --Back--> Runtime. Esc is bound only while Runtime is active; the screen emits Back on Esc or button. Runtime state components release inventory, interaction prompt and debug widgets when leaving. The settings screen blocks local move/look input and stops held weapon input; the multiplayer world continues running.

The original ALS debug component Blueprint spawned a HUD on each possession and created its overlay menu directly. BP_NGDebugComponent is a project copy: coloring/material graphs remain, possession-time widget spawning is disconnected, and overlay events route through a native subclass to UNGRuntimeUIStateComponent. That HFSM component owns both ALS_HUD and OverlayStateSwitcher using WidgetMounter, follows possession, and cleans up on Exit/AbortEnter. The Character SCS points to the project copy. ALS plugin files and AnimBP are unchanged. Debug UI is omitted in Shipping. Widget class references are editable soft properties of the state component.

Settings follow Lyra's separation of machine graphics and local preferences, category descriptions and explicit Apply, adapted to the existing UMG/HFSM. UGameUserSettings stores display mode, supported resolution, quality, FPS and VSync. UNGInputPreferences stores look sensitivity, Y inversion and overall volume in GameUserSettings.ini. Look preferences are applied in the project PlayerController; volume is applied to the world audio device on local Controller BeginPlay and on Apply. Back discards uncommitted field edits. Key descriptions document core mappings, without a key-remapping implementation. No CommonUI/GameSettings framework dependency was added.

GCFSMUtilities lacks DLL export for its native TriggerEvent symbol. UI navigation calls the same reflected UFunction using initialized parameter storage and checked properties, leaving the plugin unchanged. Personal settings transitions remain local to the HUD FSM; they do not become replicated gameplay state.

Offline migration: NGSetup -SettingsOnly; backups: Saved/RefactorBackup/Settings. Build evidence: Saved/Logs/NGSettingsBuild.log; asset evidence: Saved/Logs/NGSettingsAssets.log. Actual runtime UI/input/replication behavior remains pending user-approved testing. See Docs/Gameplay/SettingsAndDebugUI.md for limits and the implementation file list.
## 23. Center aim component and interaction capture — 2026-11-01

ANextgenoneCharacter owns UNGAimComponent. It supplies a constrained-viewport center ray, local aim hit/point and OnAimUpdated, with read-only Blueprint getters. The existing Runtime-owned interaction prompt renders its centered dot; appearance/visibility logic stays in Aim. Local traces do not authorize hits or damage, and the existing server eye/muzzle weapon validation remains unchanged.

Interaction acquisition now prefers the exact ray hit, then assists within CaptureRadius (default 45 cm) using nearby interactable overlaps and ranking by off-axis distance. Both camera LOS and existing eye LOS/range/front-cone checks constrain assisted acquisition. MaxDistance is 350 cm by default, hard-clamped at 500 cm; Character.Interaction is the shared local/server range authority, replacing the separate ability range for current Characters. Capturing a target does not bypass GA_Interact, inventory guards, stale-avatar checks or server mutation rules.

Offline UBT build: PASS, exit 0, no compiler diagnostics. NGSetup -AimOnly compiled and saved Character, WBP_InteractionPrompt and GA_Interact: exit 0, 0 errors, 14 ALS/environment warnings. Asset defaults recorded range=350, capture=45, Aim component present, prompt native tick enabled. Logs: Saved/Logs/NGAimBuild.log and NGAimAssets.log. No gameplay tests executed; runtime placement, acquisition feel, UI and network scenarios await separate approval.
## 24. Ballistic combat, ammo items and dedicated connect — 2026-11-01

Ranged damage now comes from server-simulated ANGBallisticProjectile collision rather than instantaneous hitscan. GA_Fire keeps ServerOnly execution and the persistent PlayerState fire limiter. Eye tracing chooses the initial aim point only. Each shot reserves the limiter, prepares bounded pellet actors, spends one magazine round and launches them. Projectiles use swept/substepped UProjectileMovement with a custom acceleration override (gravity-vector snapshot + linear drag), range/lifetime limits, replicated movement and cosmetic client simulation. A separate BallisticProjectile object channel prevents bullet-bullet collisions while preserving world/Pawn blocking. Impact uses the existing GAS damage helper and current-avatar checks. A captured shooter LifeSerial rejects damage attribution after shooter respawn; cosmetic callbacks never apply damage.

Matching reserve ammunition is represented by ID_Ammo_Pistol, ID_Ammo_Rifle and ID_Ammo_Shotgun inventory definitions, not a second weapon reserve pool. The weapon instance retains magazine ammo only. Reload checks the current weapon, quietly consumes matching inventory quantities, updates the magazine, then publishes inventory changes; recursive completion is guarded. Existing reserve tags on old instances are ignored and no persistent save migration is claimed. Starter loadout is granted once per PlayerState. Selection filters weapon definitions, supporting 1/2/3 despite interspersed ammo stacks.

Lyra Shotgun art/mechanical animations and three individual weapon shot SoundWaves were imported. DA_Shotgun uses eight server-spread pellets. BP_BallisticProjectile is extensible; weapon configuration holds projectile class, speed/gravity/drag/spread/pellets, ammo class and sound. No ALS/AnimBP replacement, character montage retarget or new inventory/health framework was introduced.

Dedicated hosting was selected by the user. UNGConnectionSubsystem handles direct IPv4/DNS client travel, last-address persistence, bounded wait and network/travel status. WBP_ServerConnection is nested in the Settings screen and follows existing FSMState_GameSettings lifetime. Steam is intentionally an inactive UI/config placeholder until the project has an App ID. There is no Steam authentication, server discovery or EOS/CommonUser framework integration. Lyra CommonSession's request/status separation was adapted to the existing UI.

NEXTGENONEServer.Target.cs and Windows/Linux launch + source-engine packaging scripts were added. The installed engine marker exists at S:/UE_5.8/Engine/Build/InstalledBuild.txt: a standalone server binary needs a source engine with server target support. No engine switch, cook, packaging, server launch, deployment, firewall modification or gameplay test was performed. Editor build PASS (UBT exit 0); asset commandlet exit 0, 0 errors / 14 ALS/environment warnings. Details and files: Docs/Gameplay/BallisticsAndAmmo.md; operational instructions: Docs/Networking/DedicatedServer.md.
## 25. Bone collision and physical bullet reaction — 2026-11-01

NextgenoneCharacter now owns UNGHitReactionComponent (UPhysicalAnimationComponent derivative). The movement capsule ignores the projectile object channel, and the skeletal mesh blocks it with query/physics enabled. Projectiles therefore hit Physics Asset bodies and carry a BoneName into the existing GAS damage effect. Damage remains authoritative and retains stale-avatar/shooter-life guards; successful damage additionally triggers a bounded impulse response, including a lethal first hit. Already-dead corpses do not receive repeated damage/reaction through this path.

HitReaction multicasts bone-space impact location, bone, direction and bounded weapon impulse. Each machine briefly simulates the affected skeletal branch with physical-animation drives and fades body-specific blend weights back to their saved values. Originally external simulated bodies are not disabled by recovery. Root/pelvis are excluded from partial simulation (torso fallback defaults to spine_01). ALS retains locomotion/capsule and ragdoll ownership; entering ragdoll clears these drives without switching ragdoll bodies off. Exact Chaos poses are cosmetic and not state-replicated. No AnimBP/plugin edits or new health/death system were introduced.

Server-side mesh animation/bones are always updated, URO disabled and dedicated-server mesh physics enabled so body hit detection can follow the pose. This has a CPU cost and requires later scale measurement. PhysicsCore is now an explicit dependency for body-instance operations. WeaponDefinition.BodyImpactImpulse is separate from damage and applies per pellet. Full rig/Physics Asset compatibility and arbitrary-gravity body response remain unverified. Details: Docs/Gameplay/BulletBodyImpact.md.
## 26. Lyra weapon Niagara — 2026-11-01

Seven selected Lyra Niagara systems and their closed content dependencies were imported to /Game/Effects, without source Lyra/ALS modification or a Lyra gameplay framework dependency. Niagara is explicitly enabled and used by the runtime module. WeaponDefinition holds muzzle, shell, moving-projectile tracer, world impact and character impact systems plus shell mesh/socket settings. Existing custom values are preserved. The unused glass effect is available for manual assignment rather than falsely claiming surface-type routing.

Client/listen-host cosmetic helpers spawn trigger-driven FX and bound their lifetime; dedicated servers do not spawn them. Ballistic actors drive tracer segments from actual local movement and stop/clean the trail on impact/EndPlay. Shot and impact RPCs carry a weapon-definition snapshot so in-flight impacts retain their weapon FX after re-equip. Existing cosmetic delegates remain available. FX never apply damage or modify authoritative combat state.

Niagara parameter contracts were inspected: bool triggers, tracer vector arrays vs impact position arrays, typed NiagaraPosition muzzle values. Headless compilation checks active script statuses and allows UpToDateWithWarnings while rejecting Error/Unknown/Dirty. Impact CPU-script LWC diagnostics remain, with successful GPU compute compilation; render readiness is unavailable under NullRHI. No visual/gameplay tests were performed. Details, imported asset list and verification limits: Docs/Gameplay/LyraWeaponVFX.md.
## 27. ALS demo menus removed, diagnostics in Settings — 2026-11-01

The Tab action was confirmed as IMC_Debug.DebugToggleHudAction. Runtime no longer instantiates ALS_HUD or OverlayStateSwitcher and no longer listens for overlay-menu events. Legacy soft references are cleared in the saved Runtime component. Only the existing Esc -> Settings navigation remains there. Original ALS plugin assets are preserved.

The controller uses a project IMC_NextgenoneDebug copy with known ALS demo/debug actions removed; unrelated custom mappings are preserved and gameplay/ability mappings remain unchanged. Diagnostics are now a nested WBP_DebugSettings in the existing Settings screen/state: existing ALS debug view, traces, shapes and layer colors plus read-only current-character information. Switches apply immediately for the current development session and retain ALS's local static-flag semantics. Shipping hides this diagnostics section. No alternate health, weapon-selection or locomotion system was introduced; demo props, debug mesh swaps and slow motion are not gameplay settings.

Offline C++ build PASS and asset commandlet exit 0 (0 errors / 13 existing ALS/environment warnings). No runtime tests executed. Details: Docs/Gameplay/SettingsAndDebugUI.md, latest update.
## State-owned abilities and effects — 2026-11-03

This supersedes the earlier base AbilitySet description. UNGAbilitySet and DA_PlayerAbilities are deleted. UNGCharacterDefinition now contains InputConfig only. PlayerState.StartLife owns ASC Owner/Avatar initialization and once-per-life health initialization, but grants no default character abilities.

Each UNGControlStateDefinition contains Abilities (FNGAbilityGrant), Effects, MappingContexts and gameplay-input policy. The existing NGCharacterStateResources gives/removes state-owned specs/effects using handles. FNGAbilityGrant carries class, input tag, level and bActivateOnEnter. After acquiring all resources and registering the state scope, the adapter activates flagged grants; failed activation rolls back entry. Repeated scope refresh does not duplicate grants/entry activation. Temporary effects require Duration/Infinite and no aggregated stacking. Initialization health remains outside Alive entry so switching states does not heal.

Alive directly owns Jump, Dash, Mantle, Interact and Punch with their previous input tags/levels. Dead directly owns NGDeathAbility with no InputTag and bActivateOnEnter=true. The dedicated NGCharacterDeathStateComponent is deleted; no special death GAS event or base grant is needed. Health requests the FSM transition. GA_Death only activates if its spec belongs to a currently registered state scope of the correct Avatar, with the Dead gameplay-effect tag present. State Exit/Unbind owns cleanup; ability completion does not clear other state grants.

Existing equipment ownership remains in Combat: weapon definitions still choose Fire/Reload classes, now granted directly through FNGAbilityGrant without constructing an AbilitySet. The server fire-rate limiter remains on PlayerState and is not reset by state/equipment grant churn.

Early UnPossess/ASC handoff uses generic state-resource readiness: the old binding survives the pending transition and resource entry, including all automatic activations. OnResourcesReady resumes PendingLifePawn; old handles are released before changing Avatar. ReadyASC resolves the retained ASC even after PlayerState is removed from the old Pawn. An unpossessed ordinary state does not reacquire grants through Refresh. Callbacks during cancellation use extracted handle arrays to avoid reentrant cleanup. A failed/stopped graph can leave handoff waiting; this is an authoring error, not a reason to bypass FSM.

Pawn death identity comes from its replicated GCFSM state and ControlStateDefinition.StateTag; no Character bDead, bDeathAbilityActivated or ragdoll-phase flag remains. Dead's Infinite NGDeadStateEffect grants State.Dead on the currently attached ASC. The old corpse retains its FSM identity after that ASC moves to a new life. GA_Death executes the definition's PresentationCue; NGDeathRagdollCue uses the ALS base class and rejects a different source Avatar. Proxies restore the configured local cue on state entry/BeginPlay, including late relevance, without depending on the former PlayerState. Body motion/pose is local, without server anchor or physics correction. ALS plugin/AnimBP remain unchanged. Presentation is immediate; a future delayed death montage needs explicit FSM phases rather than another phase flag.

Death lifecycle refactor verification: full offline NEXTGENONEEditor Win64 Development PASS, UBT exit 0 (Saved/Logs/AgentBuild.log). Targeted NGSetup -StateLifecycleOnly authoring exit 0, 0 errors / 11 ALS/environment warnings (Saved/Logs/DeathLifecycleAssets.log). Alive/Dead/Pawn Blueprints compiled and saved; InputConfig force-resaved retaining 13 native and 3 GAS routes. Alive: State.Alive, 5 abilities, 1 IMC; Dead: State.Dead, 1 ability, 1 effect, no IMC, native ragdoll cue. Backup: Saved/DeathLifecycleRefactor/20261103-200722/. Gameplay/network behavior still requires separate approval and verification.

## Input routes separated from state IMC — 2026-11-03

UNGGameplayInputConfig now contains only NativeInputActions/AbilityInputActions and tag lookup; MappingContexts and StateMappingContexts are removed. FNGInputMappingContextEntry lives with ControlStateDefinition, in the same GameplayFoundation script namespace (no redirect needed). Pawn input adapters only bind actions and observe the existing UI input policy, without IMC acquisition/release hooks or context handles. Control-state resources remain the sole gameplay IMC owner.

ControlStateDefinition.ValidateInputMappings compares actions in this state's IMC against the actual Pawn's InputConfig at Enter. A missing Native/GAS route rejects entry through the existing failure/rollback path. InputConfig's independent editor validation still checks tags, references, duplicate routes and GAS Boolean types, but cannot validate a global IMC catalog. States need not map every route of a Pawn; native handlers and parent states can provide other activation paths. No extra InputConfig reference or duplicate catalog was added to state assets.

Offline NEXTGENONEEditor build PASS, UBT exit 0. Reflected InputConfig no longer exposes either context array; authoring read-back retains 13 native and 3 ability routes, Alive IMC_NG_Character priority 10, Dead no gameplay IMC. No gameplay or multiplayer tests run.
