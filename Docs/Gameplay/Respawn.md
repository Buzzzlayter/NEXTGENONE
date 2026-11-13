# Простой респаун игрока

## Сервер

После RestartPlayer GameMode устанавливает одну подписку на OnStateChanged Character FSM.
Переход в Dead запускает один таймер респауна на игрока. Обратных вызовов из абилки
или персонажа в GameMode нет; подписки на OnResourcesReady нет.
Значения Class Defaults → NEXTGENONE / Respawn:

- RespawnDelay = 3 секунды после перехода Character FSM в Dead;
- CorpseLifetime = 15 секунд после снятия possession.

В callback повторно проверяются Controller, тот же Pawn, possession и Dead.
GameMode снимает possession, затем вызывает штатный RestartPlayer. Точка появления
выбирается штатным GameMode через PlayerStart; класс берётся из DefaultPawnClass.
При неудачном spawn пишется warning; бесконечных повторов нет.

Старый Pawn освобождает GAS-ресурсы через UnPossessed. PlayerState и ASC сохраняются;
существующий StartLife привязывает новый Avatar и применяет InitHealthEffect один раз.
Инвентарь сохраняется на PlayerState, стартовые предметы повторно не выдаются.
GameMode не создаёт оружие и не дублирует систему здоровья. Поздняя очистка старого
тела защищена существующими identity/generation-проверками текущего Avatar.

Logout, EndPlay и внешний RestartPlayer снимают принадлежащие GameMode таймеры.
Смерть запускается существующим Character Dead → GA_Death → ALS ragdoll.
GameMode не отправляет события UI и не запускает death ability. Способность смерти
не знает о GameMode или респауне; задержка и политика находятся только в GameMode.

## Настройка UI после пользовательской сборки

Добавить **Observe Player Lifecycle** (`UNGPlayerLifecycleComponent`) в Components
общего UI root, который остаётся активным в WaitForReady, игровом и Dead-состояниях.
Не размещать наблюдатель внутри состояния, которое уничтожается при смерти.

| Поле | Значение по умолчанию | Переход |
| --- | --- | --- |
| DeathEvent | Dead | Игровое составное состояние → экран смерти |
| PawnChangedEvent | PawnChanged | Экран смерти/игровое состояние → WaitForReady |

```text
UI Root (Observe Player Lifecycle)
  Entry → WaitForReady --Ready→ Gameplay
  Gameplay --Dead→ Dead
  Dead --PawnChanged→ WaitForReady
  Gameplay --PawnChanged→ WaitForReady
```

В WaitForReady оставить существующий `UNGWaitForReadyComponent`, RequiredCharacterState
= State.Alive, ReadyEvent = Ready. Если PawnChanged приходит повторно, пока WaitForReady
уже активен, он не должен отправлять UI в Gameplay: дождаться Ready.
DeathEvent также обработать из WaitForReady на случай смерти до готовности UI.

Gameplay должен быть общим родителем игровых экранов, включая инвентарь и storage:
выход по Dead демонтирует их через обычный Exit маунтеров. В Dead при необходимости
добавить HFSMInputComponent с UI mapping/policy и блокировкой gameplay. При выходе
его policy снимается штатно. Ручные input-флаги на Controller не нужны.

Наблюдатель подписывается только на своего локального Controller и текущий Pawn.
Он отправляет Dead по Character FSM и PawnChanged по штатному OnPossessedPawnChanged.
Старый Pawn отписывается при смене. Готовность он не вычисляет и Ready не отправляет.
HUD/Controller штатным респауном не пересоздаются: игровые виджеты пересоздаются
маунтерами при повторном входе Gameplay после WaitForReady. Если HUD пересоздаётся
внешней логикой, новый UI root заново подписывается и проверяет текущий Dead-снимок.

## Границы и проверки

- Респаун включён для PlayerController, чьи Pawn созданы через RestartPlayer этого
  GameMode. AIController этим пакетом не респаунится.
- В момент таймера проверяется Dead и разрешение handoff. При зависшем входе FSM
  новый Avatar не создаётся. Зависший граф нужно исправить; автоматического опроса нет.
- Нет подтверждения от клиента, что экран смерти уже нарисован; задержка серверная.
- Нет spectator/free camera, сброса инвентаря, переноса лута в труп или persistence.
- Сохранённый инвентарь обновляет UI через повторный SetInventorySource и существующие
  события; виджеты сами readiness не отслеживают.

Выполнен только статический просмотр C++ API и diff. UBT, Live Coding,
Blueprint-компиляция, PIE и сетевые проверки агентом не запускались. Ассеты не менялись.
После пользовательской сборки и настройки UI проверить отдельно: owning client/listen
host, repeated death/respawn, смерть с открытым storage, logout в задержке, смену Pawn
до таймера, попадание в старый труп, отсутствие дублей grants/starter inventory и свежую
привязку камеры/input/ASC. Два клиента и dedicated server пока не проверены.
