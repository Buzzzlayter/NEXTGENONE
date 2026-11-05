# Переход на Unreal Engine 5.8.4

Дата: 2026-11-05. Ветка: `upgrade/ue-5.8.4`, создана от `main` при чистом рабочем дереве.

## Проверенное окружение

- Движок: `S:/UE_5.8`, Build.version 5.8.4, changelist **58902052**, compatible changelist 55116800.
- Таргет: `NEXTGENONEEditor Win64 Development`, редактор закрыт.
- Реально выбранный UBT компилятор: Visual Studio MSVC **14.44.35228**, каталог tools 14.44.35207.
- Windows SDK: **10.0.22621.0**.
- EngineAssociation остаётся `5.8`: установленный patch определяется Build.version. Target rules уже используют BuildSettingsVersion.V7 и Unreal5_8 include order, менять их не потребовалось.
- EngineVersion=5.8.0 в дескрипторах ALS/GCFSM не выдал несовместимости в выполненной сборке; искусственно поднимать минимальную версию плагинов не потребовалось.

## Что исправлено

В GCFSM Runtime и Editor был отключён ShadowVariableWarningLevel. После удаления подавления полная пересборка обнаружила **40 C4456/C4458**, которые при настройках проекта считаются ошибками. Это скрытая диагностика старого кода, а не доказанная регрессия движка 5.8.4.

Исправления ограничены именами параметров/локальных переменных и удалением одной неиспользуемой переменной:

- Runtime: GCFSMBasicState.cpp, GCFSMLocalState.cpp, GCFSMState.cpp, GCFSMSubmachineState.cpp — параметры реализаций Initialize больше не перекрывают поля класса.
- Runtime: GCFSMRootState.cpp — убрана ненужная локальная переменная contextObject; GCFSMSnapshot.cpp — неиспользуемая внешняя guid; GCFSMUtilities.cpp — различены локальные state/rootState.
- Editor: GCFSMStateNode.cpp и GCFSMTriggerEventNode.cpp — локальные переменные и lambda-параметры больше не перекрывают node/startNode.
- GCFSM.Build.cs и GCFSMEditor.Build.cs — удалено подавление shadow warnings.

Заголовки с отражёнными параметрами, Blueprint-пины, имена классов и сериализуемых свойств не менялись. Ассеты, defaults игрового проекта, GAS и сетевые правила не менялись. ALS и GameplayMessageRouter исправлений не потребовали.

## Фактические результаты сборки

1. Принудительная пересборка Editor через UBT `-Rebuild -NoHotReloadFromIDE -MaxParallelActions=4`: **FAIL**, exit 6, 40 скрытых shadowing-ошибок GCFSM. Компиляционные действия покрыли NEXTGENONE, GameplayFoundation, UIModule, ALSV4_CPP, GCFSM Runtime/Editor и GameplayMessageRouter Runtime/Nodes. Других compiler warnings/deprecated-диагностик в логе нет.
2. После исправлений запуск `.agents/skills/ue-build/Verify-Build.ps1`: **PASS**, exit 0. Выполнены 18 действий, включая перекомпиляцию изменённых GCFSM .cpp и финальную линковку. В логе **0 compiler errors, 0 compiler warnings, 0 deprecated-диагностик**. Повторная полная пересборка после успешной проверки не запускалась.

Локальные полные логи (Saved не коммитится):

- `Saved/Logs/AgentBuild-5.8.4-baseline.log` — первая полная пересборка, 128.35 секунды.
- `Saved/Logs/AgentBuild.log` — успешная сборка после исправлений, 35.90 секунды по UBT.

Новые подавления предупреждений не добавлялись. Deprecated API, требующих замены, выполненный compile не выявил; поиск по именам DeprecatedProperty в проектных compatibility-полях не является основанием удалять эти поля.

## Границы проверки

PIE, smoke, multiplayer, запуск редактора, пересохранение Blueprint, cook/package, Game/Shipping/Server targets и Linux не запускались. Предкомпилированные плагины установленного движка не пересобирались из исходников: результат относится к проекту и компилируемым проектным плагинам. Успешная C++-сборка не подтверждает работоспособность графов/ассетов или runtime-сетевых сценариев.
