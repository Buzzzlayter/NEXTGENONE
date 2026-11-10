# Исследование: упрощение текущего функционала и границы Equipment

Дата: 2026-11-07. Проверенная база: main `b3e7b10`.
Рабочая копия обновлена fast-forward с origin/main. Это предложение, не реализация.
Прочитаны актуальные Inventory, item activation, Combat/weapon abilities, Character presentation,
Character FSM/resources, PlayerState lifecycle, UI inventory и профильные документы.
Сборки, PIE, сетевые прогоны, профилирование и инспекция Blueprint-графов не выполнялись.
Авторинг и результаты прежних сессий из Docs не выдаются за проверки этого исследования.

Уточнение от 2026-11-10: база статического чтения — `101fc13`, джетпак —
`origin/feature/jetpack` (`1904161`). Пользователь исключил bohdon; ориентир для оценки
и адаптации Equipment — Lyra. Цель теперь включает одновременно оружие и джетпак,
а также расширение GAS через способности, теги и эффекты. Ниже обновлены целевые
контракты; runtime-реализация этих изменений ещё не выполнена.
Дополнительное требование пользователя: Back/Feet/Torso — типизированные инвентарные ячейки.
Размещение предметов остаётся в существующем Inventory FastArray; Equipment применяет их к Pawn.

## Вывод

Упрощать нужно число владельцев и вариантов исполнения одной операции, а не число проверок authority.
Крупнейшая обоснованная граница — отделить управление экипировкой от оружейного исполнения Combat.
Сначала определить контракт для независимых Hands/Back и перенести существующее оружие,
затем подключить джетпак через тот же lifecycle. Броню и двуручность добавлять под конкретный сценарий.
Упрощения Inventory из раздела 1 — отдельная работа, не обязательная предпосылка Equipment.

Это не предложение импортировать Lyra EquipmentManager целиком, менять ALS или строить общий framework.
Конкретная необходимость небольшого Equipment-компонента — освободить будущие неоружейные предметы
от зависимости на Combat и дать один lifecycle для их grants/effects/presentation.
Общий механизм сначала оставить в NEXTGENONE; переносить в GameplayFoundation после появления
реального второго потребителя и удаления всех ALS/weapon/project зависимостей.

## Что уже сделано и не требует повторного рефакторинга

- Inventory имеет один FastArray, Layout отдельно от InitialContents, общий FMutationScope,
  AddItemInternal/RemoveQuantityInternal и ConsumeItem/ConsumeItemsByDefinition.
- StatStacks принадлежат instance; общий split/merge policy запрещает деление stateful stacks.
- OnInventoryChanged — одно snapshot invalidation; UI не требует per-request completion.
- Input не знает Combat/Inventory; предмет имеет одну ActivationAbility из fragment.
- Fire/Reload implementations вынесены в NGWeaponAbilities.cpp; WeaponDefinition имеет validator.
- Общие теги и NGPlayerContext уже вынесены; FSM failure теперь терминальный для Pawn,
  освобождает barrier смены Avatar и блокирует gameplay.

Следовательно, старые предложения «вынести Fire/Reload», «объединить уведомления» и
«исправить зависание на известном FailEntry» не являются новыми задачами. Их runtime-верификация
остаётся нужна. Отменённые пользователем completion RPC не возвращаем.

## 1. Минимальные упрощения существующего кода

| Сейчас | Предлагаемое изменение | Что обязательно сохранить |
| --- | --- | --- |
| NGInventoryManagerComponent.h: несколько публичных вариантов add с bNotify, deferred add, ручной Notify | После инвентаризации вызовов оставить небольшой публичный набор add/restore/consume/transfer; детали уведомления скрыть. Pickup/drop требуют отдельного commit boundary, поэтому deferred нельзя просто удалить | Pickup доступен только после переноса; drop публикуется после debit; оба участника transfer видны целиком |
| FNGInventoryList знает Manager, проверяет правила, создаёт instance и исполняет fragments; Manager тоже управляет операцией | Выбрать одного координатора мутаций — Manager. FastArray оставить сериализацией/dirty marking; не добавлять третий transaction/service слой | Проверки после fragment callbacks, subobject registration, запрет reentrant mutation |
| UI Refresh для каждого индекса вызывает GetSlotTag/GetItemAtSlot/GetCount с повторным поиском | Один read-only snapshot слотов за обход; переиспользовать его в UI. Без постоянного дублирующего кэша | Стабильный SlotIndex, фильтры групп, пустые/ещё не resolved items |
| ResolvePlayerInventory/GetInventoryPawn/HasCurrentItem/CanActivate повторяют части контекста | Разделить получение текущего Player Avatar и проверку конкретного item snapshot; небольшие функции возвращают проверенные данные | На серверном входе проверка остаётся; перед отложенным commit проверяется снова |
| Medkit вручную хранит subscriptions, effect, item slot/count и Avatar | Сначала сделать идемпотентную локальную cleanup-функцию и компактный snapshot операции. Общую delayed-item базу создавать только при втором действии с тем же lifecycle | Урон, sprint, потеря/перемещение предмета, смерть, FSM failure отменяют использование без расхода |
| Три grants оружия строятся вручную внутри SetEquippedOnServer | Один resolver существующего Fire/Reload/Aim набора и один путь выдачи/снятия принадлежащих handles | Нынешние default Fire/Reload/Aim сохраняются при миграции; ошибки grants не публикуют частичный equip |

Прямой Entries[SlotIndex] не предлагать без доказанного сетевого контракта: порядок FastArray
на клиенте нельзя автоматически приравнивать к логическому индексу. Snapshot проще дополнительной карты,
которую придётся синхронизировать при каждом receive. Для текущих небольших inventories нет замеров,
доказывающих проблему производительности; цель — ясный read API и меньше повторного кода.

## 2. Combat: конкретные причины разделения

Опорные файлы:
- Source/NEXTGENONE/Gameplay/Combat/NGCombat.{h,cpp}
- Source/NEXTGENONE/Gameplay/Abilities/NGGameplayAbilityFromEquipment.cpp
- Source/NEXTGENONE/Gameplay/Abilities/NGWeaponAbilities.cpp
- Source/NEXTGENONE/Gameplay/Abilities/NGItemActivateAbility.cpp
- Source/NEXTGENONE/Gameplay/Character/NextgenoneCharacter.cpp

Combat сейчас одновременно хранит CurrentItem/Equipment, выдаёт WeaponHandles, наблюдает Inventory,
обрабатывает удержание основной атаки, исполняет выстрел/reload и передаёт cosmetics.
GetAssociatedEquipment в базовой equipment ability возвращает именно Combat; GetAssociatedItem
берёт его текущий предмет. Значит, название общее, но контракт оружейный и динамический.
Добавление шлема через этот контракт заставит либо расширять Combat, либо дублировать управление grants.

Equipment.Definition содержит WeaponDefinition; общего equipment definition пока нет.
WeaponDefinition смешивает ballistics/ammo, appearance, ALS overlay и три ability class.
Character.ApplyCombatPresentation читает weapon definition и управляет ALS held object напрямую.
При этом AttachSocket есть в definition, но данный путь использует ALS AttachToHand без этого поля.
Нужно либо поддержать socket в выбранном presentation contract, либо мигрировать неиспользуемую настройку;
не объявлять существующие authored sockets уже работающими.

## 3. Целевое распределение ответственности

| Владелец | Единственная ответственность |
| --- | --- |
| PlayerState.Inventory | Владение item instances, Count/StatStacks, размещение и перенос, включая типизированные equipment-ячейки; не знает WeaponDefinition |
| Pawn.Equipment (предлагается) | Runtime-применение предметов из equipment-ячеек на текущем Avatar, generation/revision, принадлежащие grants/effects и lifecycle visuals |
| Pawn.Combat | Оружейные запросы, проверка shot view, подготовка/исполнение выстрела и reload; читает активную weapon record |
| GAS abilities | Запуск действия и его время жизни, cancel/commit; предметная ActivationAbility маршрутизирует запрос к нужному механизму |
| Character/ALS presentation | Применение рассчитанного appearance к мешу/overlay; не владеет предметом и grants |
| UI | Snapshot инвентаря и presentation-проекция equipped; не принимает игровых решений |

Выбор Pawn для runtime Equipment соответствует нынешнему Combat lifecycle: grants/visuals действуют
на текущем теле. Inventory и ASC остаются на PlayerState. Назначение одежды/устройств задаётся их
размещением в equipment-ячейках Inventory. Runtime record материализуется для текущего Avatar;
это не второй независимо изменяемый контейнер. Сохранение/очистка equipment-ячеек при смерти
и повторное применение на respawn — отдельная политика, не основание дублировать назначение.

### Структура записи и данные

Equipment-компонент владеет единственным набором runtime records, адресуемых типом места.
Для одежды/устройств record ссылается на исходную типизированную ячейку Inventory и её item;
она не владеет ещё одним размещением. Hands для текущего оружия остаётся выбором предмета из Action.
Первый сценарий — независимые Hands и Back; имена тегов уточняются при реализации.
Запись связывает definition, EquipmentInstance, поколение установки, GrantedASC и принадлежащие handles.
Поколение относится к конкретной установке: снятие Back не инвалидирует Hands/reload.
Идентичность жизни Avatar проверяется отдельно. Remembered selection оружия не является активной записью.

Лёгкий EquipmentInstance создаётся заново при каждом надевании и служит устойчивым GAS SourceObject.
Связь с исходным ItemInstance принадлежит этому instance и доступна серверу/владельцу;
общедоступная репликация equipment не раскрывает содержимое приватного инвентаря.
Instance не копирует Count/StatStacks: magazine и будущий запас топлива сохраняются в исходном item.
Handles принадлежат записи менеджера; instance не создаёт второй независимый grant/revoke путь.

Предлагаемый EquipmentDefinition — только общий контракт: совместимое место установки, grants/effects,
описание visual attachment. WeaponDefinition расширяет его оружейными данными. ALS overlay остаётся
проектным appearance-параметром, а не частью общего Inventory. Не разбивать каждое число на отдельный fragment.
Переезд сериализуемых native полей и типов требует отдельного плана Core Redirect/миграции assets.

Action-ячейка предоставляет быстрый доступ; типизированная equipment-ячейка задаёт ношение предмета.
Оба вида ячеек принадлежат одному Inventory FastArray со стабильными индексами.
Для оружия сохраняется существующее правило выбора из Action; отдельная инвентарная ячейка
для оружия в руках не вводится этим пакетом. Джетпак применяется только из equipment-ячейки Back.
Наличие Equipment fragment у предмета в Storage само по себе не даёт GAS/визуал.

### Места установки и настройка обычных предметов

Типизированные инвентарные ячейки задаются в существующем UNGInventoryLayoutDefinition:
FNGInventorySlotDefinition.SlotTag определяет тип, AcceptedItems фильтрует ItemTags через GameplayTagQuery.
Примеры будущих тегов: Inventory.Slot.Equipment.Back, Inventory.Slot.Equipment.Feet,
Inventory.Slot.Equipment.Torso. Это предложения имён, не уже объявленные native-теги.
EquipmentDefinition задаёт требуемый тип инвентарной ячейки. Не добавлять параллельный Equipment Layout
с независимым списком предметов или вручную синхронизируемое поле назначения.

Inventory на сервере проверяет совместимость переноса, Equipment проверяет definition/ресурсы и
текущее применение к Avatar. Название предмета, mesh и класс способности не определяют место автоматически.
Проектный серверный обработчик переноса координирует equipment-preflight для drag и активации;
общий GameplayFoundation Inventory не получает зависимости на EquipmentDefinition, GAS-правила предмета или ALS.
Слоты расширяются данными, без enum/switch по видам одежды в Combat/Inventory/UI.
Weapon-specific правило Action остаётся проверкой оружия, а не общим требованием ко всем предметам.

Hands/Back — первый реализуемый сценарий; тот же контракт должен допускать Feet и Torso.
Сапоги и футболка одновременно занимают разные места. Вторые сапоги заменяют содержимое Feet
через общий equip lifecycle; прежний Torso/Back при этом не меняется. Предметы, занимающие
несколько мест, и слои одежды с конфликтами требуют отдельной политики, не подразумеваются этим примером.

Путь настройки: ItemDefinition → Equipment fragment → EquipmentDefinition;
Item Actions назначает общую ActivationAbility экипировки. Она инициирует проверяемый перенос
в соответствующую inventory-ячейку. Перетаскивание в неё использует тот же серверный путь переноса.
После успешного размещения Equipment применяет предмет; выход из ячейки снимает его ресурсы.
Не выполнять перенос повторно из InventoryChanged и не предоставлять обход серверных правил через drag.
До commit проверяются правила ячейки и обязательные equipment-ресурсы; невалидный запрос сохраняет
предыдущее размещение. Ошибка после commit очищает runtime-ресурсы и диагностируется;
она не обещает автоматического отката уже совершённых GAS-действий.

При замене старый предмет переносится в допустимую исходную/свободную inventory-ячейку по
общим правилам swap/transfer. Если допустимого размещения нет, замена отклоняется до commit;
не удалять старый предмет и не выбрасывать его автоматически. Снятие также требует допустимой
ячейки назначения либо явной операции drop/transfer. Источник истины размещения остаётся один.
Отдельные способности экипировки сапог и футболки не нужны. Специализированные способности
требуются для собственного поведения, например тяги джетпака. Наборы GAS и визуал могут отсутствовать.

Ниже пример целевой настройки, а не уже существующие классы/ассеты/поля Editor:

| Настройка | Сапоги | Футболка | Джетпак |
| --- | --- | --- | --- |
| ItemDefinition | ID_Boots | ID_TShirt | ID_Jetpack |
| ItemTags | Item.Clothing.Boots | Item.Clothing.Shirt | Item.Device.Jetpack |
| Equipment fragment | ED_Boots | ED_TShirt | ED_Jetpack |
| Требуемый тип inventory-ячейки | Inventory.Slot.Equipment.Feet | Inventory.Slot.Equipment.Torso | Inventory.Slot.Equipment.Back |
| Фильтр места | Принимает Item.Clothing.Boots | Принимает Item.Clothing.Shirt | Принимает Item.Device.Jetpack |
| Представление | Модульный Skeletal Mesh обуви | Модульный Skeletal Mesh футболки | Mesh на сокете спины |
| GAS | Необязательные GE/grants | Необязательные GE/grants | Способность тяги, GE тега ношения, дополнительные GE/grants по definition |

Логическое место, visual attachment и GAS target — разные настройки. GE применяются к существующему
ASC PlayerState, способности действуют на текущий Avatar; место Feet само по себе не изменяет скорость.
Новые характеристики при необходимости добавляются в GAS отдельно от оборудования.

### Контракт presentation для одежды и устройств

Описание visual должно различать как минимум два способа представления:

- Socket attachment: mesh/actor, сокет и transform для джетпака, оружия или инструмента.
- Modular skeletal part: Skeletal Mesh и привязка к позе основного меша персонажа для обуви/одежды.
  Для совместимого скелета возможен Leader Pose; оборудование не заменяет ALS AnimBP.
  Произвольная одежда с другим скелетом требует отдельной настройки, не считается автоматически совместимой.

Логический Torso не является именем socket. Деформируемая футболка следует анимации тела;
простого прикрепления к spine-сокету недостаточно. Смена/удаление visual относится к конкретной записи;
снятие Feet не удаляет Torso/Back. Источник appearance — реплицируемое equipment-состояние,
доступное owner/proxy/late join без чтения чужого ItemInstance. Выбор ALS overlay остаётся
в одном проектном presentation-пути; надевание одежды не сбрасывает оружейный overlay.
Загрузка ассета и поздние visual callbacks проверяют instance/поколение и не восстанавливают снятую вещь.

Обычный новый предмет с уже поддерживаемым способом представления настраивается ассетами.
Новый C++ нужен для нового поведения или нового способа отображения. Контракт модульной одежды
учитывается при проектировании; её runtime-реализация и authoring конкретных моделей идут отдельным пакетом.

### Не потерять выбор предмета при unequip

CurrentItem сейчас сохраняется после SetEquippedOnServer(false), чтобы RequestEquip(true) мог
активировать тот же предмет снова. В новой модели нужно явно различать remembered selection
и active record. Убрать bool bEquipped без сохранения selection — потеря работающего поведения.
SelectedItem не даёт права Fire/Reload; permission даёт только валидная active record текущего Avatar.

### SourceObject и отложенные действия

Equipment abilities должны получать устойчивую привязку к конкретной record/предмету и generation,
а не разрешать «текущее оружие Combat» после ожидания. Целевой SourceObject — конкретный EquipmentInstance,
как в Lyra. GetAssociatedItem разрешает исходный предмет через этот instance и проверяет актуальность записи.
Отдельный item-activation spec уже правильно имеет SourceObject=item — его контракт сохраняется.

Reload, ADS и auto-fire отменяются при выходе соответствующей record; повторная экипировка того же
item не делает старый callback валидным. Снаряд сохраняет shot/source-life snapshot на момент выстрела.
Оружейный cooldown остаётся у постоянного владельца ASC; перенос его в record откроет обход переэкипировкой.

### Контракт расширения GAS

Definition описывает наборы grants/effects данными, по образцу Lyra Ability Sets.
Использовать существующие FNGAbilityGrant/FNGGrantedHandles и общий путь выдачи/снятия;
не импортировать Lyra ASC и не дублировать FSM lifecycle. Выделение переиспользуемого
AbilitySet asset допустимо для этих наборов; перевод Character FSM на него — отдельная задача.

| Возможность | Контракт |
| --- | --- |
| Выдать способность | Отдельный spec с InputTag/Level и SourceObject=EquipmentInstance; выдача сама по себе не запускает действие |
| Запустить при экипировке | Явная политика активации после подготовки источника, всех grants и необходимых effects; NetExecutionPolicy определяет исполняющую сторону |
| Добавить ASC-теги | Infinite GE с granted tags на время ношения, снимаемый по принадлежащему записи handle |
| Изменить атрибуты | GE на существующем ASC; модификаторы действуют по правилам GAS, без второй системы характеристик |
| Реагировать на событие | Выданная ability использует GAS triggers/tasks; её callbacks и ресурсы завершаются при снятии |
| Снять экипировку | Инвалидировать запись, отменить собственные abilities, снять только свои specs/effects и визуал |

ItemTags, SlotTag и InputTag не добавляются в owned tags ASC автоматически.
Для джетпака различаются тег наличия надетого устройства (условный Equipment.Jetpack)
и существующий State.Action.Jetpack, означающий активную тягу. Первый не разрешает тягу самостоятельно.

Разные предметы могут давать один ASC-тег. Снятие одного сохраняет вклад другого.
GE разных записей не должны объединяться в один снимаемый ресурс без явного контракта stacking;
для независимых while-equipped эффектов использовать отдельные active effects без объединения stacks.
Указание SourceObject само по себе не изолирует AggregateBySource: источником stacking является ASC.
Суммирование/приоритет атрибутных модификаторов задаётся средствами GAS.

Все handles нескольких наборов накапливаются в принадлежащей записи коллекции.
Снятие работает с захваченным GrantedASC, не с заново найденным ASC другого Avatar.
Конфликтующие command InputTags требуют явного правила: первый пакет отклоняет неоднозначный
ввод между одновременно активными grants; скрытый выбор первого spec или запуск двух команд не допускается.
Это не запрещает одинаковые пассивные классы у разных источников без конфликтующего ввода.

While-equipped effects — обратимые по снятию ресурсы. Instant GE, периодический расход/лечение
и уже совершённые действия не откатываются удалением handle. Разовое действие при надевании
оформляется отдельно и не участвует в обещании полного rollback. Новые AttributeSet-классы
на каждом equip не создаются автоматически; HealthSet остаётся единственным на PlayerState.

Автоактивация не должна происходить из OnGiveAbility до готовности источника и всех зависимостей.
Для серверной пассивной способности возможен запуск после установки; для LocalPredicted/LocalOnly
нужен запуск на владельце после готовности specs и instance, с проверкой текущего Avatar/поколения.
Нельзя безусловно вызвать один server TryActivateAbility для всех NetExecutionPolicy.
Отказ обязательной startup ability завершает новую установку с очисткой её ресурсов;
отказ обычной команды по cooldown/стоимости не снимает предмет. Автоматического вечного retry нет.
После каждого GAS callback повторно проверяется, что установка ещё актуальна.
В первом пакете обязательной может быть только серверно исполняемая startup ability:
сервер сам наблюдает её результат. Локальный/predicted startup запускается после readiness
владельца и не является условием подтверждения серверного equip. Расширение до обязательного
клиентского startup требует отдельного контракта; completion RPC этим планом не вводится.

## 4. Порядок equip/unequip и репликация

Новый equip сначала проверяет item, место, definition и набор ресурсов. Нужно явно выбрать
семантику отказа: рекомендуемый контракт — не снимать старый предмет при невалидном новом запросе.
Сохранение старого предмета при отказе — отдельная согласуемая корректировка текущего поведения,
поскольку нынешний EquipItemFromAbility сначала делает unequip, а новую definition валидирует позднее.

После preflight операция заменяет активную запись, выдаёт полный набор handles и только затем
публикует успех/реплицируемый snapshot. При ошибке частичной выдачи снимаются только свои handles.
Unequip инвалидирует generation до callbacks, отменяет свои действия, снимает handles и visuals.
Cleanup идемпотентен; чужие state grants не снимаются. Изменения выбранного/активного предмета
не должны запускать рекурсивную мутацию inventory из его защищённого OnInventoryChanged.

Владелец получает private item identity/ammo; остальные клиенты — достаточный appearance snapshot.
Текущие Equipment и owner-only CurrentItem нельзя слить в один публичный item pointer: чужие item subobjects
могут быть недоступны. Item.bEquipped сохраняется как derived UI projection, изменяемая только Equipment.
Она не разрешает операции и не переносится в FNGItemState. Удалять её можно лишь с заменой общего UI-контракта.

## 5. Оружейный механизм после отделения Equipment

1. Combat получает проверенный weapon context: активный item, definition, Avatar и revision.
   Повторное получение context перед delayed commit обязательно; контекст не должен быть вечным cache.
2. Fire сохраняет цепочку fresh view RPC → server validation → ServerOnly GA_Fire → spawn/debit.
   Один immutable shot snapshot используется для всех pellets и cosmetics, даже если callbacks меняют equipment.
   Это ограничивает сложность reentrancy; не означает, что такая runtime-ошибка уже воспроизведена.
3. PrimaryAttack пока сохраняет LocalOnly press/release и существующий auto-fire timer. Перенос timer в ability
   возможен отдельным небольшим этапом, если это реально сократит два lifecycle в один; сам по себе перенос
   в другой файл не упрощает систему. Отпускание/смерть/UI/смена предмета должны остановить удержание.
4. Reload остаётся способностью времени жизни; magazine в item StatStacks, reserve в Inventory.
   ConsumeItemsByDefinition + credit callback уже общий путь — не создавать второй AmmoInventory.
5. ApplyDamageToPawn используется также кулаком, но расположен как static Combat API.
   При появлении armor damage processing вынести узкую операцию применения урона рядом с Health/GE,
   сохранив проверку intended Pawn. Не превращать её в универсальную damage-service подсистему заранее.
6. ValidateDefinition оставить в Editor и server boundary; кэшировать resolved refs можно в activation context.
   LoadSynchronous в GetReserveAmmo не доказывает повторный дисковый I/O, но делает query потенциальной точкой
   первой блокирующей загрузки. Preload ресурсов перед equip, включая cook-доступность, требует отдельного шага.

Прямое удаление GA_Fire ради меньшего числа вызовов изменит расширяемость настроенного FireAbilityClass.
Поэтому этот маршрут сейчас сохраняется. ADS-required fire, отсутствие lag compensation и нынешний
server anchor снаряда тоже сохраняются; hip-fire, rewind и muzzle sockets — отдельные gameplay-задачи.

## 6. Как расширять до брони и инструментов

- Шлем/броня: общий equip lifecycle + собственные effects/визуал. Combat не классифицирует броню.
  Снижение урона при необходимости реализуется в damage/attribute pipeline, не в UI или pickup.
- Инструмент: тот же equip lifecycle; собственная ability использования вместо weapon Fire/Reload.
  Общее PrimaryUse routing вводится, когда появляется реальный инструмент; сейчас PrimaryAttack оружейный.
- Аптечка остаётся transient item action, не equipment record. Деньги остаются loot без ActivationAbility.
- Двуручность, несколько occupied slots, durability, sockets модификаторов и persistence не входят
  в первый перенос. Для каждого нужен отдельный контракт конфликтов и сохранения состояния.

## 7. Что не сокращать

- Проверки current Avatar, authority, item ownership/slot/count перед commit и после ожидания.
- Equipment revision, source life generation, persistent rate limits, rollback transfer после fragments.
- Различие состояния сервера и presentation клиента, initial item NetGUID resolution.
- Character FSM и UI FSM; terminal failure policy и барьер передачи ASC.
- Ability cancellation, приоритеты input policies, симметричный release handles.
- Redirects и legacy поля до фактической миграции и проверки assets.

FSM/resources усложнены жизненным циклом и сетевыми ролями. Начать с таблицы фаз
Enter → Acquire → Activate → Ready → Suspend/Exit/Failed и маленьких функций
AcquireLocalInput/ReleaseLocalInput, а не механического слияния флагов в один enum.
Эти состояния не все взаимоисключающие: серверные grants и локальный input приобретаются в разное время.
Обобщение FNGGrantedHandles на общий acquire/release — только если устраняет действительный дубль,
не стирает проверку источника и не добавляет общий rollback-протокол HFSM.

## 8. План небольшими пакетами

| Этап | Результат | Критерий |
| --- | --- | --- |
| A. Зафиксировать baseline | Матрица существующего поведения и список Editor assets | Список сценариев ниже согласован; никакой новый gameplay не добавлен |
| B. Inventory API | Один read snapshot, закрытые внутренние мутации, ясные pickup/drop commit boundaries | Нет новых caches/result RPC; прежний split/merge/notify контракт сохранён |
| C. Один владелец equip | Вынесены существующие record/handles/selection в небольшой проектный Equipment | Combat больше не владеет выдачей/снятием grants и правилом установки предмета |
| D. Combat context | Fire/Reload/Aim привязаны к конкретному item/generation; один shot snapshot | Никакой callback старого equip не действует на новый; сетевые gates сохранены |
| E. Presentation | Общее описание attachment, ALS применяется в одном месте | UI/Inventory не знают weapon/ALS; owner/proxy видят корректный visual |
| F. Второй тип — джетпак | Предмет в Back, независимый от оружия в Hands, с GAS grants/effects/тегами | Добавление не требует switch по типам в Combat/Inventory/UI |
| G. Адресный polish | Medkit cleanup, local input acquisition; миграция legacy assets | Каждая абстракция убирает конкретный дубль; нет универсального service слоя |

C–E — один архитектурный переход, но реализуется небольшими ревьюируемыми пакетами с совместимым API.
Старый и новый Equipment не должны одновременно быть authority-владельцами состояния.
Не совмещать перенос с новой анимационной системой, космосом, UI дизайном и новым уроном брони.

### Матрица сохранения поведения (будущие проверки, не выполнены)

| Сценарий | Сохраняемое поведение |
| --- | --- |
| Equip A, unequip, equip снова | Выбранный item сохраняется, magazine не сбрасывается, grants не дублируются |
| A → B при fire/reload/ADS | Старые действия отменены; callbacks не расходуют ammo B |
| Оружие вынесено из Action/drop/transfer | Активная экипировка снимается, item UI-флаг обновляется |
| Reload нескольких stacks | Debit reserve и credit magazine до inventory notification; cancel без расхода |
| Medkit из Storage/Action | Одна action, delayed heal, все прежние причины отмены, корректное списание |
| UIOnly/потеря ввода/смерть | Auto-fire прекращается; нельзя лечить/стрелять новым Avatar старым callback |
| Invalid equip definition/grant | Диагностика, нет частично выданного набора; согласованная семантика старого equip |
| Respawn и поздний снаряд | Inventory/ASC один, старое поколение не повреждает новую жизнь |
| Owner/proxy/late join | Приватный inventory не раскрыт, appearance восстанавливается без private item pointer |
| Stateful split и transfer rollback | Прежние ограничения stats/Count и атомарная видимость сохраняются |

До реализации нужен baseline на текущем commit: статическое чтение не подтверждает работающую игру.
Сборки, Data Validation/cook, Blueprint compilation, затем адресные gameplay/network regression
запускаются только после отдельного разрешения согласно AGENTS.md.

### Как измерять именно упрощение

Текущие ориентиры: NGCombat.cpp 368 строк; PlayerInventory.cpp 263; общий InventoryManager.cpp 553.
Это размер файлов, не измерение сложности и не обещание процента сокращения.
Главные критерии: один владелец active equipment; один grant/revoke путь; неизменные серверные инварианты;
меньше публичных вариантов одной мутации; новый неоружейный предмет без изменений Combat;
нет второй копии ammo/health/item state и нет второго registry объектов.

Критерии расширяемости: новый тип оборудования подключает definition/свои abilities без изменений
Combat/Inventory/UI; Hands и Back сосуществуют; passive GE, ASC-теги и способности принадлежат источнику;
снятие одного не затрагивает другой, повторное надевание не дублирует ресурсы.
Feet/Torso добавляются конфигурацией; сапоги и футболка используют общую equip ability,
а socket attachment и модульная одежда имеют явные разные presentation-настройки.
Матрицу дополнить одинаковым ASC-тегом от двух вещей, несколькими наборами grants,
автоактивацией после готовности, отказом startup ability и снятием во время активной тяги.
Это критерии будущей приёмки, не выполненные проверки.

Перед реализацией остаётся уточнить очистку/сохранение equipment-ячеек после смерти и правила respawn.
Для одежды/устройств Storage — хранение, а активация перемещает в типизированную equipment-ячейку.
Для оружия сохраняется существующее требование Action; отдельные Hands/Back входят в целевую модель.

## 9. Оценка Lyra как источника решений — 2026-11-10

Первоисточники Epic: [Inventory and Equipment](https://dev.epicgames.com/documentation/unreal-engine/lyra-inventory-and-equipment-in-unreal-engine),
[Abilities in Lyra](https://dev.epicgames.com/documentation/unreal-engine/abilities-in-lyra-in-unreal-engine).
Для статического чтения использовано публичное зеркало [johnlogostini/Lyra](https://github.com/johnlogostini/Lyra/tree/7e87d760560a05cd61069f725471d948f4ddb190/Source/LyraGame),
commit 7e87d760 (2023-10-12). Это старый срез, не подтверждение API/поведения Lyra для UE 5.8.4.
Перед переносом C++ сверить нужные API с версией движка проекта.

Подходят: EquipmentDefinition/Instance, SourceObject конкретной установки,
накапливаемые GrantedHandles, серверная выдача Ability Sets и описание attach actors/socket/transform.
Список менеджера допускает несколько instances; ограничение одного выбранного оружия в прочитанном
срезе задаётся QuickBar. Для проекта нужны собственные независимые места Hands/Back.

Адаптировать, а не копировать без изменений:

- В прочитанном QuickBar исходный item задаётся через SetInstigator после EquipItem;
  manager уже выдал abilities, а Lyra OnGiveAbility может запустить OnSpawn.
  Наш источник item/Avatar должен быть готов до любой автоматической активации.
- Lyra AbilitySet в этом срезе логирует невалидные entries и продолжает выдачу;
  наш preflight и политика обязательных ресурсов не должны публиковать частичный успех.
- Теги ношения, правила stacking, конфликты мест/ввода, private item identity,
  ALS presentation и смена жизни требуют проектных контрактов из разделов 3–4.

bohdon исключён по решению пользователя. Оценка Lyra основана на документации и статическом чтении;
сборка, импорт, authoring assets и игровые проверки не выполнялись.
