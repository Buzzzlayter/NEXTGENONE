# Настройка поведения бота

Актуальное изменение lifecycle: Behavior стал корневым HFSMState, запускаемым штатной FSM контроллера; прежние инструкции Start/Stop ниже требуют миграции на OnEnter/OnExit. См. [BotBehaviorHost.md](BotBehaviorHost.md). Ассеты ещё не мигрированы агентом.

## Текущий контракт

AI-граф размещается в отдельном Blueprint от `UNGAIBehavior`, а не в AIController Blueprint. Контроллеру назначается `BehaviorClass`. Для разных поведений создаются разные классы объектов; один класс можно назначить нескольким контроллерам — runtime-экземпляры независимы. Один экземпляр живёт вместе с контроллером и не пересоздаётся при потере готовности или смене Pawn.

Controller creates the Behavior object once. Pawn initialization waits for ready Character FSM resources, then calls BP Start and installs runtime state/failure observers. These observers do not rerun initialization. BP Stop handles leaving Alive, resource failure, UnPossess and EndPlay; the next possession initializes its Pawn separately. No readiness polling or defensive start/stop flags.

Запуск, пауза и завершение GCFSM задаются пользователем внутри BP-событий Start/Stop. C++ не запускает, не пересоздаёт и не освобождает граф автоматически. Controller не хранит ActiveAction и не регистрирует действия. Ресурсами владеют состояния: Finish/Exit очищают только собственные path request, input, делегаты и таймеры. Объект поведения и его переменные сохраняются; память восприятия имеет собственный lifecycle привязки к Pawn.

Добавления ReleaseContext/RemoveContext в GCFSM убраны. Публичный UGCFSM::Stop нельзя вызывать на машине с активным состоянием: остановку описывать корректными переходами графа либо паузой.

## Настройка после пользовательской сборки

Для разных размещённых NPC с одним Controller Blueprint используется native-наследник `ANGBotCharacter` от `ANextgenoneCharacter`. Только у бота есть `AIBehaviorClass` с EditAnywhere: можно настроить default Bot Blueprint и отдельно экземпляры уровня через Details → AI → AI Behavior Class. None использует BehaviorClass контроллера. Контроллер выбирает класс при possession до ожидания готовности; тот же класс сохраняет существующий объект поведения, другой класс создаёт новый после Stop прежнего. Изменение поля во время игры не переключает граф автоматически. Для Pawn другого типа используется default контроллера.

После пользовательской сборки открыть BP_NGBotCharacter → Class Settings → Parent Class = NGBotCharacter, скомпилировать и сохранить. Сохранить существующие графы Character FSM, Mesh, AnimBP и AIControllerClass. В ANextgenoneCharacter поля поведения больше нет.

1. Для своего поведения создать Blueprint от `NGAIBehavior`; готовый пример — `BP_AIBehavior_Enemy`.
2. Event Start запускает GCFSM, Event Stop отправляет событие перехода из рабочих состояний через Exit к FSM Stop. Pause FSM сохраняет активные ресурсы и не заменяет завершение.
3. Назначить BehaviorClass в `BP_NGAIController` либо AI Behavior Class на экземпляре `BP_NGBotCharacter`. Controller Blueprint не должен содержать граф поведения.
4. На Bot Pawn: Parent Class=`NGBotCharacter`, AIControllerClass=`BP_NGAIController`, AutoPossessAI=`PlacedInWorldOrSpawned`, AutoPossessPlayer=`Disabled`. Сохранить ALS AnimBP и Character FSM Alive/Dead.
5. Настроить inventory layout и InitialContents с оружием/патронами. Способности выдаются Character states и equipment, а не AI-контроллером. Для движения нужен NavMesh.

У `UNGAIActionState` Context теперь типа `UNGAIBehavior`. GCFSM присваивает его до Enter. Состояние получает контроллер через объект поведения, без GetFirstPlayerController и без LocalPlayer. Context не является параметром ExposeOnSpawn.

Для паузы выбрать native-класс `NGAIPauseState`: единственный выход Succeeded, параметр Cooldown (по умолчанию 1 с). Отсчёт идёт через Tick FSM и останавливается вместе с графом; отдельного таймера, GAS и ActionTimeout нет. Нулевое значение завершается на ближайшем Tick; отрицательное/нечисловое нормализуется в ноль с предупреждением. Нода напрямую наследует UHFSMState. Отдельный Blueprint паузы не требуется; старые ноды FSM_AI_Pause заменить при миграции графа.

## Сохранённые ассеты

`BP_AIBehavior_Enemy` содержит две параллельные машины навигации и оружия. Event Start запускает обе; Event Stop отправляет общее событие Stop, обработанное каждой машиной. Актуальные связи и ограничения описаны в [BotCombat.md](BotCombat.md). `BP_AIBehavior_Simple` — отдельное пользовательское поведение.

Выбор ImplementationClass через MCP вызывает crash при реконструкции пинов в текущем инструменте. Класс выбирать вручную до исправления инструмента; исходники движка в этом пакете не изменялись.

## Ноды и выходы

Для универсальных пространственных запросов используется `NGAIQueryState`: EQS asset → именованный результат → Move с DestinationType=`QueryResult`. Настройка и контракт результатов: [BotEQS.md](BotEQS.md).

Иерархия плоская: FindTarget, Move, Ability и FireBurst напрямую наследуют NGAIActionState. Пауза напрямую наследует UHFSMState. Для всех вариантов движения используется NGAIMoveState с DestinationType (Point/LastKnownTarget/Reposition). Общая механика удержания GAS вынесена в обычную структуру FNGAIAbilityInput, без дополнительных UObject-баз. У FireBurst свой BurstDuration; наследуемых InputTag/HoldDuration больше нет.

Действия — native-наследники UHFSMState с параметрами EditAnywhere / BlueprintReadWrite / ExposeOnSpawn на нодах. У каждой ноды должен быть Implementation Class. Значения задаются до Enter; не менять параметры владения во время исполнения.

`GetDeclaredTransitionEvents()` объявляет Succeeded/Failed на CDO; редактор добавляет их при реконструкции. Внешние события можно добавлять вручную. Совпадающие связи сохраняются, исчезнувшие связанные пины остаются orphan до исправления графа.

Move владеет одним path request, Ability — одним уже выданным InstancedPerActor spec и его input lifetime. Старые callbacks проверяют Pawn, ASC/AvatarGeneration, possession, экземпляр поведения и запуск состояния. Старое поведение не может отправить результат в новую машину.

## Проверки

Перед остановкой Controller снимает подписки и очищает ObservedCharacterFSM; только затем вызывает BP Stop. Без актуальной привязки получение ready ASC и повторный запуск невозможны. Штатные HasActorBegunPlay/IsActorBeingDestroyed закрывают обращения вне lifecycle Actor; отдельного bStopping нет.

Выполнен статический просмотр исходников/API и diff; граф BP_AIBehavior_Enemy скомпилирован и сохранён через Editor MCP, связи прочитаны обратно. Пользователь ранее подтверждал работу прежнего Move/Jump-графа с NavMesh; это не проверка нового объекта поведения или боевого пакета.

Сборку выполняет пользователь. PIE, сетевые проверки и smoke tests этого пакета не запускались. Проверить после отдельного разрешения: первый запуск без Wait, два бота с общим BehaviorClass, разные классы поведения, смерть/UnPossess внутри ability, новый Avatar с тем же объектом поведения, корректность BP Start/Stop и world teardown.

После изменения выходов собрать C++ и выполнить Refresh/Reconstruct Nodes. У действий остаются Succeeded/Failed; отмена хранится как внутренний Aborted, но в графе идёт через Failed. Связанные старые пины Aborted редактор сохраняет как orphan: перенести их связи на Failed и затем удалить orphan. Пользовательские события не удаляются автоматически.


### Initialization and runtime events

`BindPawn` subscribes to resource readiness and checks the initial snapshot once. `InitializeGameplay` removes that initialization subscription when the current Avatar is ready, then installs state-change/resource-failure subscriptions and invokes BP Start once. Runtime events do not repeat initialization or rescan readiness. Leaving Alive or a resource failure invokes BP Stop and removes runtime subscriptions. A new possession establishes a fresh initialization subscription; the Behavior object is retained. No bChangingBehavior/bBehaviorStarted, next-frame retry or recurring readiness polling. State lifecycle and resource cleanup remain the graph's responsibility.

### Остановка Behavior и очистка действий

`OnStopBehavior` в Controller Blueprint должен отправлять событие выхода в машину,
содержащую активное Behavior-состояние. Переход из Behavior завершает вложенные FSM
через штатный `UGCFSMState::PrivateExit`, который вызывает `Exit` всех активных задач.
Просто отправить несвязанный event или очистить память недостаточно: переход должен
действительно покинуть Behavior. Отдельный вызов FSM Stop у каждой вложенной задачи
для этого не нужен. Плагин GCFSM не изменялся.

`UNGAIBehavior::CancelActions` и рекурсивный обход задач удалены. Если такой вызов есть
в пользовательском BP, убрать его и сохранить переход выхода из Behavior. Статический
поиск имени в сохранённых ассетах Content/Gameplay/AI совпадений не нашёл; это не
заменяет проверку Blueprint-компиляцией после пользовательской C++-сборки.

`UNGAIActionState` разделяет завершение и очистку:
- `FinishAction` освобождает ресурсы и откладывает Succeeded/Failed на следующий кадр,
  чтобы не выполнять переход внутри Enter или синхронного GAS callback.
- `Exit` вызывает приватный `ResetAction`: инвалидирует RunSerial, снимает таймеры
  и освобождает незавершённую работу без создания callback результата.
- ActionTimeout обслуживается одним одноразовым таймером. Общий опрос readiness/Pawn
  каждые 0,1 с и Deadline удалены. Проверки Pawn/AvatarGeneration/RunSerial сохранены
  в асинхронных callbacks. Оперативную отмену при смерти/UnPossess обеспечивает выход
  из Behavior, а не watchdog.

Таймеры самих действий (Fire, удержание ability input, поиск цели) этим пакетом не
менялись. Выполнены статический просмотр и diff --check; сборка и игра не запускались.
