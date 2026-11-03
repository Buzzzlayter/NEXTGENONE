# GameplayFoundation runtime module

2026-11-02. Общий модуль выделен; следующим пакетом добавлена новая обработка input. FSM персонажа подключена; актуальный контракт в Docs/Gameplay/CharacterStateMachine.md.

## Границы

`NEXTGENONE → GameplayFoundation`. У нового модуля нет зависимостей на NEXTGENONE, ALS, UIModule или GCFSM. UI-модуль в этом пакете не меняется.

Структура Source/GameplayFoundation:

- Public/AbilitySystem/NGAbilitySystem.h — ASC с tag input, HealthSet, init/damage effects, прямые grant/handle structs и экспортируемые native tags.
- Public/Character/NGCharacterDefinition.h — InputConfig, без базовой выдачи способностей; без привязки к классу Pawn.
- Public/Input/NGGameplayInputConfig.h — только Gameplay IA/tag bindings и editor-валидация маршрутов; IMC/priority принадлежат ControlStateDefinition.
- Public/Input/NGGameplayInputComponent.h — повторно используемые Pawn bindings, tag-based GAS input, симметричный release/shutdown. Не зависит от UI/ALS.
- Private — реализации и регистрация runtime-модуля.

Проектный ALS/Combat input adapter находится в `Source/NEXTGENONE/Gameplay/Input/NGCharacterInputComponent.h/.cpp`. Конкретные способности остаются в NEXTGENONE; в GameplayFoundation хранятся общие классы. Перемещение файлов внутри одного модуля не меняет /Script-пути и не требует Core Redirects.

В NEXTGENONE остаются ANextgenoneCharacter/PlayerState/Controller, конкретные способности, Combat, Inventory, Interaction, ALS-адаптеры и инструменты миграции. Переносить ALS-зависимые классы в общий модуль сейчас означало бы перенести и их зависимости. Новые общие классы персонажа/abilities размещать в GameplayFoundation только при отсутствии этих зависимостей.

## Единственный ASC

PlayerState по-прежнему создаёт один default subobject AbilitySystem. Его реализация теперь — UNGPlayerAbilitySystemComponent (NEXTGENONE), производная от перенесённого UNGAbilitySystemComponent. Тип публичного поля остаётся базовым.

Проектный наследник сохраняет прежний специальный маршрут Interact/Pickup: находит InteractionComponent текущего Avatar и вызывает RequestInteract. Это не вторая система и не второй ASC. Общий модуль умеет только tag-based activation и не знает о взаимодействии с миром.

HealthSet больше не приводит Avatar к ANextgenoneCharacter. Урон проверяет authority, текущий Avatar, совпадение SourceObject контекста с поражённым Avatar, положительное текущее здоровье, отсутствие State.Dead и конечную положительную величину урона. Проверки актуальности Pawn/LifeSerial в проектном damage path сохраняются. Respawn не должен позволять попаданию в старое тело менять здоровье нового Avatar.

## Ссылки и загрузка

Первоначально в DefaultEngine.ini были добавлены точечные ClassRedirects и StructRedirects из /Script/NEXTGENONE в /Script/GameplayFoundation. По отдельному поручению пользователя 39 проектных ассетов пересохранены и загружены свежим процессом без этих redirects; соответствующие 11 строк удалены. Подробности и backup — в [CoreRedirectMigration.md](CoreRedirectMigration.md). Имена самих классов/структур не менялись. Устаревшие копии исходников удалены, внешние includes обновлены.

Модуль зарегистрирован в uproject и является public dependency NEXTGENONE. Все существующие targets получают его через зависимость основного модуля; отдельный executable/target не нужен.

При первоначальном переносе Blueprint/Data Assets не пересохранялись; последующая ограниченная миграция описана выше. Компонентные archetypes PlayerState и сетевое поведение в игре ещё не проверены. После обновления проекта в IDE новые файлы находятся в Source/GameplayFoundation. Новые типы ассетов доступны после полной сборки и запуска редактора.

## Следующий пакет

Актуализация 2026-11-03: FSM уже подключена. Core ControlStateDefinition содержит StateTag, необязательный PresentationCue, прямые Abilities/Effects и MappingContexts. AbilitySet удалён; CharacterDefinition хранит только InputConfig. Проектные GA_Death, DeadStateEffect и ALS cue остаются в NEXTGENONE. Общий модуль не зависит от ALS/UI/NEXTGENONE. Текущий контракт: CharacterStateMachine.md; старые заметки ниже описывают предыдущие этапы.

Сборка NEXTGENONEEditor Win64 Development: PASS (UBT exit 0), включая компиляцию проектного ASC и линковку GameplayFoundation. Лог: Saved/Logs/NGGameplayFoundationModuleBuild.log. PIE, игровые/сетевые тесты и загрузка Blueprint через redirects не запускались.

Input/config реализованы отдельным пакетом: общий Pawn input component, проектный ALS/Combat адаптер, координация через UPlayerInputSubsystem и удаление hardcoded action bindings из Controller. Затем подключён CharacterDefinition.AbilitySets: общие validation/grant helpers в GameplayFoundation, server lifecycle и владение handles в существующем PlayerState. Старые C++ fallback выдачи больше не выполняются. Сетап и ограничения — в CharacterInputSetup.md. FSM и перенос её общих классов ещё не выполнены. Разрешение на игровые/сетевые тесты не выдавалось.
