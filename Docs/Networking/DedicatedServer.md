# Dedicated server и подключение

Основной режим проекта — отдельный dedicated server. Steam пока является заглушкой: App ID не задан, OnlineSubsystemSteam не включён, регистрации сессий/аутентификации Steam нет. Подключение сейчас идёт напрямую по IPv4/DNS и UDP-порту.

## Что подключено

Esc → настройки → «Сеть»: адрес сервера, «Подключиться», «Отключиться», статус/ошибки и явно отключённая кнопка Steam. WBP_ServerConnection вложен в существующий WBP_GameSettings и живёт вместе с FSMState_GameSettings. UNGConnectionSubsystem на GameInstance переживает переход между картами, хранит последний адрес, слушает ошибки сети/travel и ограничивает ожидание подключения 30 секундами. В адресе запрещены URL-параметры/команды; IPv6 пока не поддерживается.

UI подключения следует разделению запроса/статуса из Lyra CommonSessionSubsystem, но адаптирован к существующей UMG/HFSM. Lyra CommonUI/CommonUser, Experience System, EOS и online framework не переносились. Это меню direct-connect, а не Steam browser/matchmaking. Disconnect возвращает в локальный Gameplay.

## Разработка на текущем движке

В S:/UE_5.8 найден Engine/Build/InstalledBuild.txt. Отдельный NEXTGENONEServer.exe в этом проходе не собирался. Для локальной разработки предусмотрен headless dedicated-режим UnrealEditor-Cmd:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/StartDedicatedServer.ps1 -EditorServer -Port 7777
```

Скрипт резолвит движок по EngineAssociation/реестру; при необходимости передать -EngineRoot. Процесс запускается скрыто, выводятся PID и путь Saved/Logs/DedicatedServer.log. Скрипт не запускался агентом. Его запуск является игровым/сетевым прогоном и требует отдельного апрува по правилам проекта.

После согласованного запуска серверной части клиент подключается через настройки к 127.0.0.1:7777; другой компьютер в LAN — к LAN-адресу сервера. Это примеры будущего запуска, а не результаты проверенного соединения.

## Отдельная серверная сборка

Добавлен Source/NEXTGENONEServer.Target.cs. Для полноценного server target нужен source build UE 5.8 с server support; для Linux также нужен соответствующий Linux toolchain и совместимые сборки зависимостей. Проектная привязка движка автоматически не менялась.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/PackageDedicatedServer.ps1 -EngineRoot X:/UE_5.8_Source -Platform Win64
# Или -Platform Linux при настроенном toolchain.
```

Скрипт вызывает UAT BuildCookRun -server -noclient, собирает/cook/stage/archive только Gameplay и помещает результат в Saved/ServerPackages/<Platform>. Он откажется работать с installed engine до начала сборки. В этом проходе packaging/cook не выполнялись.

Windows-запуск staged пакета:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/StartDedicatedServer.ps1 -ServerExecutable X:/ServerPackage/NEXTGENONEServer.exe -Port 7777
```

Linux-запуск готового staged executable/launcher:

```bash
bash Scripts/StartDedicatedServer.sh /opt/nextgenone/NEXTGENONEServer.sh 7777
```

На хосте нужен доступный UDP 7777. Для доступа из интернета — публичный адрес и входящий UDP-порт; при домашнем NAT потребуется проброс порта. Серверный пакет переносится на отдельную Windows/Linux машину и запускается как постоянный процесс/служба. Провайдер, VM, служба рестарта, мониторинг и деплой пока не выбирались и не настраивались.

## Steam позже

Config/Steam.example.ini — неактивная справочная заготовка, а не действующая конфигурация. После получения App ID понадобится включить подходящий Steam OSS/transport, настроить dedicated-server advertisement и authentication, затем согласованно проверить клиентские Steam-аккаунты, совместимость билдов, серверный browser/приглашения. Текущая кнопка Steam ничего из этого не выполняет. App ID 480 не подставлялся по просьбе пользователя.

Источники: [Epic: Dedicated Servers](https://dev.epicgames.com/documentation/unreal-engine/setting-up-dedicated-servers-in-unreal-engine?lang=en-US), [Epic: Steam OSS](https://dev.epicgames.com/documentation/en-us/unreal-engine/online-subsystem-steam-interface-in-unreal-engine), [Epic: Common User / Common Session](https://dev.epicgames.com/documentation/en-us/unreal-engine/common-user-plugin-in-unreal-engine-for-lyra-sample-game). Локально прочитан Lyra Plugins/CommonUser/Source/CommonUser/Private/CommonSessionSubsystem.cpp.

## Фактически выполнено

NEXTGENONEEditor Win64 Development: PASS, UBT exit 0, без диагностик компилятора (Saved/Logs/NGBallisticsBuild.log). Авторинг NGSetup -BallisticsOnly: exit 0, 0 ошибок / 14 прежних предупреждений ALS/окружения (Saved/Logs/NGBallisticsAssets.log). Windows/Linux server target, упаковка, серверный процесс, прямое подключение и Steam не проверялись. PIE/игровые/сетевые тесты не запускались.