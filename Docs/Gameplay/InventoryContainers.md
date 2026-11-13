# Универсальные контейнеры и UI инвентаря

Актуальный сетап личного инвентаря и отдельного состояния Storage: [InteractionUI.md](InteractionUI.md). Источник каждой панели назначает Inventory Window мounter; сама панель не выбирает между своим/внешним контейнером.

## Пакет консолидации — 2026-11-07

Реализация пунктов [TechnicalTODO](../TechnicalTODO.md) сохранена коммитами. Подтверждены только статический просмотр, diff и синтаксис PowerShell runner; сборка C++, Data Validation, cook, Blueprint compilation и игра не запускались.

- Канонический стартовый список — **PlayerState.Inventory → InitialContents**: ItemClass, Count, SlotTag и необязательный SlotIndex (-1 означает первый подходящий слот группы). Оружие и патроны — отдельные записи, без автоматического вычисления резервов. Старые **Combat.StarterItems** и **Inventory.StarterItems** сохранены только для ручной миграции, больше не исполняются. Выдача однократная; при частичном отказе успешные предметы остаются, ошибки логируются, respawn не повторяет выдачу. Каждый Count должен помещаться в один стек; больший резерв задавать несколькими записями.
- Pickup использует один чистый поиск подходящего пустого слота на клиенте/сервере. При auto-equip оружие идёт в свободный Action; если Action заполнен — в Storage без экипировки. Если подходящего Storage тоже нет, pickup остаётся в мире.
- **Character.Interaction → DroppedPickupClass** задаёт класс выброшенного предмета. Native default — NGItemPickup. Для прежнего Blueprint-визуала назначить BP_ItemPickup явно; hard class reference доступна cook. IsDataValid проверяет null/abstract, фактический cook ещё не выполнен.
- Drop/transfer/activation возвращают только bool отправки запроса. Per-request callbacks, ID и completion RPC удалены по решению пользователя. UI получает итоговые данные через репликацию и OnInventoryChanged, без оптимистических мутаций и подтверждения каждой команды.
- Мутации публикуют итоговое уведомление после регистрации subobjects и списаний; повторная мутация тех же контейнеров из синхронного callback отклоняется. **OnInventoryChanged без аргументов** — общая инвалидизация, потребитель читает snapshot. Хранится один dirty-флаг, без per-slot map/delta payload. Старый FNGInventoryChangeMessage сохранён только для существующего Core Redirect до миграции ассетов, runtime его не использует. Поздний OnRep_ItemDef также инвалидирует snapshot. Reload использует общий consume; до списания проверяет весь debit, затем добавляет магазин и уведомляет inventory. Stateful partial consume запрещён, как split.
- Панель сохраняет неизменившиеся item-подписки; одинаковые slot snapshots не вызывают повторный OnSlotUpdated и загрузку иконки. Полный UI snapshot пока формируется; прирост производительности не измерялся.

Схемы DA_InventoryLayout_Player/Box, BP_NextgenonePlayerState и ссылки GameMode/Box созданы ранее через MCP и сохранены. Для уже размещённой коробки проверить наследование Layout: старый экземпляр мог сохранить None. При необходимости заменить его экземпляром настроенного BP с сохранением пользовательских свойств. Этот пакет не пересохраняет карту автоматически.

## Политика разделения и слияния стеков

`Count` — количество в слоте; `StatStacks` — состояние всего item instance (например, заряды), а не каждой единицы Count. Пустые stats допускают split и merge предметов одного класса в пределах MaxStackSize. При непустых stats split и merge запрещены, даже если значения совпадают: их копирование дублирует состояние, а объединение с потерей одного instance теряет состояние. Полный перенос в пустой слот, полный drop и перестановка целых предметов внутри контейнера разрешены. Transfer и drop используют общую политику FNGItemState; автоматического распределения stats между частями нет. Сборка и runtime этой правки не проверены.

## Транспорт инвентаря без зависимости PlayerController

`UNGPlayerInventoryManagerComponent` наследует общий `UNGInventoryManagerComponent`. Это единственный Inventory-компонент на PlayerState: он хранит предметы через базовый класс и обслуживает drop/transfer RPC, сессию внешнего контейнера и события UI через проектный наследник. Общий класс в GameplayFoundation не зависит от NEXTGENONE, ALS или UI. Коробки продолжают использовать общий Manager.

`ANextgenonePlayerState` создаёт проектный наследник вместо базового Manager, сохраняя имя subobject **Inventory**. Размер и правила слотов задаются назначенным Layout. Отдельный PawnInventory удалён. Добавлять компоненты вручную не нужно. Controller и Interaction не содержат инвентарных RPC или сессии.

Оба Manager используют один `UNGInventoryLayoutDefinition` из GameplayFoundation. AddSlots и конструкторное создание слотов удалены. В компоненте назначается **Inventory → Layout**; прямое редактирование runtime Entries убрано. InitializeInventory вызывается на сервере из InitializeComponent, а также перед ранней выдачей предметов. Клиенты получают готовые записи FastArray и не создают слоты из Layout. Ссылка на Layout отдельно не реплицируется.

## Data Asset схемы слотов

В `/Game/Gameplay/Inventory/Layouts` созданы два Data Asset класса **NGInventoryLayoutDefinition**:

- `DA_InventoryLayout_Player`: Slots из 25 записей — первые 20 с SlotTag=Inventory.Slot.Storage, остальные 5 с Inventory.Slot.Action.
- `DA_InventoryLayout_Box`: Slots из 20 записей с SlotTag=Inventory.Slot.Storage.

Каждая запись содержит только SlotTag и AcceptedItems (GameplayTagQuery). Пустой AcceptedItems разрешает любой предмет. Позиция записи в Slots задаёт SlotIndex; один и тот же Layout можно назначать нескольким независимым контейнерам. Предметов, количества стека и ItemInstance в Layout нет. Новые native-типы и настройки доступны после полной сборки и перезапуска Editor; агент сборку не выполняет.

Для игрока назначить Player Layout на Inventory в Blueprint-наследнике существующего ANextgenonePlayerState. Если такого BP ещё нет, создать `BP_NextgenonePlayerState` и выбрать его в BP_NextgenoneGameMode → Player State Class. Это тот же native PlayerState и тот же subobject Inventory. Для коробки назначить Box Layout на Inventory в BP_InventoryBox. Изменения отдельных слотов конкретной коробки задавать отдельным Layout asset.

Layout применяется один раз за жизнь компонента; смена Pawn не пересоздаёт инвентарь PlayerState. Приватный bSlotsInitialized отличает корректную пустую схему от ещё не выполненной инициализации. Без Layout выдаётся Warning и InitializeInventory возвращает false; стартовая выдача игрока при этом не отмечается завершённой. Назначенный Layout с пустым Slots создаёт корректный контейнер нулевой вместимости. Изменять Layout у работающего контейнера и динамически изменять число слотов этим API нельзя.

Старые Blueprint overrides InventoryList.Entries больше не являются конфигурацией: InventoryList теперь Transient + Replicated, записи создаются из Layout в новой сессии. Их правила вручную перенести в Slots нужного asset. Существующие пользовательские ассеты и карта не мигрируются кодом. StarterItems/InitialContents остаются отдельными настройками и работают после создания слотов; позиции InitialContents должны входить в диапазон Slots.

Runtime API Manager и его проектного наследника оставлен C++ без BlueprintCallable/BlueprintPure. InventoryUIRequested/OnInventoryRequested и ToggleInventoryUI удалены: внешнее взаимодействие передаётся через общий контекст Interaction и UI Router, свой инвентарь открывается напрямую UI FSM. Нужные Blueprint-точки пользователь помечает явно. RPC/RepNotify и callback OnPawnSet сохраняют обязательные UFUNCTION, настройки остаются в Details. InventoryOperationInterval задаётся на **PlayerState.Inventory**. Blueprint/assets агент не менял.

Сессия хранится на Manager и привязана к Pawn, открывшему контейнер. OnPawnSet и EndPlay очищают её. Запросы старого Pawn отклоняются после смены владения. Проверки ExpectedPawn, доступ/дистанция/LOS и ожидаемые item pointers сохранены. Drop/transfer rate limits и однократность стартовой выдачи — приватное состояние PlayerInventory Manager, который живёт на PlayerState и переживает смену Pawn. Character при инициализации вызывает GrantStarterInventory; Combat.BindInventory только подписывается на изменения.

Флаги bInventoryUIOpen/bSettingsUIOpen удалены. UI FSM блокирует Gameplay общей политикой UPlayerInputSubsystem. Прицел и интеракция проверяют IsGameplayInputBlocked и не знают названий экранов. Settings mounter уже использует эту политику. Старые Blueprint-узлы флагов удалить, в нужных UI-состояниях включить bBlockGameplay. Сборка/runtime не проверены.

## Стартовая Action-панель и ввод 1–5

Стартовые оружейные предметы из Combat.StarterItems выдаются в свободные Action-ячейки в порядке списка; остальные предметы и резервные патроны — в Storage. Используется AddItemDefinitionToSlot, который сохраняет инициализацию item-фрагментов (включая магазин). При нехватке подходящих ячеек предмет не выдаётся, пишется warning. Существующие предметы не дублируются. Набор выдаётся один раз на PlayerState, поэтому изменение стартового распределения требует новой игровой сессии. Pickup/обычный перенос сохраняют прежнюю логику; это изменение только стартового набора.

В /Game/Gameplay/Input/ActionBar созданы пять Boolean IA_NG_ActionSlot1..5 и IMC_NG_ActionBar. Клавиши One..Five назначены только в этом IMC, в C++ физических клавиш нет. В DA_Input_Character.NativeInputActions каждому IA назначен Input.ActionBar.Slot1..5. Старый Native Input.Equipment.SelectSlot и его mappings из IMC_NG_Character удалены; сам старый asset IA_NG_SelectSlot не удалялся. Существующие gamepad DPad_Left/Up/Right перенесены на слоты 1/2/3; назначения gamepad для 4/5 пользователь выбирает в IMC.

DA_CharacterState_Alive включает IMC_NG_ActionBar с priority 20 рядом с IMC_NG_Character priority 10. Контексты проходят общий StateResources/PlayerInputSubsystem lifecycle и блокируются UI-политикой Gameplay. Кнопка N выбирает N-ю Action-ячейку; пустая ячейка ничего не экипирует. Пока исполнение содержимого Action-панели поддерживает существующую оружейную экипировку, автоматическое применение consumable не реализовано.

Ассеты настроены/сохранены через Editor MCP, read-back маршрутов и mappings выполнен. Backup исходных InputConfig/IMC/Alive: Saved/ActionBarSetup/20261104-201219. Сборка новых C++ изменений и игровые проверки не запускались; выполняет пользователь.

## Подготовка Blueprint-ассетов

`/Game/Gameplay/Inventory/BP_InventoryBox` создан агентом через Editor MCP, Blueprint скомпилирован и сохранён без игрового запуска. Стартовые предметы в BP_NextgenoneCharacter.Combat уже содержат три ammo-класса и Pistol/Rifle/Shotgun; Equipment-фрагмент ID_Pistol ссылается на DA_Pistol. Нативный PlayerState имеет 20 Storage + 5 Action. Это проверка конфигурации, не наличия предметов в игровой сессии.

Скрипт `Scripts/Editor/SetupInventoryWidgets.py` подготовлен для запуска в консоли Editor командой `py "C:/Users/buzzz/OneDrive/Desktop/NEXTGENONE/Scripts/Editor/SetupInventoryWidgets.py"`. Он создаёт WBP_InventorySlot, WBP_InventorySlotsList, WBP_ActionSlotsList, обновляет WBP_InventoryPanel и создаёт WBP_ActionPanel/WBP_ContainerInventory в /Game/Gameplay/UI. Назначает native родителей, btn_Root/WrapBox/SlotsList, SlotClass и фильтры Storage/Action. Только структурные виджеты; OnSlotUpdated, иконки/тексты/drag-decorator и оформление делает пользователь. Layout/FSM мounting не меняется: существующий InventoryWindow уже ссылается на WBP_InventoryPanel; остальные панели назначить обычным mounter в нужных состояниях.

Перед изменением существующие assets копируются на диск в Saved/InventoryWidgetSetup/<timestamp> и дублируются в /Game/Developers/InventoryWidgetBackups/<timestamp>. Результат пишется в report.json. Скрипт агентом ещё не выполнен: UMGToolSet не загрузился, инструменты управления окнами недоступны; Python Remote Execution выключен и его включение было отклонено автоматической проверкой безопасности. Запуск локального скрипта пользователем ожидается. PIE/сеть/UBT не запускались.

## Модули и данные

Один `UNGInventoryManagerComponent` используется для персонажа, коробки и будущих сумок. Ядро перенесено в `Source/GameplayFoundation/{Public,Private}/Inventory`: ItemDefinition, ItemFragment, ItemInstance, SetStats, FastArray и stat stacks. Оно не зависит от NEXTGENONE, ALS, UI или оружейных классов. `NetCore` — public dependency, поскольку FastArray присутствует в публичных заголовках.

UI в `Source/UIModule/{Public,Private}/Inventory` получает только `FUIInventorySlotData` и интерфейс операций, не знает классов предметов/Combat/PlayerState. `NEXTGENONE` содержит адаптер `UNGInventoryPanel`, оружие, pickup и `ANGInventoryBox`. UIModule — public dependency NEXTGENONE, так как экспортируемый Panel наследует общий UI-класс. Обратных зависимостей нет.

ItemDefinition содержит имя, иконку, ItemTags, MaxStackSize, pickup-визуал и фрагменты. Оборудование — `UNGInventoryFragment_Equipment.Definition` типа UDataAsset. Только Combat приводит этот asset к WeaponDefinition через GetWeaponDefinition. Класс Inventory не проверяет, является ли предмет оружием.

## Ячейки

Единственный FastArray InventoryList.Entries хранит все слоты, включая пустые. Одна запись FNGInventoryEntry содержит SlotIndex, SlotTag, AcceptedItems, Instance и StackCount. Вместимость — Entries.Num(); отдельные SlotDefinitions/MaxEntries и OnRep_SlotDefinitions удалены. Пустой слот имеет Instance=null и StackCount=0. Удаление предмета очищает содержимое, сохраняя сам слот. Перенос/swap меняет только содержимое: правила и сетевые идентификаторы остаются у ячейки. FastArray уведомляет UI и передаёт изменения записей; ItemInstance и его характеристики по-прежнему реплицируются отдельным subobject.

Профиль игрока 20 Storage + 5 Action теперь задаётся Player Layout asset, а не C++. Настройка ячеек находится в **Layout → Slots**, содержимое — в StarterItems/InitialContents и серверных операциях. Порядок Slots определяет начальную нумерацию; правила и вместимость копируются в runtime Entries один раз. Последующее изменение Layout asset влияет на новую игровую сессию, но не пересобирает уже работающий контейнер.

На сервере InitializeInventory назначает слотам устойчивые индексы и помечает начальные записи, включая пустые, для репликации. На клиенте runtime Entries изначально пуст: локальные template-записи больше не создаются. Порядок записей FastArray не считается порядком слотов — поиск идёт по SlotIndex. Отдельного реплицируемого массива вместимости или правил нет. Проверка HasResolvedItems допускает пустые слоты, а для занятого ожидает ItemInstance и его определение; она не является подтверждением полной начальной доставки. Первый полностью доставленный снимок, packet loss/late join и Iris этим проходом не проверялись.

Action-панель фильтрует тот же контейнер, не дублируя предметы. Кнопки выбора оружия выбирают N-ю Action-ячейку; оружие экипируется только из неё. Полный перенос внутри контейнера сохраняет ItemInstance, split/межконтейнерный перенос сохраняют прежний контракт создания экземпляра у нужного владельца и копирования состояния.

Разные сумки в дальнейшем могут использовать отдельные экземпляры того же компонента/акторы-контейнеры. Несколько компонентов у одного Actor не должны делить один item instance; AddItemInstance проверяет это, item OnRep уведомляет фактические контейнеры через Contains. Вложенность сумок и управление их attachment/lifetime пока не реализованы.

## Перенос и сервер

`TransferTo(Target, SourceSlot, TargetSlot, Amount)` — доверенная серверная операция компонента, не RPC. Amount=0 означает весь стек. Проверяются authority, общий World, количество, свободная ёмкость, AcceptedItems и состояние стеков. Реализованы перенос в пустую ячейку, разделение, объединение одинакового класса с одинаковыми stat stacks и перестановка целых предметов внутри одного контейнера. Обмен разными предметами между двумя занятыми контейнерами пока отклоняется. При переносе между владельцами создаётся instance с правильным Outer, переносятся stat stacks; старый не переиспользуется у другого Actor. Уведомления публикуются после изменения обоих контейнеров.

Клиент использует `NGPlayerInventoryManagerComponent.RequestInventoryTransfer`. RPC содержит ExpectedPawn, оба контейнера/номера ячеек и ожидаемые item instances исходной/целевой ячеек. Сервер повторно проверяет их, состояние жизни, rate limit и доступ к обоим контейнерам. Разрешены контейнеры собственного PlayerState/Pawn и открытый через валидированную интеракцию внешний контейнер, пока сохраняются дистанция/LOS и CanInteract. Смена Pawn сбрасывает открытый внешний контейнер.

UI сообщает только о запросе: возвращённый true означает dispatch, а не подтверждение успешной операции сервером. Интерфейс не меняет счётчики локально и не принимает клиентский результат операции как истину.

`bOwnerOnly` задаётся в шаблоне компонента до репликации и не переключается в ходе сессии. Личные предметы owner-only. У коробки bOwnerOnly=false: содержимое доступно всем relevant-клиентам, не только открывшему окно. Для скрытых контейнеров с репликацией только разрешённым наблюдателям понадобится отдельная политика доступа/репликации; это не заявлено реализованным.

## Коробка

Создать BP от `ANGInventoryBox`, назначить Mesh, подогнать Collision. У компонента Inventory назначить **Layout**. Native-класс коробки не задаёт размер; его определяет Slots назначенного asset. В InitialContents задать Item (ItemClass/Count/StatStacks), без номера слота. Сервер размещает записи по порядку списка в первые совместимые свободные ячейки. Старые сохранённые номера в InitialContents игнорируются. У игрока InitialContents также размещается автоматически; SlotTag можно оставить для выбора группы Action/Storage, пустой тег разрешает любую совместимую группу. При отсутствии места запись отклоняется с диагностикой, занятые слоты не перезаписываются. Точные индексы сохраняются в runtime capture/restore API. Коробка подключена к существующему INGInteractable/GAS-пути. На сервере Interact открывает источник, ClientShowInventory подтверждает контекст Interaction.Storage для UI Router.

При закрытии окна вызвать `NGPlayerInventoryManagerComponent.CloseInventoryView` из существующего UI state/Blueprint, затем выполнить обычный переход FSM. Сам контейнер и виджеты не добавляют IMC, клавиши, input mode или fullscreen layout.

## Blueprint UI — сетап пользователя

1. `WBP_InventorySlot` от `UUIInventorySlotWidget`. Опциональный Button `btn_Root` (Is Variable), внутри — ваши иконка/количество/выделение. Data содержит только Inventory, Item, SlotIndex, Name, Icon, Count и bEquipped. Inventory/Item/SlotIndex идентифицируют ячейку и ожидаемый предмет для запросов; остальные поля нужны отображению. При Item=null отображается пустая ячейка. btn_Root по клику вызывает PrimaryAction. Для Drop вызвать RequestOperation(Drop, Amount); разрешение операций определяет обработчик инвентаря, а не список в UI. Фильтр Action/Storage применяется самой панелью до передачи данных слотам. SlotTag, StackLimit, ItemTags, StatStacks и AvailableOperations из UI-снимка удалены, в доменной модели инвентаря/предметов они сохраняются. После сборки обновить Break UIInventorySlotData в Blueprint; если удалённые выходы подключены вручную, убрать эти подключения. Blueprint-ассеты агент не менял; сборка/runtime не проверены.
2. `WBP_InventorySlotsList` от `UUIInventorySlotsList`: WrapBox `WrapBox` (Is Variable), SlotClass=WBP_InventorySlot. Список переиспользует слот-виджеты, показывает в том числе пустые ячейки. Для gamepad включить фокус кнопки и вызвать SetFirstSlotFocused из UI state при необходимости. OnInputTypeChanged слота идёт через существующий UPlayerInputSubsystem; InitShortcut/OnShortcutUpdated меняют только представление подсказки.
3. `WBP_Inventory` от `UNGInventoryPanel`: вложенный WBP_InventorySlotsList с именем `SlotsList` (Is Variable). SlotFilter=Inventory.Slot.Storage. Монтировать InventoryStateComponent с Source=Player.
4. `WBP_ActionPanel` от того же UNGInventoryPanel: SlotFilter=Inventory.Slot.Action. Получает пять настоящих ячеек; монтируется отдельно.
5. `WBP_ContainerInventory` от UNGInventoryPanel: SlotFilter=Inventory.Slot.Storage (либо пустой фильтр для всех ячеек). Монтировать InventoryStateComponent с Source=Interaction в состоянии Storage рядом с личной панелью. Маршрутизация описана в InteractionUI.md.

Drag-and-drop подключён в native UUIInventorySlotWidget: удержание ЛКМ и движение начинают перенос всего стека, Drop вызывает MoveItemHere(SourceData, 0). Между Action, основным инвентарём и коробкой используется один серверный путь; локально содержимое не меняется, результат приходит репликацией. Принятый UI-запрос не означает подтверждение операции сервером. UUIInventoryDragDropOperation хранит снимок исходного слота и LocalPlayer. Общие inventory-виджеты не знают Pawn. Слот проверяет локальный жест и данные запроса; NGInventoryPanel передаёт запрос через PlayerInventory Manager. Окончательное решение о доступе, количестве, размещении, merge/swap и изменении данных принимает сервер. Отпускание за пределами слотов ничего не переносит. Обычный клик без перетаскивания сохраняет PrimaryAction. Иконка предмета используется как временное изображение перетаскивания размером со слот.

После полной сборки и перезапуска Editor существующие BP-наследники слота получают поведение автоматически. В Designer пустые и занятые слоты должны иметь размер и принимать hit-test (Visibility=Visible); декоративные изображения/текст не должны перехватывать ввод. Старые BP overrides OnPreviewMouseButtonDown/OnDragDetected/OnDrop могут перехватить native обработку — удалить ручной drag-код либо корректно вызывать Parent. Кнопка перетаскивания задаётся в Class Defaults → Inventory → DragDrop → Drag Button (по умолчанию ЛКМ). Разделение стека при drag, touch и перенос геймпадом пока не подключены. Сборка и игровые/сетевые тесты этой правки агентом не запускались.

Окна/слоты не строят WidgetTree кодом и не добавляются во viewport; размеры, графика и placeholder принадлежат вашему UMG/layout. Старый ItemsList/генерируемые текстовые кнопки NGInventoryPanel удалены. Если старый Blueprint обращался к ItemDefinition.Weapon, заменить доступ на GetEquipmentDefinition либо поиск Equipment-фрагмента.

Иконка слота: существующий Image с именем `img_item` автоматически привязывается через BindWidgetOptional. Native SetData обновляет его на каждом снимке: отменяет старую загрузку, очищает brush, скрывает изображение для пустой ячейки и загружает Data.Icon для занятой. Hidden сохраняет размер ячейки; изображение предмета не перехватывает hit-test. Ручной SetBrushFromSoftTexture для этого Image в OnSlotUpdated убрать, чтобы иконкой управлял один обработчик; текст/число/оформление остаются в Blueprint. Передача пустой soft-ссылки в SetBrushFromSoftTexture сама по себе не очищает прежнюю картинку. Исправление проверено статически, сборка и визуальная проверка не выполнялись.

При переносе между игроком и открытым контейнером пустой слот принимает предмет;
совместимые стаки объединяются, если целиком помещаются в лимит. Иначе полные стаки
меняются местами, если подходят обеим ячейкам. Частичный swap отклоняется.
Оба инвентаря публикуют изменения после завершения одной серверной операции.
Проверено статически; сборка и проверка в игре агентом не запускались.

## Происхождение R5 и границы переноса

Адаптированы локальные `F:/UNREAL/Source/R5/UI/Inventory/Widgets/R5InventoryWidget.*`, `R5InventorySlotsList.*`, `R5InventorySlotWidget.*`, идея интерфейса `R5InventoryOperationInterface.h` и view-data из `R5InventoryTypes.h`. R5BL record paths/views, backend operation handler, consumable cooldowns, cheats/debug windows и отдельные HFSM layout/input handlers не переносились: они завязаны на отсутствующие R5-подсистемы либо дублируют наш mounter/инвентарь. Неподдерживаемые действия не возвращают фиктивный успех.

## Загрузка и совместимость

Будущий backend/файловый загрузчик на сервере разрешает внешние item IDs в доверенные ItemDefinition-классы, затем вызывает ImportItemState в заданные ячейки. CaptureInventoryState возвращает позиции/классы/количество/stat stacks. API не выполняет сетевые запросы к backend и не принимает импорт от клиента. Глобальные persistent item IDs, вложенные содержимые сумок и атомарная замена всего snapshot пока не реализованы; ImportItemState добавляет один предмет и отклоняет занятую/неподходящую ячейку.

Добавлены 12 точечных ClassRedirects/StructRedirects NEXTGENONE → GameplayFoundation. Их сохранять до успешной сборки, пересохранения ассетов и свежей загрузки. Миграция ассетов агентом не запускалась. Скрытый Weapon_DEPRECATED имеет общий тип UDataAsset; PostLoad преобразует старое значение в Equipment-фрагмент, учитывая переопределения дочернего Blueprint и не мутируя наследованный фрагмент. Старое поле не используется gameplay-кодом. Преобразование/сохранность старых настроек ещё надо подтвердить на реальных assets; поле/redirects до этого не удалять.

Выполнен только статический просмотр исходников и include/module границ. Сборка, Editor authoring, PIE и сетевые тесты не запускались. Требуются отдельные проверки: 20/5 UI, разные query ячеек, сохранность оружейных assets, два игрока у одной коробки, одновременные переносы/устаревшие запросы, split/merge, смена Pawn и потеря доступа к контейнеру.
