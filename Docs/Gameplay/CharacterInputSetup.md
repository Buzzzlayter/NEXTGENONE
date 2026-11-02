# Конфиги персонажа и ввода — этап 1

## Заполнено через MCP — 2026-11-03

DA_Input_Character теперь заполнен: 13 NativeInputActions, 3 AbilityInputActions, базовый IMC_NG_Character с приоритетом 10 и 35 mappings. Созданы 17 новых IA и 3 IMC в `/Game/Gameplay/Input/Configured`; существующие IA_Dash, IA_Mantle, IA_PickupWeapon переиспользованы без изменения. ALS plugin и старые IMC не менялись.

| Действие | Клавиатура/мышь | Геймпад, обозначения Xbox |
|---|---|---|
| Движение / обзор | WASD / мышь | левый / правый стик |
| Прыжок | Space | A |
| Dash в воздухе / Mantle | Q / V | RB / Y |
| Взаимодействие | E | LB |
| Огонь или удар / прицел | ЛКМ / ПКМ | RT / LT |
| Перезарядка | R | X |
| Бег / стойка | Shift / Ctrl | L3 / B |
| Ходьба | Alt | аналоговое движение; отдельной toggle-кнопки нет |
| Слоты 1/2/3 | 1/2/3 | D-pad влево/вверх/вправо |
| Убрать / тестовая экипировка | U / F | D-pad вниз / выбор слота экипирует оружие |
| Инвентарь | I | View |
| Настройки | Esc | Menu |
| Закрыть инвентарь | I / Esc | View / Menu / B |
| Назад в настройках | Esc | B / Menu |
| Подтверждение меню | Enter | A |

Строки подтверждения/навигации описывают назначенные UI keys; реальное поведение UMG/геймпада пока не проверено в игре. UI_Confirm IA предоставлен в Menu context; текущие стандартные виджеты и Grid preprocessor продолжают выполнять свою Slate-навигацию.

Move использует Axis2D и Cumulative accumulation, W/S — Swizzle YXZ, S/A — Negate. Стики имеют DeadZone. Mouse/Stick look — отдельные IA; pitch инвертирован Y-only, yaw не инвертирован. Слоты используют Scalar X=2/3 в mappings для соответствующих клавиш/D-pad. Digital trigger keys RT/LT используются для Boolean fire/aim; аналоговая сила нажатия не реализована.

FSMState_GameRuntime.HFSMInputComponent подключает отдельный IMC_NG_UI_Global (priority 20); Runtime Settings Navigation и Inventory Window получили IA открытия. FSMState_GameSettings подключает IMC_NG_UI_Menu (100). CloseActions назначены на WBP_InventoryPanel/WBP_GameSettings, описание управления заполнено в ControlsDescription. UI_Back не consumes Gameplay input, чтобы B не блокировал stance при закрытом меню. Gameplay IA/IMC не помещены в UI config.

Четыре затронутых UI Blueprint скомпилированы; все 25 затронутых assets сохранены. Проверены readback конфигов, references, swizzle/negate/scalar и UI назначения. Это авторинг, не игровой smoke test. C++ не менялся, UBT повторно не запускался. PIE/сетевые/игровые тесты агент не выполнял. Backup UI assets: `Saved/InputSetupBackup/20261103/`.

Нижние инструкции сохраняются как контракт для новых Pawn/configs; повторно создавать уже существующие ассеты не требуется.

2026-11-02. Конфиги и общий GameplayInputComponent находятся в GameplayFoundation; ALS/Combat адаптер NGCharacterInputComponent — в NEXTGENONE/Gameplay/Input. Добавлены runtime-маршрутизация ввода и локальная координация UI/Gameplay через UPlayerInputSubsystem. Следующим пакетом PlayerState подключён к CharacterDefinition.AbilitySets как единственному источнику базовых способностей Pawn. Ассеты пользователь заполняет вручную. FSM персонажа ещё не подключена.

## Этап 3: выдача базовых Ability Sets

На сервере PlayerState.StartLife читает тот же GameplayInput.CharacterDefinition, который клиент использует для ввода. В нём нужно назначить существующий DA_PlayerAbilities. В сохранённом наборе уже есть GA_Jump, GA_Dash, GA_Mantle, GA_Interact и GA_Punch с актуальными input tags; скрытые C++-выдачи и default /Game-путь удалены.

Без CharacterDefinition сервер пишет понятную ошибку и не завершает инициализацию GAS этого Pawn. После удаления fallback-выдачи удалены и неиспользуемые поля PlayerAbilitySet/DefaultPlayerAbilitySet/JumpAbility на PlayerState. Из Controller удалены старые AbilityMappingContext/DashInput/MantleInput/PickupInput. Настройки нужно назначать через CharacterDefinition/InputConfig; автоматического копирования прежних значений нет. Назначение нового Definition через RPC клиента отсутствует: сервер использует конфиг своего Pawn.

Definition проверяет пустые/повторные sets, классы способностей, уровни и повторы валидных input tags между базовыми наборами. Применяет каждый set отдельно и собирает общий набор handles. SourceObject базовых ability specs — текущий Pawn. Владение ASC остаётся на PlayerState; второго ASC нет.

- Повторная инициализация уже привязанного Pawn не выдаёт повторно наборы и не инициализирует здоровье заново.
- Смерть, UnPossess/Unbind и EndPlay снимают базовые grants по handles. Снятие старого Pawn после новой выдачи не совпадает с текущим владельцем и ничего не снимает.
- При смене Pawn старые grants и действия снимаются до InitAbilityActorInfo нового Avatar.
- Для нового Pawn увеличивается LifeSerial и выполняется прежний init-health effect. Для повторного possession того же живого Pawn здоровье не сбрасывается.
- Duration/Infinite эффекты базового набора снимаются с его scope и могут применяться при повторной привязке. Instant эффекты не откатываются при выходе и не применяются повторно при possession того же Pawn. Cooldowns, созданные самими abilities, и серверный fire limiter этим снятием не сбрасываются.
- Смена Definition при уже активной привязке не является runtime hot-swap: для неё нужен явный lifecycle/будущая FSM. Текущий пакет не реализует произвольное переключение состояний.

Equipment grants остаются у Combat; это отдельные handles. Настоящие постоянные grants игрока при необходимости должны иметь отдельный явный scope, а не попадать в CharacterDefinition.

## Этап 2: обязательный сетап перед игровым запуском

Старые автоматические ALS/Combat bindings в Controller отключены. Без назначенного CharacterDefinition.InputConfig GameplayInput выводит warning и не устанавливает ввод. Не запускать игру в ожидании старых клавиш до заполнения ассетов.

В BP_NextgenoneCharacter выбрать новый native-компонент **GameplayInput** и назначить DA_Character_Default в CharacterDefinition. Тот же общий компонент можно подключать к другому Pawn: в его SetupPlayerInputComponent вызвать InitializeInput, при потере управления — ShutdownInput. Для native movement/камеры другого Pawn нужен свой адаптер; один конфиг не создаёт движение произвольному Pawn.

NativeInputActions для NGCharacterInputComponent:

| InputTag | IA type | Поведение |
|---|---|---|
| Input.Move | Axis2D | X вправо, Y вперёд; клавиатура и левый стик |
| Input.Look.Mouse | Axis2D | Mouse delta, без умножения на delta time |
| Input.Look.Stick | Axis2D | Скорость обзора; правый стик; градусы/с задаются на GameplayInput |
| Input.Jump | Boolean | Единственный ALS→GAS маршрут прыжка; не добавлять второй binding в AbilityInputActions |
| Input.Aim | Boolean | Удержание прицеливания; единственный ALS/Combat маршрут |
| Input.Sprint | Boolean | Удержание sprint |
| Input.Combat.Primary | Boolean | Стрельба при экипировке, иначе удар; автоогонь принадлежит Combat |
| Input.Equipment.Equip / Input.Equipment.Unequip | Boolean | Экипировать / снять |
| Input.Equipment.Reload | Boolean | Перезарядить |
| Input.Equipment.SelectSlot | Axis1D | Значения 1, 2, 3… через modifiers в IMC; номера слотов не связаны с физическими клавишами |
| Input.Stance / Input.Walk / Input.Ragdoll | Boolean | Команды ALS |
| Input.Camera.Tap / Input.Camera.Held | Boolean | Команды камеры ALS; Tap/Hold triggers задаются на IA |
| Input.Rotation.Velocity / Input.Rotation.Looking | Boolean | Режимы вращения ALS |

Дискретная native-команда исполняется один раз при первом Triggered, а не заранее на Started: это позволяет использовать Hold/Tap/порог курка. Movement/Look обрабатываются каждый Triggered. Ability input также отправляет pressed один раз на первый Triggered и released на Completed/Canceled.

AbilityInputActions: существующие Input.Dash, Input.Mantle, Input.Interaction.Interact и при необходимости Input.Combat.Punch. Для обычного удара/стрельбы достаточно Native Input.Combat.Primary. Отдельные Input.Combat.Fire/Reload можно связывать прямо с GAS, но они не дают native автоогонь/маршрут Reload компонента; основной контракт — строки таблицы.

### UI отдельно от персонажа

- Создать **IMC_UI_Global** с Boolean IA открытия инвентаря и настроек. Назначить его на UHFSMInputComponent существующего GameRuntime, Gameplay IMC туда не помещать.
- В Runtime Settings Navigation задать **OpenSettingsAction**.
- В Inventory Window задать **ToggleAction**.
- В WBP_InventoryPanel и WBP_GameSettings задать **CloseActions**. Действия закрытия должны быть mapped в UI contexts соответствующего экрана, включая Back на геймпаде. Это простые кнопки без chords/hold; native key routing в UIOnly сопоставляет physical key с активными UI mappings.
- В FSMState_GameSettings назначить IMC_UI_Menu (Back/Confirm/navigation при необходимости) на его HFSMInputComponent. UIOnly ввод получает сфокусированный widget; закрытие не зависит от gameplay callback.
- UHFSMInputComponent теперь получает mapping/policy handles от UPlayerInputSubsystem. bBlockGameplayInput можно выставить явно; UIOnly также блокирует Gameplay. Inventory и Settings уже запрашивают полную блокировку, focus и Grid cursor policy самостоятельно.

Никаких новых hardcoded клавиш для открытия/закрытия этих экранов нет. Кнопка Back/Cancel в настройках продолжает работать независимо от CloseActions. Старый hardcoded список клавиш удалён из настроек: текст можно заполнить в WBP_GameSettings.ControlsDescription. Автоматические подсказки по mappings/устройству и runtime remapping пока не реализованы.

### Владение и ограничения

GameplayInput снимает собственные binding handles и context handles при повторной инициализации, UnPossess, смерти и EndPlay. Не очищает чужие bindings. Releases используют только ASC, чей Avatar всё ещё этот Pawn. UI-политики складывают блокировки; последняя активная политика с указанным режимом/курсором определяет их значения. При восстановлении contexts используется IgnoreAllPressedKeysUntilRelease для Boolean actions.

Один IMC можно запросить несколькими владельцами: действует максимальный приоритет, снятие одного handle не удаляет чужой запрос. Запросы нужно освобождать в Exit/AbortEnter/EndPlay; мёртвые owners дополнительно очищаются при следующем пересчёте. Это не фоновый tracker: без lifecycle освобождения не обещается немедленная уборка уничтоженного owner. Gameplay/UI должны использовать разные IMC и IA.

Прежние UI API/флаги и drop RPC Controller пока сохранены для Blueprint-совместимости. Старый smoke transport остаётся opt-in, его вызовы переведены на Combat напрямую. Ни один тест не запускается этим изменением. Серверный limiter PlayerState не перенесён и не сбрасывается локальным таймером автоогня.

После отдельной миграции 2026-11-02 ссылки 39 существующих проектных ассетов пересохранены; 11 redirects переноса в GameplayFoundation удалены после fresh-load. Новые конфиги пользователь ещё должен создать/назначить. Подробности: ../Architecture/CoreRedirectMigration.md.

## Что создать в редакторе

Через Data Asset создать два ассета в выбранной папке проекта (путь не зашит в C++):

1. `DA_Input_Character` класса **NG Gameplay Input Config**.
2. `DA_Character_Default` класса **NG Character Definition**: назначить первый в InputConfig и существующий `DA_PlayerAbilities` в AbilitySets.

В DA_Input_Character:

- **MappingContexts** — Gameplay IMC и их приоритеты. Каждый IMC указывается один раз. Существующий `IMC_Abilities` можно использовать; недостающие действия сначала добавить в IMC.
- **NativeInputActions** — движение, камера и команды существующих компонентов. Строка содержит IA и смысловой тег. Точный список новых native-тегов будет закреплён при реализации обработчиков на этапе 2; произвольный тег ещё не создаёт обработчик.
- **AbilityInputActions** — IA и точный InputTag из существующего Ability Set. Например: Input.Dash, Input.Mantle, Input.Combat.Punch, Input.Interaction.Interact. Jump у текущего ALS-персонажа проходит через NativeInputActions, как указано в таблице выше.

Сейчас готовыми ассетами среди этих действий являются IA_Dash и IA_Mantle; наличие остальных IA нужно проверить/создать при заполнении. Не добавлять одновременно старый pickup-тег и новый interact-тег для одной кнопки. Runtime-спецмаршрут interact в ASC будет перенесён в компонент при следующем этапе.

## Клавиатура и геймпад

Клавиши/кнопки назначаются только в IMC. Один IA может иметь mapping клавиатуры и геймпада; строки InputConfig для устройств не дублируются.

- Ability actions: Boolean, с порогом срабатывания для аналогового курка в Enhanced Input. Контракт GAS — pressed один раз на первый Triggered; Completed и Canceled оба освобождают удержание.
- Movement/Look: native actions с нужным типом Axis. Для мыши и стика предусмотреть разную интерпретацию delta/rate при обработке, dead zones/modifiers задавать в assets.
- Для того же IA нельзя одновременно делать native и ability binding в одном config. Jump/Aim из ALS не подключать второй раз: текущий адаптер предоставляет единый native-маршрут.

UI IA/IMC не помещать в этот config. Их владелец — существующая UI FSM через HFSMInputComponent и UPlayerInputSubsystem. Старые IMC можно сохранить для переноса mappings, но Controller больше не активирует их автоматически.

## Валидация ассетов

Классы реализуют стандартный IsDataValid. Проверяются пустые ссылки, повторы IMC/IA/тегов, отрицательные приоритеты, отсутствие IA в указанных contexts, Boolean для GAS-ввода, пустые Ability Sets/grants/effects. Отсутствие base ability для input-тега выдаёт warning: способность может появиться от экипировки или временного состояния.

Это проверка конфигурации, не доказательство работы input/сети. Агент её в этом этапе не запускает без отдельного апрува на проверки. Авторинг и компиляция C++ не запускают PIE.

## Следующие этапы

Каркас FSM и временных ресурсов реализован отдельным пакетом; пользовательский Blueprint-сетап и ограничения описаны в [CharacterStateMachine.md](CharacterStateMachine.md). Runtime/сеть пока не проверены. StateMappingContexts в InputConfig перечисляет дополнительные contexts без автоматической активации.

1. Готовые классы конфигурации и заполнение ассетов пользователем (этот пакет).
2. Pawn input component, маршрутизация native/GAS, удаление MapKey/action-specific кода Controller реализованы в C++; runtime подтверждение и авторинг ассетов пользователем ещё требуются. Инвентарь блокирует Gameplay; UI остаётся независимым от Pawn. Серверные ограничения combat сохраняются.
3. Выдача из CharacterDefinition подключена. Следующий пакет — Blueprint FSM на Pawn и C++ компоненты временных ресурсов. ControlStateDefinition добавляется вместе с механизмом применения, не как неиспользуемая схема заранее. Сервер подтверждает переходы; локальный input только owning client, представление — всем релевантным клиентам. Смерть и тело должны переживать UnPossess, ASC остаётся на PlayerState; новый FSM/death flow пока не реализован.
4. Выделение модулей после стабилизации поведения, с Core Redirects.

На этапах 2–3 нужны адресные read-only проверки GCFSM replication и ALS ragdoll без Controller до изменения их интеграции. Тестирование — только с отдельным явным апрувом и согласованным объёмом.
