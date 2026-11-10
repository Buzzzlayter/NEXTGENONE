# ALS: клиентские команды и серверная власть

Изменения исходников ALS разрешены пользователем для закрытия унаследованных RPC. AnimBP не менялся. GAS отвечает за разрешение и жизненный цикл действий; CharacterMovement сохраняет свою сетевую модель движения. Пересылать каждый transform через GAS не требуется.

## Разрешённые входы клиента

| Команда | Проверка и исполнитель |
| --- | --- |
| Активация выданной GA по input tag | Серверный GAS проверяет spec, текущий Avatar, здоровье, состояние, cooldown |
| Повторное нажатие Ragdoll | Generic GAS input event активной способности; завершение разрешает сервер после cooldown |
| DesiredStance | Только Standing/Crouching; общий server state gate |
| DesiredGait | Только Walking/Running/Sprinting; допустимую скорость отдельно вычисляет сервер |
| DesiredRotationMode | Только VelocityDirection/LookingDirection; Aiming из этого RPC запрещён |
| ViewMode | Только FirstPerson/ThirdPerson; клиентский bForce игнорируется |
| Движение/взгляд | Существующий CharacterMovement/Controller; полный аудит движковых RPC этим пакетом не выполнялся |

Четыре оставшихся ALS state RPC используют CanAcceptClientLocomotion и отдельные лимиты по виду команды (ClientStateCommandInterval, default 0.03 s). В проектном Character проверяются привязанный ASC/Avatar, положительное здоровье, отсутствие Dead/Dash/Mantle/Roll/Ragdoll. Параметр интервала должен быть конечным и не меньше 0.01 s. Последнее валидное намерение каждого из четырёх видов сохраняется в фиксированной очереди и применяется после снятия временного state gate и истечения интервала. Более новое намерение заменяет старое; смена Controller сбрасывает очередь. Невалидные enum по-прежнему отклоняются до постановки в очередь. Это ограничение применения состояния, а не защита от любого сетевого flood.

## Удалённые клиентские RPC

Server_SetMeshLocationDuringRagdoll, Server_PlayMontage, Server_RagdollStart, Server_RagdollEnd, Server_SetVisibleMesh, Server_SetRotationMode, Server_SetOverlayState, Server_SetAllowedGait и Server_MantleStart удалены из отражаемого API плагина вместе с реализациями/вызовами.

Клиент больше не задаёт серверу ragdoll-координаты, montage/play rate, skeletal mesh, overlay, aiming или готовый mantle ledge. Серверные multicasts для presentation сохранены как внутренний транспорт ALS; их вызов клиентом не является командой серверу. SetVisibleMesh теперь принимает изменения только на authority, VisibleMesh реплицируется также владельцу. Нового клиентского API выбора внешности нет.

AllowedGait вычисляется в обычном серверном ALS UpdateCharacterMovement; отдельного RPC, позволяющего назначить серверу максимальный gait, больше нет. MantleCheck сохраняет оригинальную геометрию ALS, а ReplicateMantleStart — обычный virtual callback, не RPC. Проектный executor распространяет только синхронный результат собственной server-side проверки GA_Mantle.

### Исправление speed update / replay — 2026-11-07

Смена AllowedGait и MovementSettings сразу применяет MaxWalkSpeed/MaxWalkSpeedCrouched. Compressed flags объединяются с локальным запросом обновления: клиентский move без Custom_0 больше не стирает ожидающее серверное изменение. PrepMoveFor восстанавливает gait через setter вместе со скоростью; CanCombineWith запрещает объединять moves с разными gait/settings-change flags. SavedAllowedGait остаётся локальной историей клиента и не становится доверенным серверным параметром.

Обнаруженный дефект мог оставлять server speed cap старым при клиентском спринте. Это согласуется с сообщением пользователя о рывках autonomous Pawn; runtime-корреляция не установлена. ALS считает lean по изменению скорости, поэтому наклон может реагировать на correction/replay; AnimBP и коэффициенты lean не менялись. Сборка и воспроизведение не выполнялись. Нужна отдельная проверка переходов walk/run/sprint и replay на сервере + owning client; порядок доставки locomotion intents и CMC moves этим патчем полностью не унифицирован.

Пользователь после сборки подтвердил устранение описанных рывков и наклона на сетевом управляемом персонаже. Это ручное подтверждение конкретного симптома, не полный regression или профилирование; агент собственный прогон не выполнял.

## Действия через GAS

- UNGRollAbility: ServerInitiated/ServerOnlyTermination. Сервер валидирует и начинает действие, клиент не задаёт ему asset/rate. Montage берётся из authored GetRollAnimation, проверяется skeleton; PlayRate берётся только из ability defaults, проверяется конечность и диапазон 0.1..3. Воспроизведение и root motion проходят через AbilityTask_PlayMontageAndWait/ASC. Клиентский экземпляр также выполняет task: обычная ASC montage replication пропускает owning client. Старый Replicated_PlayMontage у проектного Character передаёт только намерение Input.Roll, игнорируя asset/rate.
- UNGRagdollAbility: ServerInitiated/ServerOnlyTermination. На сервере применяет Infinite UNGRagdollStateEffect с State.Ragdoll. Character подписывается на этот тег и использует исходные локальные RagdollStart/End ALS. Клиентский экземпляр GA нужен для пересылки generic input events; сам эффект клиент не применяет. Снятие эффекта при отмене/удалении способности завершает живой ragdoll. На смерти get-up не вызывается; существующая FSM → GA_Death → cue сохраняется.
- Обе способности используют серверный UNGALSActionCooldownEffect, default 0.3 s. Он хранится на постоянном ASC, не в локальном таймере ability, поэтому окончание/снятие grant не обнуляет действующий cooldown.
- Живой ragdoll не помечен Ability.Type.Movement: локальная блокировка gameplay-ввода UI не должна завершать его серверный lifetime. Он имеет Ability.Type.Ragdoll; Jump на сервере может завершить эту способность для get-up после той же проверки cooldown, что и повторный Ragdoll input (общая CanRequestGetUp). Смерть/смена Avatar/снятие Alive grants отменяют её обычным GAS lifecycle.
- Aim/Fire/Reload/Interaction/Mantle/Dash учитывают новые несовместимые состояния. У BP с ручным override InterruptingTags следует добавить State.Roll и State.Ragdoll.

Тело живого ragdoll следует серверному TargetRagdollLocation из серверной физики; клиентские координаты удалены. Позиции/rotation проверяются на нечисловые/бесконечные значения до trace и SetActorLocation. Поза и кости трупа по-прежнему рассчитываются локально; dead Tick не следует этому target. Переход из живого ragdoll в смерть также выполняет отделение меша и передачу смертельного импульса.

## Сетап и проверка

1. Выполнить полную сборку и перезапуск Editor: удалены отражаемые RPC, добавлены native GA/GE. Live Coding для изменения отражаемой структуры не использовать.
2. В Alive ControlStateDefinition.Abilities выдать UNGRollAbility с Input.Roll и UNGRagdollAbility с Input.Ragdoll. Не выдавать их из конструктора Pawn или PlayerState.
3. В InputConfig добавить нужные IA в AbilityInputActions; физические клавиши/геймпад задаются в IMC. Старый Native Input.Ragdoll временно пересылает press/release в GAS; оставить только один маршрут. ALS double-tap stance и breakfall передают Input.Roll через проектный bridge. Для осознанного отдельного roll input можно назначить IA → Input.Roll.
4. Проверить authored GetRollAnimation, mesh/PhysicsAsset и GetGetUpAnimation. Если GA не выданы, старый RPC-путь их не заменяет: соответствующее действие не исполнится.

Выполнены только статический просмотр, сверка API с локальными исходниками UE 5.8 и поиск оставшихся ссылок. В сохранённых project/ALS Blueprint-ассетах строковых ссылок на удалённые RPC не найдено; это не заменяет компиляцию Blueprint. UBT/Live Coding/PIE/сеть не запускались по правилу пользователя. Не подтверждены: сборка новых типов; 2 клиента + dedicated server; activation/get-up с задержкой; отмена ragdoll при смерти/respawn; roll root motion у owning client; late join; отрицательные проверки прямых RPC и spam. Протокол несовместим со старой сборкой клиента — все участники должны обновиться.
