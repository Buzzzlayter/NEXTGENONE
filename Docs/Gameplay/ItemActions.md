# Действия предметов через GAS

## Настройка после полной сборки

В `ID_Medkit` → Fragments добавить:

1. **Item Actions** (`NGInventoryFragment_ItemActions`): Primary Ability и Secondary Ability = `NGUseMedkitAbility` либо ваш BP-наследник этой способности.
2. **Medical Item** (`NGInventoryFragment_Medical`): Healing = 50, Use Duration = 3 секунды (настраиваются).

Не добавлять эти способности в Alive: PlayerInventory выдаёт выбранную способность через GiveAbilityAndActivateOnce с SourceObject = конкретный item. После EndAbility spec удаляется. Native-типы недоступны в Editor до сборки; этот пакет не назначает fragments в бинарном ID_Medkit. Существующая аптечка без этих двух fragments пока остаётся обычным loot.

Аптечка в Action: клавиша её ячейки вызывает Primary Ability. В собственном Storage или Action: ПКМ вызывает Secondary Ability. Мышиная кнопка задаётся в defaults слота → Secondary Action Button; общая RequestOperation(SecondaryAction) доступна для UI-геймпад action пользователя. Никаких новых gamepad bindings/IMC пакет не создаёт. Не назначать Secondary Action Button равной Drag Button. Пустые/чужие/предметы сундука применять нельзя.

## Полиморфное действие

Item Actions хранит две независимые ссылки на классы GAS. Новый предмет не требует ветки в виджете или RPC. Например, создать BP-способность экипировки шлема от **NGItemUseAbility**, реализовать серверный ActivateAbility, прочитать **GetAssociatedItem**, вызвать существующий механизм экипировки и завершить EndAbility. Назначить её нужному действию definition шлема. Пустой action означает отсутствие действия. Система слотов брони и сама экипировка шлема этим пакетом не создаются.

Действия должны быть ServerOnly и InstancedPerActor. Клиент передаёт item/ожидаемый слот и Primary/Secondary; класс берётся только из definition на сервере. Сервер проверяет текущий Pawn/ASC, владение, слот, количество, состояние отказа FSM и death. Rate limit принадлежит постоянному PlayerInventory и не сбрасывается сменой предмета. Ответ запроса подтверждает **активацию**, не успешное завершение задержанного лечения; слоты/HP обновляются репликацией.

Старые weapon definitions без Item Actions продолжают использовать существующее equip-действие. Если fragment назначен, он имеет приоритет, автоматического fallback на equip при пустом/ошибочном action нет.

## Лечение

NGUseMedkitAbility лечит через Instant NGHealingEffect существующий HealthSet и списывает одну аптечку из исходного instance. Здоровье ограничено MaxHealth; при полном HP и мёртвом Pawn запуск запрещён. Урон, смерть, смена Pawn, FSM failure, изменение исходного слота/количества или потеря предмета отменяют ожидание без расхода. Вход в sprint отменяет ItemUse на сервере. Обычная ходьба разрешена. Dash/mantle/punch/reload/roll/ragdoll блокируют начало и прерывают использование при появлении соответствующего GAS-тега.

State.UsingItem реплицируется медицинским GE; пока он активен, Fire/Reload/Aim заблокированы. UI-инвентарь сам блокирует gameplay input, но не отправку UI SecondaryAction. Анимация, progress widget и montage в пакет не входят. Для их оформления можно отнаследовать способность; завершение и очистка должны вызывать родительскую реализацию.

В конце сервер заново проверяет тот же Avatar/предмет/слот/количество и здоровье. Общий ConsumeItem выполняет debit и синхронный healing credit до inventory notification, с защитой от повторной мутации. Старый Pawn не может лечить новую жизнь. Если позже появятся immunity/условные healing GE, потребуется явный контракт успешного лечения перед расходом; текущий native GE без immunity-условий.

## Проверки

Выполнены статический просмотр и сверка используемых GAS API с локальным UE 5.8.4. Сборка, Blueprint authoring, PIE и сетевые тесты не запускались. После отдельного разрешения проверить: own Storage/Action, ПКМ/клавиша ячейки, полный HP, последний предмет/стек, перенос и drop во время ожидания, урон/смерть/respawn, spam двух actions, UIOnly, клиент + dedicated server, обновление HP/количества на UI. Пользовательский ID_Medkit нужно настроить после сборки по разделу выше.
