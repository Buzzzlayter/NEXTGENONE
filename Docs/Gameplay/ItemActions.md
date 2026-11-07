# Действия предметов через GAS

Editor-сетап от 2026-11-07 выполнен: ID_Medkit содержит оба fragments ниже, а ID_Pistol/ID_Rifle/ID_Shotgun — Item Actions с NGEquipWeaponItemAbility. Blueprint-компиляция и сохранение успешны; игровой запуск не выполнялся. Подробный статус: [EquipmentInput.md](EquipmentInput.md).

## Настройка после полной сборки

В `ID_Medkit` → Fragments добавить:

1. **Item Actions** (`NGInventoryFragment_ItemActions`): Activation Ability = `NGActivateMedkitAbility` либо ваш BP-наследник этой способности.
2. **Medical Item** (`NGInventoryFragment_Medical`): Healing = 50, Activation Duration = 3 секунды (настраиваются).

Не добавлять эти способности в Alive: PlayerInventory выдаёт выбранную способность через GiveAbilityAndActivateOnce с SourceObject = конкретный item. После EndAbility spec удаляется. При создании новой аптечки без этих двух fragments она остаётся обычным loot.

Аптечка в Action: клавиша её ячейки вызывает ActivationAbility. В собственном Storage или Action: ПКМ вызывает ту же ActivationAbility. Мышиная кнопка задаётся в defaults слота → Secondary Action Button; общая RequestOperation(SecondaryAction) доступна для UI-геймпад action пользователя. Primary/Secondary — только UI-жесты, сервер их не получает и не различает. Никаких новых gamepad bindings/IMC пакет не создаёт. Не назначать Secondary Action Button равной Drag Button. Пустые/чужие/предметы сундука применять нельзя.

## Полиморфное действие

TakePickup только переносит предмет и уведомляет об изменении inventory. Подбор не активирует предмет автоматически и не проверяет оружие/свободные руки через Combat; экипировка запускается отдельной активацией предмета из UI или Action-слота.

У предмета одна ActivationAbility. Fragment читает только серверный обработчик активации; NGInventoryPanel и UIInventorySlotWidget не знают fragment и Combat. RequestActivationItem передаёт только предмет. Например, создать BP-способность экипировки шлема от **NGItemActivateAbility**, реализовать серверный ActivateAbility, прочитать **GetAssociatedItem**, вызвать механизм экипировки и завершить EndAbility. Назначить её ActivationAbility definition шлема. Отсутствие fragment означает отсутствие активации. Система слотов брони и сама экипировка шлема этим пакетом не создаются.

Способность должна быть ServerOnly и InstancedPerActor. Клиент передаёт item/ожидаемый слот; класс берётся только из definition на сервере. Сервер проверяет текущий Pawn/ASC, владение, слот, количество, состояние отказа FSM и death. Rate limit принадлежит постоянному PlayerInventory и не сбрасывается сменой предмета. Bool запроса означает отправку, не успешное завершение активации; completion RPC нет, слоты/HP обновляются репликацией.

Оружию также нужен Item Actions: ActivationAbility = **NGEquipWeaponItemAbility**. Только эта способность вызывает private Combat.EquipItemFromAbility; прямые RequestEquipItem/EquipInventoryItemOnServer/ServerEquipItem удалены. Панель, Action input и auto-equip pickup запрашивают активацию предмета без прямой экипировки. Неявного weapon fallback нет: после сборки назначить fragments в ID_Pistol/ID_Rifle/ID_Shotgun. Старые Blueprint-вызовы Combat.RequestSelectSlot заменить на PlayerState.Inventory.RequestActivationSlot, вызовы Combat.RequestEquipItem — на RequestActivationItem. Это native API; новые BP-вызовы выставлять только при необходимости.

Экипировку для отображения panel читает через общий ItemInstance.IsEquipped; это отдельный реплицируемый presentation-флаг, который выставляет владелец экипировки. Флаг не является разрешением серверной операции и не переносится в FNGItemState при drop/transfer. Панель не подписывается на Combat и не вычисляет weapon-specific состояние.

## Лечение

NGActivateMedkitAbility лечит через Instant NGHealingEffect существующий HealthSet и списывает одну аптечку из исходного instance. Здоровье ограничено MaxHealth; при полном HP и мёртвом Pawn запуск запрещён. Урон, смерть, смена Pawn, FSM failure, изменение исходного слота/количества или потеря предмета отменяют ожидание без расхода. Вход в sprint отменяет ItemActivate на сервере. Обычная ходьба разрешена. Dash/mantle/punch/reload/roll/ragdoll блокируют начало и прерывают использование при появлении соответствующего GAS-тега.

State.ActivatingItem реплицируется медицинским GE; пока он активен, Fire/Reload/Aim заблокированы. UI-инвентарь сам блокирует gameplay input, но не отправку UI SecondaryAction. Анимация, progress widget и montage в пакет не входят. Для их оформления можно отнаследовать способность; завершение и очистка должны вызывать родительскую реализацию.

В конце сервер заново проверяет тот же Avatar/предмет/слот/количество и здоровье. Общий ConsumeItem выполняет debit и синхронный healing credit до inventory notification, с защитой от повторной мутации. Старый Pawn не может лечить новую жизнь. Если позже появятся immunity/условные healing GE, потребуется явный контракт успешного лечения перед расходом; текущий native GE без immunity-условий.

## Проверки

Выполнены статический просмотр и сверка используемых GAS API с локальным UE 5.8.4. Сборка, Blueprint authoring, PIE и сетевые тесты не запускались. После отдельного разрешения проверить: own Storage/Action, ПКМ/клавиша ячейки, полный HP, последний предмет/стек, перенос и drop во время ожидания, урон/смерть/respawn, spam двух actions, UIOnly, клиент + dedicated server, обновление HP/количества на UI. Пользовательский ID_Medkit нужно настроить после сборки по разделу выше.
