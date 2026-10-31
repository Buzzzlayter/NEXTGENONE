# NEXTGENONE — проектная памятка для Codex

UE **5.8** C++ проект.

## Структура
- Модули проекта: основной `NEXTGENONE` (Runtime) — `Source/NEXTGENONE/`; UI — `Source/UIModule/`. Шаблонный модуль `ALSv4_Community` и `Content/ThirdPerson` полностью удалены. Реальная ALS-интеграция находится в плагине `Plugins/ALSV4_CPP` и сохранена.
- Единая игровая цепочка: `ANextgenoneGameMode`, `ANextgenoneCharacter`, `ANextgenonePlayerController`, `ANextgenonePlayerState` в `Source/NEXTGENONE/Gameplay/`. `ANGCombatTestGameMode` наследует основной режим и добавляет только тестовый harness.
- Рабочая карта проекта: `/Game/Maps/Gameplay`, Blueprint режима: `/Game/Blueprints/BP_NextgenoneGameMode`. Отдельная combat-карта удалена переносом в Gameplay; тестовый runner выбирает свой режим через URL override.
- Voxel удалён: нет `NgxVoxel`, старого voxel Pawn, voxel-ассетов и `DevLevel`. Не восстанавливать эту подсистему без поручения пользователя.
- Таргеты: `Source/NEXTGENONE.Target.cs`, `Source/NEXTGENONEEditor.Target.cs`.
- Включённые плагины (`NEXTGENONE.uproject`): `ModelContextProtocol`, `Terminal`,
  `EditorToolset`, `ModelingToolsEditorMode`.

## Движок и сборка
- Движок UE **5.8**, установлен в `S:\UE_5.8` (резолвится из реестра по EngineAssociation).
- **Тулчейн — **14.50.35717**.

### Батники в корне (self-contained, движок берут из реестра)
- `Build.bat` — собрать `NEXTGENONEEditor Win64 Development`.
- `RunEditor.bat` — запустить редактор с проектом.
- `GenerateProjectFiles.bat` — пересоздать .sln/проектные файлы.
- `DeepClean.bat` — удалить `Binaries`/`Intermediate`/`DerivedDataCache`/`Saved` (+ `Binaries`/`Intermediate`
  в плагинах) и `*.sln`/`*.slnx` в корне. `Content`/`Config`/`Source` не трогает.
  Откажется работать при открытом редакторе, спрашивает подтверждение. После — `GenerateProjectFiles.bat`.

## Верификация для агента (компиляция C++ + логи)
Цель: агент видит **реальный** вывод компилятора и логи редактора текстом, а не угадывает API.
Оформлено двумя скиллами; скрипты лежат рядом с ними в `.Codex/skills/ue-build/`.

### Скилл `ue-build` — компиляция C++ → текст
- `Verify-Build.ps1` — полная офлайн-сборка UBT (редактор **закрыт**). Печатает `RESULT` +
  ошибки `файл(строка): error Cxxxx` + warnings; полный вывод в `Saved/Logs/AgentBuild.log`.
  Запуск: `powershell -NoProfile -ExecutionPolicy Bypass -File .Codex/skills/ue-build/Verify-Build.ps1`
  (`-Game` — game-таргет; `-Config Debug|DebugGame|Shipping`).
  **Exit:** `0`=PASS (код компилируется; сюда же link-lock при открытом редакторе),
  `3`=BLOCKED (Live Coding активен, **ничего не собралось** — не ошибка кода), иное=реальная ошибка.
- `Trigger-LiveCoding.ps1` — при **открытом** редакторе шлёт Ctrl+Alt+F11 (best-effort, SendKeys,
  крадёт фокус). Результат компиляции читается из лога (см. ниже). UBT при активном Live Coding
  собирать отказывается (`Unable to build while Live Coding is active`).
- В MCP **нет** инструмента «выполнить консольную команду» → headless-запуск самой компиляции
  невозможен; компиляцию инициирует UBT (редактор закрыт) либо Ctrl+Alt+F11 (редактор открыт).

### Скилл `ue-logs` — логи редактора → текст (редактор открыт, через MCP)
- Тулсет `EditorToolset.LogsToolset`, вызов `GetLogEntries`.
  **ВАЖНО:** всегда передавай `category:""` для поиска по всем категориям — дефолт `"LogsToolset"`
  это несуществующая категория и даёт ошибку.
- Ошибки/warnings/ассерты: `{category:"", pattern:"(?i)error|warning|fatal|assert", maxEntries:50}`.
- Результат Live Coding: `{category:"LogLiveCoding", pattern:"", maxEntries:30}`.
- Список категорий: `GetLogCategories {filter:"..."}`; детальность: `SetVerbosity {category, verbosity}`.
- Редактор закрыт (MCP недоступен) → лог прошлой сессии `Saved/Logs/NEXTGENONE.log` (Read/Grep).

## Интеграция Codex ↔ UE (главное)
Связь идёт через плагин `ModelContextProtocol` — Epic-native **MCP по HTTP на
`127.0.0.1:8000/mcp`**, авто-старт включён, сервер зарегистрирован как `unreal-editor`.
- Управление редактором — через навык `ue` (discovery → `call_tool`); `mcp__unreal-editor`
  уже в allow-листе, так что вызовы идут без подтверждений.
- **MCP отвечает только когда редактор запущен** (порт 8000 слушает). Если закрыт —
  сначала `RunEditor.bat`.
- Можно просить вживую: ставить/двигать актёры, править блюпринты/материалы/Niagara,
  PCG-графы, запускать PIE, читать лог редактора (`Saved/Logs/NEXTGENONE.log`).

## Git и бэкап (важно)
- Remote: `github.com/Buzzzlayter/NEXTGENONE`, ветка `main` (солопроект, коммиты прямо в main).
- Бинарники — через **Git LFS** (`.gitattributes`): `.uasset`, `.umap`, `.fbx`, текстуры и т.д.
- `.Codex/settings.local.json` — личные права (allow/deny/ask), в `.gitignore`, НЕ бэкапится.
- В коммитах отключена приписка Клода (`attribution.commit: ""`) — автор только пользователь.

## Апрув перед тестированием (правило пользователя, 2026-11-01)
- Не начинать этап тестирования без отдельного явного апрува пользователя на этот этап.
- Это относится к автотестам, PIE, сетевым проверкам, smoke/regression и прогонам с latency/packet loss.
- После реализации сообщить о готовности к тестированию и ждать апрува. Общее поручение реализовать функциональность не заменяет отдельный апрув на тестовый этап.
- Не расширять и не повторять тестовые прогоны инициативно за пределами согласованного объёма.
