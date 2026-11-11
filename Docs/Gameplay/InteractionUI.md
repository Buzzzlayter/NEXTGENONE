# UI взаимодействия: первая ветка Storage

Реализован C++ контракт; графы и UMG пользователь настраивает вручную. Сборка, PIE и сеть агентом не проверялись.

## Владельцы

- PlayerState.Inventory — единственное хранилище игрока, серверные запросы переноса и сессия доступа к контейнеру.
- Interaction на Pawn — поиск/запуск интеракции и общий локальный контекст подтверждённого взаимодействия: Type (GameplayTag), Target (Actor), Source (UObject). Контекст не содержит классов виджетов или названий состояний. Он не заменяет серверную проверку доступа.
- UI FSM — маршрутизация, input policy и монтаж представлений.
- InventoryPanel — отображение только явно переданного источника. bUseViewedInventory и автоматический выбор своего/внешнего контейнера удалены.

Кулдаун интеракции принадлежит `UNGInteractionAbility`: стандартный GAS effect
`UNGInteractionCooldownEffect` даёт `Cooldown.Interaction` на 0,15 с при `CommitAbility`.
Таймера `NextInteractionTime` и отдельного API кулдауна в ASC больше нет. Commit
выполняется после проверки цели; неверный запрос кулдаун не расходует. Эффект живёт
на PlayerState ASC и не снимается при завершении способности или interaction-сессии.

Коробка имеет настраиваемый InteractionType, по умолчанию native Interaction.Storage. После серверной проверки интеракции PlayerInventory открывает сессию, отправляет owning client подтверждение с ExpectedPawn, Source и типом; клиент записывает контекст в Interaction текущего Pawn. Только затем UI получает событие. Личная панель открывается напрямую, без создания контекста внешней интеракции.

## Граф

```text
Playing --InteractionRequested--> Interaction
                                  └─ Resolve --OpenStorage--> Storage
Playing --Inventory input-------> Inventory
Interaction --InteractionClosed-> Playing
```

1. В Playing добавить **Interaction UI Router** (UNGInteractionStateComponent). Routes: `Interaction.Storage → InteractionRequested`. ClosedEvent оставить пустым. Переход InteractionRequested ведёт в родительское состояние Interaction.
2. В начальный сабстейт **Resolve** добавить Router с Routes: `Interaction.Storage → OpenStorage`; ClosedEvent оставить пустым. Переход OpenStorage ведёт из Resolve в Storage. Он выбирает экран один раз при входе, читая уже подтверждённый контекст после подписки.
3. На **родительское Interaction** добавить компонент **Interaction Session** (UNGInteractionSessionStateComponent, наследник Router): Routes пустой, ClosedEvent=`InteractionClosed`. Он наблюдает контекст, а на Exit завершает интеракцию. Заменить им прежний родительский Router, не добавлять оба. В графе создать переход **из родителя Interaction** по InteractionClosed наружу (в Playing либо существующий стейт выбора UI по состоянию игрока). На Resolve остаётся обычный Router. Переходы между детьми сохраняют сессию; выход из родителя завершает её.
4. В Storage подключить свой layout с двумя placeholders. Добавить два **Inventory Window** (UNGInventoryStateComponent, производный обычного WidgetMounter):
   - личная панель: Source=Player, WidgetClass=WBP_InventoryPanel, свой PlaceholderID;
   - панель коробки: Source=Interaction, WidgetClass=WBP_ContainerInventory, другой PlaceholderID.
5. Оба WBP наследовать от NGInventoryPanel. В них настроить существующий SlotsList/SlotClass и SlotFilter. Для личного основного инвентаря — Inventory.Slot.Storage; для коробки можно оставить фильтр пустым. Размер и расположение задаёт layout, панели не fullscreen.
6. Для обычного Inventory использовать только мounter Source=Player. Для ActionPanel также использовать Inventory Window с Source=Player, но виджет с фильтром Inventory.Slot.Action. Обычный универсальный мounter сам источник панели больше не назначает.
7. Escape и закрывающий input настраиваются пользователем в HFSM/input component и выводят из Interaction. InteractionClosed нужен для вынужденного выхода при смерти, смене Pawn и потере объекта. Маунтеры только отображают/демонтируют панели; bCloseInteractionOnExit удалён. Сессию закрывает компонент родителя Interaction Session. Общую bBlockGameplay policy назначить на UI FSM через существующий PlayerInputSubsystem; код не добавляет IMC или клавиши.

Если после смерти экран остаётся: проверить активность Router именно на родительском Interaction, непустой ClosedEvent и наличие выходящего перехода с точно тем же именем. Router только в Resolve после выхода из Resolve больше не наблюдает смерть. Если всё это настроено, причина требует проверки текущего Character FSM StateTag и доставки события; статический просмотр не доказывает состояние конкретного Blueprint в запущенной игре.

У Router нет зашитых названий событий: указанные выше имена — пример сетапа. Сопоставление тегов точное. Неизвестный тип сам переход не создаёт; для нового типа добавить маршрут и соответствующее состояние. События используют существующий HFSM контекстный маршрут, поэтому выбирать разные имена для независимых графов одного контекста.

SetInventorySource вызывается мounter после монтажа и очищается перед демонтажом. Виджеты подписываются только на изменения переданного инвентаря и данных предметов; наблюдение Pawn сохраняется для equipment и транспорта операций, без выбора источника или навигации FSM. Два окна используют общий MoveInventoryItem/серверный TransferTo; готовая drag-and-drop визуализация этим изменением не добавляется.

## Потеря контекста и сеть

### Прерывание через GAS

В native Interaction-компоненте персонажа есть редактируемый **InterruptingTags**. Defaults: State.Interaction.Blocked, State.Dead, State.Action.Dash, State.Action.Mantle, State.Action.Reload. Появление любого из них в ASC текущего Avatar событийно вызывает EndInteraction: сервер отменяет активные способности с asset-тегом **Ability.Type.Interaction**, очищается общий контекст, Inventory Manager завершает только связанную storage-сессию и уведомляет owning client. Повторная активация GA_Interact запрещена через CanActivateAbility и тот же набор тегов; сервер также проверяет его в интеракции/доступе к контейнеру.

GA_Interact получает Ability.Type.Interaction в native-конструкторе. Если Blueprint переопределяет asset tags, проверить наличие этого тега. Будущие длительные интеракционные способности должны иметь этот asset-тег, поддерживать cancellation и снимать свои ресурсы на EndAbility. GA_Interact сейчас мгновенная и уже завершена после открытия Storage: отмена этой ability сама по себе не закрывает экран. Поэтому отдельно завершаются контекст и серверная сессия; только их изменение даёт UI событие InteractionClosed.

Для нового состояния (например, оглушения) добавить его GAS-тег в InterruptingTags. Тег выдаётся сервером через существующий Gameplay Effect/ресурсы Character FSM. Снятие тега разрешает новые взаимодействия, но автоматически старую сессию не открывает. UI bBlockGameplay — локальная input policy, она не выдаёт State.Interaction.Blocked и не отменяет способности сама по себе.

На сервере и owning client Interaction привязывается к ASC при инициализации персонажа; при отвязке подписки снимаются. EndInteraction защищает Avatar перед отменой способностей. Закрывающие RPC сравнивают ожидаемые Pawn и Source, чтобы не закрыть контейнер другого Pawn/другого источника. Повторное открытие того же Source тем же Pawn пока не имеет отдельного сетевого session ID.

Router событийно выбирает положительный маршрут. Его Tick только проверяет потерю Target/Source, текущего Pawn и смерть через Character FSM; потеря отправляет ClosedEvent однократно. Выход снимает подписки. OnPawnSet/EndPlay PlayerInventory очищают связанный контекст, cleanup сравнивает источник, чтобы не удалить другой контекст.

Сервер повторно проверяет доступ, дистанцию/LOS, жизнь, ExpectedPawn, slots/items и rate limit для каждого переноса. Сам локальный контекст не даёт права изменить предметы. Автоматическое закрытие по изменению дистанции/LOS без операции пока не добавлено: недопустимый перенос отклоняется сервером. Это остаётся отдельным ограничением первого этапа. Late relevancy/ещё не разрешённая Source-ссылка при ClientShowInventory не имеют retry в этом пакете.

Для диалогов/терминалов переиспользуется общий контекст и Router; их серверные контракты и состояния пока не реализованы. Логика их данных не должна попадать в InventoryManager.
