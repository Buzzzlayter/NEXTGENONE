# Бот: первый исполняемый граф

## Созданные ассеты

- /Game/Gameplay/AI/BP_NGBotCharacter — копия настроенного BP_NextgenoneCharacter с прежними ALS AnimBP, Character FSM Alive/Dead и компонентами. AIControllerClass=BP_NGAIController, AutoPossessAI=PlacedInWorldOrSpawned, AutoPossessPlayer=Disabled.
- /Game/Gameplay/AI/BP_NGAIController — наследник NGAIController, BehaviorMachineName=FSM. Граф в EventGraph: Waiting → Move → Ability → Pause → Move; NotReady возвращает в Waiting, включая петлю через reroute. В Waiting назначен NGAIWaitForGameplayState. Failed/Aborted действий направлены в Pause, повторная попытка через секунду.
- /Game/Gameplay/AI/FSM_AI_Pause — конкретный Blueprint-класс от UHFSMState с Context типа NGAIController. Дополнительной логики/таймеров в Blueprint нет: паузу выполняет штатный TimeOut GCFSM на ноде (1 секунда); выход таймаута ведёт к Move. Пустой Done заменён этим классом.

Оба ассета сохранены. Пользователь назначил NGAIMoveState и NGAIAbilityState; агент сохранил и прочитал обратно параметры: Destination=(300,0,0), bRelativeToPawn=true, AcceptanceRadius=50, ActionTimeout=10 у Move; InputTag=Input.Jump, HoldDuration=0.2, ActionTimeout=3 у Ability. Blueprint compilation выполнена на текущих загруженных native-типах. После переноса Context непосредственно в Wait и Action нужна полная пользовательская сборка, перезапуск Editor и повторная компиляция Blueprint. Персонаж на карту агентом не добавлялся; PIE/сеть не запускались.

Причина ручного выбора двух классов: MCP set_pin_value вызывает падение редактора при реконструкции GCFSM-ноды с новыми ExposeOnSpawn-пинами. В исходниках BlueprintGraphPin.cpp SetPinValue после TrySetDefaultValue обращается к прежнему NativePin, а GCFSM пересоздаёт пины синхронно. Первый контроллер не успел сохраниться при этом crash; после пользовательского перезапуска он восстановлен и сохранён со связями. Повторно выбирать эти классы через этот MCP-вызов нельзя до исправления editor API.

## Что добавлено

### Автоматические выходы классов

Новый C++-контракт UGCFSMState::GetDeclaredTransitionEvents() возвращает события класса. Он вызывается редактором на CDO, поэтому реализация не должна обращаться к World или живому Context. UNGAIActionState объявляет Succeeded/Failed/Aborted, UNGAIWaitForGameplayState — Ready. Эти же имена из NGAIEvents.h используются при отправке событий в runtime. Ready теперь фиксирован; редактируемый Controller.ReadyEvent удалён. Внешний NotReady по-прежнему настраивается на Controller и добавляется в переходы ноды вручную.

После пользовательской полной сборки и перезапуска Editor выполнить Refresh/Reconstruct Nodes у нод AI, затем Compile и сохранить контроллер. Выходы добавляются при выборе статического Implementation Class и реконструкции. Старые Succeeded/Failed/Aborted/Ready с теми же именами сохраняют свои связи без дублей. Неподключённый стартовый Event заменяется декларациями класса; прочие ручные события остаются. Редактор хранит принадлежность автособытий классу: при смене класса исчезнувшие события удаляются из активного набора, связанные пины сохраняются штатным orphan-механизмом UE. Их нужно вручную переподключить или удалить; это не продолжающие работать переходы.

SuccessEvent/FailureEvent/AbortedEvent больше не являются свойствами или входными пинами действий. Если ранее им назначались нестандартные значения или подключались провода, их нужно перенести на фиксированные выходы и удалить оставшиеся orphan-входы. Объявленные события нельзя переименовать в Details независимо от класса. При динамическом Class-пине автоматический контракт неизвестен; для этого эксперимента выбирать класс прямо на ноде. Classes без деклараций и local/submachine-ноды сохраняют ручной авторинг событий. Стандартные TimeOut/Tick не менялись.

Изменения автоматических выходов просмотрены статически, но не собраны и не проверены в редакторе или игре. Ранее сохранённый граф ещё использует предыдущие загруженные native-типы. Известный crash MCP при выборе класса с ExposeOnSpawn этим пакетом не исправляется — безопасный выбор класса выполнять вручную в UI.

### Проверка пользователя

Пользователь сообщил, что добавил NavMesh, протестировал бота и цепочка заработала. Это подтверждение пользователя для предыдущей версии графа; состав сетевой сессии и негативные сценарии не уточнялись. Оно не подтверждает новые автоматические выходы. Агент тестирование не запускал.

C++ в NEXTGENONE/Gameplay/AI. Сборку C++ выполняет пользователь. Blueprint контроллера с циклом скомпилирован через MCP с warnings_as_errors=true и сохранён, класс паузы сохранён; ссылки/параметры прочитаны обратно. UBT, Live Coding, PIE и сеть агентом не запускались. Компиляция Blueprint не подтверждает игровой результат.

- NGAIController создаёт PlayerState и запускает нереплицируемую GCFSM на сервере. Не зависит от ANextgenoneCharacter: текущий Pawn должен иметь CharacterStateMachine и проектный ASC на PlayerState.
- NGAIWaitForGameplayState подтверждает возврат графа в ожидание. Ready выдаётся после готовности Alive/ASC и этого барьера.
- NGAIActionState хранит результат Running/Succeeded/Failed/Aborted и причину; допускается одно активное действие. Выполнение привязано к Pawn, ASC, поколению Avatar, possession и конкретному входу состояния.
- NGAIMoveState использует MoveTo, отклоняет partial path и ждёт результата своего request ID. При выходе отменяет только свой запрос.
- NGAIAbilityState вызывает общий press/release. Требует ровно один неактивный InstancedPerActor spec с точным InputTag. Запуск запроса не считается успехом: ожидается фактическое завершение ability; cancel даёт Aborted. NonInstanced/InstancedPerExecution и одновременные исполнения не поддержаны первым пакетом.

AI не выдаёт способности и предметы: они по-прежнему принадлежат Character state definitions и экипировке. Жизнь/смерть управляются прежней Character FSM. Никаких player UI/IMC для исполнения AI не требуется; рабочий CharacterDefinition/InputConfig сохранить из-за общей валидации state definitions.

## Blueprint-сетап после полной сборки

AI-действия — готовые C++-наследники UHFSMState, без промежуточных action-компонентов и без обязательных Blueprint-наследников. В ноде GCFSM выбирать конкретный Implementation Class; параметры EditAnywhere + BlueprintReadWrite + ExposeOnSpawn появляются входными пинами. Можно задавать константы либо подключать вычисленные значения. Плагин присваивает их перед Enter. При динамически подключённом Class-пине специализированные пины могут не отображаться — выбрать класс прямо на ноде. После изменения C++ обновить ноды (Refresh/Reconstruct Nodes).

Пины Move: Destination, bRelativeToPawn, AcceptanceRadius. Пины Ability: InputTag, HoldDuration. Общий вход: ActionTimeout. Выходы результата создаются из контракта класса автоматически. ActionTimeout — watchdog действия; встроенный TimeOut GCFSM остаётся отдельным механизмом и не переименовывается.

Если ранее вручную добавлялись NGAI*StateComponent в Components, убрать эти инстансы и выбрать соответствующий NGAI*State в Implementation Class ноды. Это смена модели component → state, автоматический ClassRedirect между несовместимыми базовыми типами не добавляется. Общие компоненты UI/HFSM не менялись.

1. Создать BP_NGAIController от NGAIController. BehaviorMachineName — имя GCFSM-графа, например BotBehavior. Событие готовности фиксировано: Ready; NotReadyEvent=NotReady. Не добавлять Launch FSM в BeginPlay: запуск выполняется C++.
2. Создать bot-вариант существующего настроенного BP персонажа. Сохранить Character FSM, Alive/Dead definitions и ALS AnimBP. AI Controller Class=BP_NGAIController, Auto Possess AI=Placed in World or Spawned; Auto Possess Player выключить. GameMode должен создавать NextgenonePlayerState.
3. Поставить бота на доступную навигацию. Добавить/проверить NavMeshBoundsVolume на нужной площадке; выбор цели и обход препятствий обеспечивает стандартный Navigation.
4. В BP_NGAIController использовать существующий граф FSM либо новое имя, совпадающее с BehaviorMachineName. Текущая схема плоская: WaitingForGameplay, Move, Ability, Pause. У каждой ноды назначен Implementation Class. Wait и Action непосредственно наследуют UHFSMState и содержат Transient BlueprintReadOnly Context типа NGAIController: GCFSM сам заполняет его при Initialize до Enter и проверяет совместимость типов. Отдельную Context-переменную для этих native-классов в Blueprint создавать не нужно. Context не является ExposeOnSpawn-параметром. Не использовать UI input-компоненты.
5. У WaitingForGameplay выбрать Implementation Class=NGAIWaitForGameplayState. Ready → Move. NotReady через reroute обратно в WaitingForGameplay сбрасывает прежний цикл при новом possession.
6. У Move, Ability и Pause подключить NotReady → WaitingForGameplay. Объединение этих переходов в родительском Active — возможное дальнейшее упрощение графа, в текущем сохранённом ассете его нет.
7. У Pause выбрать Implementation Class=FSM_AI_Pause. В Details ноды TimeOut=1 секунда; выход TimeOut → Move. Это штатный GCFSM TimeOut, а не ActionTimeout. После успешного выполнения или ошибки цепочка ждёт секунду и повторяет попытку. Выбор цели/GOAP отсутствует: это только циклическая последовательность.
8. У Move выбрать Implementation Class=NGAIMoveState. На пинах Destination=(300,0,0), bRelativeToPawn=true — пройти на 300 см вперёд относительно положения/ориентации при каждом входе. Либо bRelativeToPawn=false и задать мировую точку. AcceptanceRadius=50, ActionTimeout=10. Succeeded → Ability; Failed/Aborted → Pause.
9. У Ability выбрать Implementation Class=NGAIAbilityState. Для первичной проверки без оружия на пинах InputTag=Input.Jump, HoldDuration=0.2, ActionTimeout=3; Jump должна быть выдана Alive. Succeeded/Failed/Aborted → Pause. Завершение Jump ability подтверждает её lifecycle, а не приземление или успешный traversal.
10. Для ADS вместо Jump задать InputTag=Input.Aim, HoldDuration=1, ActionTimeout=3. Aim ability должна быть уже выдана экипированным оружием. Первый пакет не добавляет автоматическую экипировку: отсутствующий spec даст Failed. Подготовка оружия ботом — следующий пакет.

Имена result-событий определяет класс состояния. Один action-state исполняет одно действие; дополнительные action-компоненты не добавлять. Не запускать параллельно Blueprint MoveTo или ту же ability из другого источника: они не принадлежат этому исполнению. Стрелять, наносить урон или менять боезапас из графа нельзя. Во время исполнения не изменять InputTag и параметры cleanup через Set: входные значения задаются до Enter.

## Lifecycle и ограничения

Readiness обновляется по OnStateChanged/OnResourcesReady Character FSM, дополнительно — таймером раз в 0.1 с на possessed Controller. Таймер покрывает смену ресурсов/ASC без изменения state tag; это не скан мира и не планирование каждый кадр. У активного действия отдельный watchdog 0.1 с проверяет поколение/готовность/timeout. Зависание пути завершится по timeout, отдельный stuck-detector ещё не добавлен.

При потере gameplay-ready активное действие немедленно освобождает ресурсы, граф получает NotReady. При UnPossess очистка выполняется до отвязки Pawn, граф ставится на паузу. При новом possession снимается пауза и отправляется NotReady; до входа WaitingForGameplay старт действий запрещён. При EndPlay выполняется та же очистка и граф остаётся на паузе до уничтожения Controller. Публичный UGCFSM::Stop не вызывается на активной машине: исходный plugin требует сначала выйти из состояния.

Release/Cancel адресуются сохранённым ASC/spec и только прежнему Avatar. Поздние path/ability callbacks фильтруются по запуску, request/spec/instance; результат фиксируется один раз. Notification графу откладывается до следующего тика, отменяется при Exit и проверяет прежний active state и possession. События результата имеют короткий срок жизни 0.1 с: нельзя вручную ставить граф на паузу посреди действия и ожидать отложенный результат. Тактическую паузу пока делать выходом из action-state.

Не реализованы: динамическая цель и Focus-компонент, планирование, perception/memory, экипировка ботом, ограниченные очереди, выбор маршрута Jump/Mantle, отряд. Успех GAS ability сам по себе не подтверждает достижение игровой цели. GOAP и новые AI-framework dependencies не добавлены.

## Проверить после отдельного апрува

- Переход Waiting → Move только после настоящего Alive/ASC; движение, прыжок, пауза и повторный цикл.
- Недостижимая точка, timeout, повторный вход, отмена/замена path request.
- CanActivate rejected, отсутствующая/дублированная input ability, удержание и release ADS, синхронное завершение и cancel.
- Смерть, UnPossess/повторный possession, смена Avatar и EndPlay: ни оставшегося движения/ADS, ни callback старого запуска на новой жизни.
- Dedicated server и два клиента: движение, ADS/смерть на proxies, отсутствие LocalPlayer-зависимостей; отдельно late join и Mixed ASC replication.
