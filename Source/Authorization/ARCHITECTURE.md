# Authorization

`Authorization` — runtime-модуль авторизации и подключения игроков. Подсистема [UNGConnectionSubsystem](Public/Online/NGConnectionSubsystem.h) живёт в `GameInstance` и хранит состояние входа и группы при переходе между картами.

## Зависимости

[`UIModule`](../UIModule/ARCHITECTURE.md) использует публичный API `Authorization` через `UNGConnectionPanel`. Панель отправляет команды подсистеме и подписывается на `OnStatusChanged` и `OnPartyChanged`. [`NEXTGENONE`](../NEXTGENONE/ARCHITECTURE.md) монтирует панель в экран **Esc → Настройки** через `UNGSettingsScreenMounter`.

`Authorization` использует Engine, EngineSettings и Unreal Online Subsystem. Провайдеры Steam и EOS включаются на уровне проекта. Публичный API доступен через `Online/NGConnectionSubsystem.h`.

## Вход и группа

- **Steam.** Подсистема получает билет через Steam Online Subsystem с идентификатором сервиса `epiconlineservices` и передаёт его в EOS.
- **Epic Games.** При запуске из Epic Games Launcher используется exchange code. При обычном запуске открывается вход через Account Portal.
- **Группа.** EOS Lobby вмещает до четырёх игроков. Другие участники находят группу по коду. Лидер запускает карту из `GameDefaultMap` как listen server, а участники подключаются из панели после появления статуса готовности. Панель показывает имена, инициалы и роль лидера.
- **Прямое подключение.** Доступно по IPv4 или DNS с портом. Последний адрес хранится в `GameUserSettings`. Если записи в секции нового модуля ещё нет, подсистема читает её из прежней секции.

## Конфигурация и проверка

В [DefaultEngine.ini](../../Config/DefaultEngine.ini) заданы провайдеры и сетевой драйвер. [Steam.example.ini](../../Config/Steam.example.ini) содержит поля для Steam App ID и EOS Artifact. Значения учётных данных вносятся в локальную конфигурацию или конфигурацию сборки. Для общего лобби обе сборки используют один продукт, Sandbox и Deployment.

Для сквозной проверки нужны две сборки с настроенным EOS. Один игрок входит через Steam и создаёт группу. Второй входит через Epic, присоединяется по коду и подключается к миссии после запуска лидером.
