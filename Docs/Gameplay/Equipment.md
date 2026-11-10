# Equipment / Combat

2026-11-10. База: `origin/main` `a969815`, ветка `refactor/equipment-combat`.
Основание: [TechnicalConsolidation.md](../Architecture/TechnicalConsolidation.md),
Equipment-проход 3–6/F из [плана](../Architecture/TechnicalConsolidationPlan.md).

Статус: C++ и адресная процедура авторинга подготовлены. Выполнен только статический
просмотр кода/diff и сверка использованных API с локальными исходниками UE 5.8.
UBT/Live Coding, запуск редактора/commandlet, Blueprint compilation, Data Validation,
cook, PIE и игровые/сетевые проверки агентом **не выполнялись**. Бинарные ассеты не менялись.
Реализация и сохранение поведения в игре пока не подтверждены сборкой/runtime-приёмкой.

## Согласованные правила

- Inventory и ASC остаются на PlayerState. Runtime Equipment находится на текущем Pawn.
- После смерти предметы, включая содержимое Back/Feet/Torso, остаются в Inventory.
  Ресурсы старого Pawn снимаются. После готовности нового Alive Avatar typed equipment
  применяется заново; оружие автоматически в руки не возвращается.
- `Inventory.Slot.Equipment.Back/Feet/Torso` — обычные стабильные ячейки того же FastArray.
  Новый контейнер, независимое назначение слотов и отдельный Equipment Layout не создаются.
- Hands — активный предмет из Action. Выбор сохраняется при Unequip на текущем Pawn.
  Команда Unequip снимает только Hands; Back при этом не меняется.
- Secondary action устройства/одежды переносит предмет в ячейку `EquipmentDefinition.SlotTag`.
  Если он уже там, действие переносит его в первую свободную подходящую обычную ячейку.
  При отсутствии места операция отклоняется целиком.
- Неподходящий drag не меняет серверный inventory: UI перечитывает прежнее размещение.
  Secondary action и drag проходят одинаковые проверки access/slot/equipment-preflight.
- Внутри player inventory замена использует swap двух ячеек. Если встречный предмет не
  помещается в исходную ячейку, операция отклоняется без изменений. Третья ячейка не ищется.
  Прежний отказ occupied-slot transfer между разными контейнерами сохранён.
  Существующие server access/interaction blockers также сохранены: операция может быть
  отклонена в запрещённом gameplay-состоянии; независимость records их не обходит.
- Отказ preflight сохраняет старое размещение/установку. Ошибка применения после commit
  снимает ресурсы новой установки и диагностируется, но оставляет item в Inventory.
  Уже исполненные GAS-действия не откатываются. Автоматического бесконечного retry нет.

## Владельцы и GAS

Combat не зависит от конкретного Character или PlayerState. Equipment получается как
компонент владельца, здоровье и текущий Avatar — через общий ASC-контракт. Character
передаёт mesh оружия через `SetWeaponMesh` и подписывается на `OnPresentationChanged`
для ALS overlay/held-object presentation. Combat не вызывает методы представления
персонажа и не подписывается на его смерть. Остановкой primary input при смерти/отвязке
управляет Character. Montage использует базовый `ACharacter`, Niagara получает mesh оружия.

| Владелец | Ответственность |
| --- | --- |
| PlayerState | Единственные ASC, HealthSet и Inventory; граница новой жизни |
| Character | Передаёт контекст компонентам, включает и снимает оборудование по lifecycle |
| Item activation ability | Выбирает действие: EquipHands либо перенос в typed/обычную ячейку через существующий Inventory API |
| Equipment component | Применяет размещённые вещи к Pawn: установки, выдача/снятие GAS-ресурсов, visuals и deferred readiness |
| Equipment instance | SourceObject одной установки; owner-only spec handles и локальный учёт startup-попыток |
| Combat | Механизмы выстрела/reload и наблюдение Hands/ammo; получает Inventory от Character |
| Fire/Reload/Aim/Jetpack abilities | Запуск, ожидание и отмена конкретного действия текущей установки |

`UNGEquipmentComponent` хранит установки и их `FNGGrantedHandles`/GrantedASC, наблюдает
Inventory, управляет выдачей/снятием и обновляет derived `Item.bEquipped`.
Он не выбирает inventory-ячейку для secondary action: это правило item activation ability.
Проверки размещения и перенос остаются в прежнем PlayerInventory; транзакционного слоя нет.
Персонаж передаёт ASC, Inventory и attachment mesh через `BindContext`, вызывает
`ActivateEquipment` после готовности Alive и `Shutdown` при смерти, failure, UnPossess
и отвязке контекста. Equipment не включает и не приводит владельца к
`ANextgenoneCharacter`/`ANextgenonePlayerState`, не подписывается на их lifecycle-события
и не получает mesh через класс персонажа. Проверки актуального Pawn/ASC, authority,
здоровья и принадлежности предмета остаются внутри Equipment.
Player ASC публикует нейтральное событие `OnAbilitySpecsReplicated`. Текущая Character FSM
сама подписывается на него и повторяет существующий `OnResourcesReady`, а Character заново
проверяет `IsGameplayReady`. Подписка принадлежит FSM и снимается при suspend, смене ASC
и EndPlay. Character не подписывается на ASC; ASC не знает о FSM или Equipment.
Проверка выполняется до обработки ожидающих GAS-активаций и после неё, обработчик
повторяемый. Appearance на proxy получает attachment target отдельно от готовности
приватных inventory/specs.
При resource failure FSM освобождает свои grants, Character останавливает ввод и
Equipment, а общий `NGPlayerContext` отклоняет игровые операции отказавшего Pawn.
ASC не хранит отдельный флаг отказа или блокировку Avatar и не интерпретирует состояние FSM.
`UNGEquipmentInstance` создаётся заново при каждой установке и служит SourceObject specs/GE.
Его исходный item и копия списка spec handles реплицируются только владельцу.
Definition/slot/generation/installed доступны всем для presentation без чужого inventory.
Startup-попытки хранятся на самой установке и исчезают вместе с ней. Deferred update
компонента завершает отложенную выдачу specs на сервере и local startup на владельце;
GE других источников не запускают эту обработку. Повторный BindContext с теми же данными
не переподписывает компонент и не публикует повторное изменение.

Character также вызывает `Combat.BindInventory(Inventory/nullptr)`. Combat снимает
подписки на Inventory и наблюдаемый item при отвязке, не ожидая уничтожения старого Pawn.
Изменение Back не переподписывает его на тот же Hands item. При смерти Combat останавливает
primary input; снятием установок управляет Character через Equipment. Существующие
Blueprint getters/delegates и командные wrappers Combat сохранены для совместимости;
они не владеют копией состояния Equipment. После CommitAbility item activation повторно
проверяет исходный Avatar, чтобы callback прежней жизни не переносил вещи на новой.

Установки независимы: снятие Back не меняет HandsRevision, reload или источник ADS.
Перед cleanup установка удаляется из актуальных записей, затем снимаются собственные
specs/GE с захваченного ASC. Старый SourceObject не становится валидным при re-equip
того же ItemInstance или смене Avatar. Item-activation specs по-прежнему используют item.

Fire/Reload/Aim наследуют отдельный `UNGGameplayAbilityFromWeapon`: он получает
короткоживущий `FNGWeaponAbilityContext` из конкретного spec, проверяет его SourceObject,
текущий Avatar, definition и принадлежность активной оружейной установке Combat.
Reload не хранит ссылки на Combat или equipment instance: после задержки контекст
заново разрешается по `CurrentSpecHandle`. Снятый spec, прежняя установка и новый Avatar
не разрешают завершение старой перезарядки. Jetpack остаётся на общем equipment-base.
Общий equipment-base возвращает только конкретную установку и исходный item.
Оружейный контекст разрешается отдельно в `UNGGameplayAbilityFromWeapon`.

`UNGEquipmentDefinition` задаёт место, собственные grants/effects, несколько
`UNGEquipmentAbilitySet` и socket visual. `UNGWeaponDefinition` наследуется от него,
сохраняя native class path и прежние Fire/Reload/Aim defaults. Для grants требуется
instanced ability от `UNGGameplayAbilityFromEquipment` с положительным Level.
Повторяющиеся command InputTags внутри набора или с другими активными grants отклоняются.
Одинаковый passive class без InputTag допускается у разных источников.

`FNGEquipmentAbilityGrant.Grant.bActivateOnEnter` означает startup, отдельно от выдачи.
`bRequiredStartup` разрешён для ServerOnly/ServerInitiated. Сначала готовятся source,
все specs и GE; если выдача deferred внутренней блокировкой GAS, завершение установки
откладывается на следующий tick. До этого источник не разрешает activation и не даёт visual.
Отказ TryActivate или cancellation обязательного startup очищает установку. Успешное
нормальное завершение startup снимает только ожидание его результата.
LocalOnly/LocalPredicted startup запускается на владельце после готовности source,
всех specs и source GE; попытка однократная и не подтверждает серверный equip через RPC.

While-equipped GE — Infinite, без Period/Executions и без stacking. Каждая установка
владеет отдельными handles: общий ASC-тег может предоставляться несколькими вещами.
Разовые изменения и периодические действия следует выполнять способностями.
Новые AttributeSets на equip не создаются.

## Combat и presentation

Combat исполняет fire/reload и косметику, наблюдает ammo/aim; authoritative equipment
record, selection и grant/revoke находятся в Equipment. Его существующие Blueprint
getters/delegates/RequestEquip сохранены как совместимые
Hands adapters без отдельного состояния. Удалять их можно только после проверки BP consumers.

Fire сохраняет fresh-view RPC → server validation → ServerOnly GA_Fire, ADS gate,
server projectile anchor и persistent rate gate PlayerState ASC. Перед spawn/debit
проверяется конкретный SourceObject; один захваченный shot snapshot используется для
всех pellets и cosmetics. Projectile source-life защита сохранена.
Reload захватывает установку, заново проверяет её перед расходом и использует прежний
reserve debit + magazine credit до inventory notification. Magazine остаётся в item stats.
Цикл автоогня через `UAbilityTask_WaitDelay` и подписка на смену HandsRevision принадлежат
`UNGPrimaryAttackAbility`, а не Combat. GAS завершает tasks вместе с ability; EndAbility
снимает подписку. Отпускание/UI-блокировка завершают ability
через WaitInputRelease; смена Hands, включая замену из синхронного callback, отменяет
удержание через Equipment.OnEquipmentChanged. Перед каждым повторным запросом проверяется
актуальность установки и Avatar. Back не прерывает очередь Hands.

Character остаётся единственным ALS Hands adapter; AttachSocket теперь применяется реально.
Остальные socket visuals принадлежат конкретной установке; смена Hands не пересоздаёт Back.
В definition явно различены SocketAttachment и ModularSkeletalPart. Runtime модульной одежды
этим пакетом не реализуется: такой режим отклоняется preflight с диагностикой. Он требует
отдельного Leader Pose/skeleton пакета по исходной спецификации. ALS AnimBP не заменён.

## Джетпак

Адресно перенесены ability и ALS-derived movement из `origin/feature/jetpack` `1904161`;
карта, controller-изменения и прочие ассеты той ветки не объединялись.
`UNGJetpackDefinition` задаёт Back, параметры Movement и MaxBurnDuration, native grant
тяги по `Input.Jetpack` и Infinite GE `Equipment.Jetpack`. Активная тяга имеет отдельный
тег `State.Action.Jetpack`. Топливо не добавлено.

Способность тяги теперь equipment ability; выдача из Alive исключается миграцией.
Gameplay IMC по-прежнему принадлежит Alive. Общая equip item ability больше не выдаёт
кратковременный `State.ActivatingItem`: надевание устройства не отменяет ADS, а смена
оружия не выключает тягу другого источника. Medkit сохраняет прежний busy lifecycle.

SavedMove захватывает конкретную установку, её generation, ramp и параметры тяги.
Старый replay не использует параметры нового устройства. Custom network move передаёт
только generation: сервер сверяет его с собственной активной Back-записью и GAS-разрешением.
Acceleration/speed и другие настройки клиент не передаёт. ALS Custom_0 сохраняется,
Custom_1 остаётся намерением тяги. Сетевое поведение ещё требует отдельной проверки.

## Подготовленные ассеты джетпака

В текущем редакторе созданы и сохранены:

- `/Game/Gameplay/Inventory/ID_Jetpack` — класс предмета для содержимого сундука:
  `ID_Jetpack_C`. Стек 1, тег `Item.Equipment.Back`, Equipment fragment с
  `ED_Jetpack` и ItemActions fragment с `NGEquipWeaponItemAbility`.
- `/Game/Gameplay/Equipment/ED_Jetpack` — Back, native ability тяги, wearing effect,
  ограничение непрерывной тяги 4 секунды. Текущий временный визуал —
  `/Engine/BasicShapes/Cylinder`: масштаб `(0.3, 0.3, 0.3)` (размер 30 см), локальное
  смещение `(0, 0, 0)` (центр модели на сокете), AttachSocket — `spine_03`. Модель и её положение
  настраиваются здесь через Mesh и VisualTransform; посадка в игре ещё не проверена.
- `/Game/Gameplay/Input/Configured/IA_NG_Jetpack` и `IMC_NG_Jetpack` — удержание
  J или R3. Маршрут `Input.Jetpack` добавлен в `DA_Input_Character.AbilityInputActions`,
  контекст — в Alive с priority 25. Способность выдаёт только экипировка.

В `DA_InventoryLayout_Player` в конец добавлен Back с фильтром `Item.Equipment.Back`;
прежние 25 слотов и их индексы сохранены. Feet/Torso этим шагом не добавлялись.
Сундуки и стартовое содержимое не изменены: пользователь добавляет `ID_Jetpack` сам.
Secondary action предмета надевает его в Back; повторная активация из Back снимает
его в свободный подходящий слот. Для видимого Back-слота в UI нужен equipment panel,
описанный ниже.

Проверены ссылки и значения через редактор после компиляции `ID_Jetpack`, сохранены
только перечисленные ассеты и три изменённых конфигурации. Это не проверка в игре.
Исходные конфигурации скопированы в `Saved/RefactorBackup/JetpackAssets/20261111_001734/`.
Описанная ниже полная операция `NGSetup -EquipmentOnly` не запускалась и для создания
этого предмета больше не нужна; она дополнительно меняет стартовое содержимое.

## Миграция после пользовательской полной сборки

Рефакторинг перенесён в основную папку проекта; её текущая ветка — `refactor/equipment-combat`.
Открывать/собирать нужно `NEXTGENONE.uproject` из основной папки. Прежний worktree оставлен
на detached HEAD; его исходные правки сохранены в stash и `Saved/BranchTransfer/20261110-204729/`.
Полная сборка и новый процесс редактора нужны из-за новых native типов, наследования
WeaponDefinition и замены класса существующего CharacterMovement subobject.

Подготовлена адресная операция `NGSetup -EquipmentOnly` в `NGEquipmentAssets.cpp`.
Она не запускается автоматически. Её запуск, включая compile трёх BP, требует отдельно
согласованного этапа по AGENTS.md; для offline commandlet редактор должен быть закрыт пользователем.

Операция:

1. Копирует исходные файлы затрагиваемых packages в `Saved/RefactorBackup/Equipment/<timestamp>/`.
2. Пересохраняет DA_Pistol/Rifle/Shotgun/TestWeapon с appearance-полями нового superclass.
   Старый неработавший default AttachSocket `hand_r` мигрирует в прежнюю фактическую ALS
   virtual bone `VB RHS_ik_hand_gun`; иные authored sockets сохраняются и теперь применяются.
3. Создаёт `ED_Jetpack` и `ID_Jetpack`, Equipment fragment и общую activation ability.
   Mesh не выдумывается: в `ED_Jetpack` назначить модель, AttachSocket и VisualTransform.
4. Дописывает Back/Feet/Torso в конец `DA_InventoryLayout_Player`, не меняя прежние индексы.
   Новые фильтры принимают `Item.Equipment.Back/Feet/Torso`; такие же ItemTags назначать предметам.
   Имеющиеся слоты/фильтры повторный запуск не перезаписывает.
5. Добавляет один Jetpack в стартовое Storage без повторной выдачи на respawn.
6. Создаёт IA/IMC Jetpack, маршрут AbilityInputActions и контекст Alive с priority 25.
   Для нового IMC предлагаются J и R3, только если они свободны в существующих Alive contexts.
   Конфликтующая клавиша пропускается с предупреждением. Уже настроенный маршрут сохраняется.
7. Убирает из Alive только grants, наследующие NGJetpackAbility; компилирует и сохраняет
   ID_Jetpack, BP_NextgenonePlayerState, BP_NextgenoneCharacter. Проверяет наличие Equipment
   и нового movement на Pawn CDO. Карта, UI layout, AnimBP и plugin assets не меняются.

Для отображения новых ячеек пользователь настраивает UMG: отдельный panel от
`UNGInventoryPanel` с `SlotFilter=Inventory.Slot.Equipment` (или конкретным Back/Feet/Torso).
Подключить его тем же `UNGInventoryStateComponent` (`Source=Player`) в inventory UI FSM;
он наследует `UUIWidgetMounterComponent` и задаёт InventorySource при Enter/Exit.
Размещение/размер — в существующем UMG layout. Пустой SlotFilter показывает все ячейки.
Текущий Storage-фильтр существующей панели автоматически не расширяется; без equipment
panel предмет после надевания не отображается в Storage, а доступ к снятию из UI требует
этой панели. Редизайн/редактирование UMG в данный C++ пакет не включены.

Четыре PropertyRedirect для Mesh/SkeletalMesh/AttachSocket/VisualTransform были временными.
11 ноября 2026 они удалены вместе с остальными проектными redirects после пересохранения
62 ассетов и успешной загрузки отдельным свежим процессом. Appearance-поля оружия совпали
до и после удаления. Подробности и backup — в [отчёте миграции](../Architecture/CoreRedirectMigration.md).

## Ожидающая приёмка

Все перечисленные сценарии **не проверены в runtime**: equip/unequip/re-equip; A→B во
время fire/reload/ADS; Back toggle при активном Hands; несовместимый drag; заполненный
inventory; swap и отказ несовместимого swap; drop/transfer; смерть/respawn; два источника одного ASC-тега;
несколько ability sets; startup после deferred GiveAbility; отказ/cancel startup;
initial replication, owner/proxy/late join; снятие джетпака во время тяги; generation
network move, prediction/replay и задержки. Матрица исходной спецификации остаётся актуальна.
