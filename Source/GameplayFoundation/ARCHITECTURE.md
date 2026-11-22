# GameplayFoundation

Общее runtime ядро GAS, ввода и инвентаря. Модуль зависит от Unreal Engine, GameplayAbilities, Enhanced Input и сетевых API. `NEXTGENONE` использует его контракты. В `GameplayFoundation` не добавляем зависимости на проектные классы, ALS, `UIModule` или GCFSM.

Перед изменениями прочитай [общие правила работы с проектом](../../Docs/WorkingAgreement.md).

## GAS и ввод

- Модуль определяет общие атрибуты здоровья, базовые эффекты, конфиги состояний и контракты ASC. Владение ASC и выбор текущего Avatar определяет [игровой модуль](../NEXTGENONE/ARCHITECTURE.md).
- Общие GAS теги объявляются в [NGGameplayTags.h](Public/GameplayTags/NGGameplayTags.h) и определяются в соответствующем `.cpp`. Потребители подключают этот контракт.
- `UNGGameplayInputConfig` связывает Input Action с тегом. `UNGGameplayInputComponent` передаёт press/release по тегу. Физические клавиши задаются в IMC, боевые и инвентарные операции исполняются способностями.
- `UNGControlStateDefinition` описывает выдачу способностей, эффекты и gameplay IMC. Игровая FSM применяет ресурсы состояния и снимает их по handles соответствующего источника.

## Инвентарь

- `UNGInventoryManagerComponent` владеет содержимым контейнера и выполняет серверные операции над предметами. Доступ игрока, RPC и сессия просмотра хранилища относятся к игровому модулю.
- `UNGInventoryLayoutDefinition` задаёт схему слотов. Стартовое содержимое задаётся отдельно. В runtime источником состояния служит один FastArray со стабильными индексами. Пустые слоты и их правила сохраняются при переносе.
- Экземпляры предметов реплицируются как subobjects. Определения и фрагменты задают данные предмета. Связь с оружием и экипировкой добавляется проектными фрагментами и способностями.

Практический сетап: [инвентарь](../../Docs/Gameplay/InventoryContainers.md), [действия предметов](../../Docs/Gameplay/ItemActions.md), [ввод](../../Docs/Gameplay/CharacterInputSetup.md).
