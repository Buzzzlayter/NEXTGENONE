# Компоненты поведения AI

GOAP приостановлен и изолирован в NGAIGOAPComponent.h/.cpp.
Текущий статус и удалённые точки интеграции: [BotGOAPExecution.md](BotGOAPExecution.md).
Для Simple не нужны planner, executor, dispatcher или специальные наследники задач.

На общем Behavior-состоянии можно настроить:

- **AI EQS Results** — NGAIQueryResultsComponent для результатов вложенных запросов.
- **AI Observe Target** — NGAIObserveTargetComponent для наблюдения за выбранной целью.

Memory остаётся ActorComponent контроллера и обновляется на сервере.
Контекст Behavior, компонентов и action-состояний — ANGAIController.

EQS Results очищается при Enter/Exit содержащего состояния. Задачи находят storage
вверх по HFSM-иерархии. Для независимых запросов назначать разные ResultKey.
Callback проверяет сессию storage, Pawn и AvatarGeneration. Соседняя FSM может читать
результат через общего родителя. Позиция является снимком, автоматического TTL нет.

Move принимает Location через граф как Point с bRelativeToPawn=false.
Полная настройка: [BotEQS.md](BotEQS.md).

Observe Target подписывается на обновления памяти при Enter и снимает подписку
при Exit. Его focus ниже приоритета FireBurst; компонент не активирует GAS Aim.

Сборку выполняет пользователь. Статический просмотр не заменяет проверки в игре.
