# NEXTGENONE — проектная памятка для Claude

UE **5.8** C++ проект.

## Структура
- Единственный модуль `NEXTGENONE` (Runtime) — `Source/NEXTGENONE/`.
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
- `Clean.bat` — удалить `Binaries`/`Intermediate`/`DerivedDataCache` (+ в плагинах).
  Откажется работать при открытом редакторе, спрашивает подтверждение. `Saved/` не трогает.

## Интеграция Claude ↔ UE (главное)
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
- `.claude/settings.local.json` — личные права (allow/deny/ask), в `.gitignore`, НЕ бэкапится.
- В коммитах отключена приписка Клода (`attribution.commit: ""`) — автор только пользователь.
