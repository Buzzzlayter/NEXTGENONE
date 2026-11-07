# Исследование: упрощение текущего функционала и границы Equipment

Дата: 2026-11-07. Проверенная база: main `b3e7b10`.
Рабочая копия обновлена fast-forward с origin/main. Это предложение, не реализация.
Прочитаны актуальные Inventory, item activation, Combat/weapon abilities, Character presentation,
Character FSM/resources, PlayerState lifecycle, UI inventory и профильные документы.
Сборки, PIE, сетевые прогоны, профилирование и инспекция Blueprint-графов не выполнялись.
Авторинг и результаты прежних сессий из Docs не выдаются за проверки этого исследования.

## Вывод

Упрощать нужно число владельцев и вариантов исполнения одной операции, а не число проверок authority.
Крупнейшая обоснованная граница — отделить управление экипировкой от оружейного исполнения Combat.
Сначала определить контракт и выполнить перенос существующего одного оружия без изменения поведения.
Броню, несколько рук, конфликты слотов и модификаторы добавлять только под конкретный сценарий.

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
| PlayerState.Inventory | Владение item instances, Count/StatStacks, размещение и перенос; не знает WeaponDefinition |
| Pawn.Equipment (предлагается) | Активные equipment records на текущем Avatar, generation/revision, принадлежащие grants/effects и lifecycle visuals |
| Pawn.Combat | Оружейные запросы, проверка shot view, подготовка/исполнение выстрела и reload; читает активную weapon record |
| GAS abilities | Запуск действия и его время жизни, cancel/commit; предметная ActivationAbility маршрутизирует запрос к нужному механизму |
| Character/ALS presentation | Применение рассчитанного appearance к мешу/overlay; не владеет предметом и grants |
| UI | Snapshot инвентаря и presentation-проекция equipped; не принимает игровых решений |

Выбор Pawn для runtime Equipment соответствует нынешнему Combat lifecycle: grants/visuals действуют
на текущем теле. Inventory и ASC остаются на PlayerState. Автоматическое восстановление надетой брони
после respawn — отдельная политика; если она потребуется, постоянное назначение хранится на PlayerState,
а runtime record материализуется для нового Avatar. Не поддерживать две независимо изменяемые записи.

### Структура записи и данные

Минимальная runtime record: Item, EquipmentDefinition, Avatar/generation, revision, принадлежащие handles.
Для первого этапа достаточно одного активного предмета в руках; не нужен UObject-instance на каждую запись,
если он не несёт собственного поведения/репликации. Перечень записей и keys слотов понадобятся для брони.

Предлагаемый EquipmentDefinition — только общий контракт: совместимое место установки, grants/effects,
описание visual attachment. WeaponDefinition расширяет его оружейными данными. ALS overlay остаётся
проектным appearance-параметром, а не частью общего Inventory. Не разбивать каждое число на отдельный fragment.
Переезд сериализуемых native полей и типов требует отдельного плана Core Redirect/миграции assets.

Action-слот — доступ к предмету, Equipment-слот — установленное снаряжение. Они не тождественны.
Пока сохраняем текущее требование оружия в Action; разрешать экипировку из Storage — изменение дизайна,
а не техническое упрощение. Наличие Equipment fragment само по себе ничего не экипирует.

### Не потерять выбор предмета при unequip

CurrentItem сейчас сохраняется после SetEquippedOnServer(false), чтобы RequestEquip(true) мог
активировать тот же предмет снова. В новой модели нужно явно различать remembered selection
и active record. Убрать bool bEquipped без сохранения selection — потеря работающего поведения.
SelectedItem не даёт права Fire/Reload; permission даёт только валидная active record текущего Avatar.

### SourceObject и отложенные действия

Equipment abilities должны получать устойчивую привязку к конкретной record/предмету и generation,
а не разрешать «текущее оружие Combat» после ожидания. Возможные реализации — существующая запись
с проверяемым ключом либо лёгкий объект привязки, только если GAS SourceObject действительно его требует.
Не выбирать второй UObject ради одного getter; сначала оценить item SourceObject + ключ поколения.
Отдельный item-activation spec уже правильно имеет SourceObject=item — его контракт сохраняется.

Reload, ADS и auto-fire отменяются при выходе соответствующей record; повторная экипировка того же
item не делает старый callback валидным. Снаряд сохраняет shot/source-life snapshot на момент выстрела.
Оружейный cooldown остаётся у постоянного владельца ASC; перенос его в record откроет обход переэкипировкой.

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
| F. Первый второй тип | Один согласованный предмет, например шлем | Добавление не требует switch по типам в Combat/Inventory/UI |
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

Перед этапом C нужно уточнить дизайн: какие предметы остаются назначенными после смерти,
будут ли отдельные слоты брони и допустима ли экипировка непосредственно из Storage.
Для исследования приняты минимальные defaults: текущая семантика одного оружия и Action сохраняется,
новые правила не подразумеваются автоматически.
