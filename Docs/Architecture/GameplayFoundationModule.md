# Границы runtime-модулей

- **GameplayFoundation** — общее ядро GAS, здоровья, grants, конфигов, tag input и инвентаря. Не зависит от NEXTGENONE, ALS, UI или GCFSM.
- **UIModule** — общие виджеты, маршрутизация UI-ввода и HFSM-механизмы. Не зависит от проектных игровых классов.
- **NEXTGENONE** — конкретные способности, правила игры, оружие, AI и интеграция ALS/UI.

ASC и HealthSet принадлежат PlayerState; Pawn — текущий Avatar. Способности и эффекты
состояний выдаются и снимаются по handles соответствующего источника. Gameplay input
передаёт press/release по тегам; не выполняет операции Combat или Inventory.

Ядро инвентаря не знает WeaponDefinition. Runtime-состояние — FastArray стабильных
слотов и item instances как subobjects. Проектные fragments и способности связывают
предметы с экипировкой. Player RPC и storage-сессия принадлежат проектному
PlayerInventory; общий InventoryManager выполняет операции над предметами.

Практический сетап: [ввод](../Gameplay/CharacterInputSetup.md),
[Character FSM](../Gameplay/CharacterStateMachine.md),
[инвентарь](../Gameplay/InventoryContainers.md), [экипировка](../Gameplay/Equipment.md).
Изменение native-модуля требует миграции ссылок ассетов до удаления Core Redirects.
