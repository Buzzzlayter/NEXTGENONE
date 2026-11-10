# Миграции Core Redirects

## 11 ноября 2026: удалены все 25 проектных redirects

Текущий `Config/DefaultEngine.ini` больше не содержит Core Redirects и
ActiveGameNameRedirects. Удалены только записи из предварительно просмотренного плана:

- 6 ClassRedirects и 6 StructRedirects переноса инвентаря в GameplayFoundation;
- 4 PropertyRedirects appearance-полей NGWeaponDefinition → NGEquipmentDefinition;
- PackageRedirect и ClassRedirect BP_NextgenonePawn → BP_NextgenoneCharacter;
- 3 ClassRedirects переименований Character/PlayerController/PlayerState;
- 2 ClassRedirects HFSMContextLibrary/HFSMEventLibrary → HFSMLibrary;
- 2 ActiveGameNameRedirects TP_Blank → NEXTGENONE.

С редиректами принудительно пересохранены 62 проектных ассета, включая карту Gameplay
и DA_Pistol/Rifle/Shotgun/TestWeapon. Затем отдельный процесс UnrealEditor-Cmd успешно
загрузил эти же 62 ассета и destination-типы без редиректов. Снимки классов ассетов,
родителей Blueprint, Ability Set grants/effects, классов/ссылок item fragments, тегов,
стеков предметов и appearance-полей оружия совпали. Оба успешных процесса завершились
с кодом 0; Error/Fatal в их логах нет. Предупреждения ALS/Python, старого animation
modifier, empty engine version и окружения остались; плагиновые ассеты не сохранялись.

Полный backup до первого запуска и до любых изменений ассетов:
`Saved/CoreRedirectMigrations/20261111-003858/`.
Manifest, backup перед успешным проходом, отчёты `resave.json`/`verify.json` и логи:
`Saved/CoreRedirectMigrations/20261111-004246/`.

Скрипт `Scripts/Editor/MigrateCoreRedirects.py` использовал явный `--plan-file`:
`Saved/RuntimeReview/RedirectCleanupPlan.json`. Для процесса миграции DDC направлен
в проектный `Saved/DerivedDataCache`, автозапуск Zen и source control provider отключены.
Первоначальные остановки из-за DDC, Python API и source control произошли до удаления
redirects; их отчёты сохранены в соседних timestamp-каталогах.

Сборка, PIE, gameplay/сетевые тесты и cook не выполнялись. Это проверка сериализации
и свежей загрузки текущего Content; совместимость внешних savegames и старых веток
не проверялась. Обычный редактор после завершения не запускался.

## Историческая миграция 2 ноября 2026

2026-11-02. Выполнена миграция ссылок из /Script/NEXTGENONE в /Script/GameplayFoundation для 7 классов и 4 структур. Из DefaultEngine.ini удалены только соответствующие 11 ClassRedirects/StructRedirects. Старые redirects переименований Character/Controller/PlayerState, Pawn Blueprint и UI libraries сохранены.

Пересохранены 39 проектных Blueprint/Data Assets из /Game/Blueprints и /Game/Gameplay, включая DA_PlayerAbilities. Плагиновые ассеты не изменялись. Затем отдельный свежий процесс UnrealEditor-Cmd загрузил те же 39 ассетов и новые native-типы без удалённых redirects. Сравнение классов ассетов, родителей Blueprint и Ability Set grants/effects прошло. В логах этих двух этапов нет Error/Fatal; необязательные profiling DLL при старте по-прежнему отсутствуют.

Резервная копия оригинального ini, 39 ассетов, manifest, отчёты и полные логи успешного прохода:

`Saved/CoreRedirectMigrations/20261102-225837/`

Исходники скрипта: `Scripts/Editor/MigrateCoreRedirects.py`. По умолчанию показывает план; --apply выполняет резервное копирование, пересохранение, точечное удаление и fresh-load. При ошибке финальной загрузки возвращает оригинальный ini. Для принудительно прерванного процесса после удаления ini восстановить его вручную из backup перед дальнейшей работой. Не использовать старую полную копию ini для восстановления после последующих пользовательских изменений без сравнения diff.

Создан глобальный навык Codex `ue-core-redirect-migration` с копией скрипта. В текущем Python нет PyYAML: штатный quick_validate не выполнился; структура двух простых полей frontmatter и синтаксис Python проверены отдельно. Сам рабочий скрипт фактически выполнил resave/fresh-load на проекте. Две первоначальные ошибки Python API остановили подготовительные попытки до сохранения/удаления ini; исправления включены в навык.

Это ограниченная проверка загрузки при авторинге, явно запрошенная пользователем. PIE, gameplay, сетевые тесты, cook и packaged build не запускались. Не проверены внешние savegames, DLC/ассеты вне текущего Content, старые ветки и полная логическая эквивалентность Blueprint-графов/всех default values. Для такой совместимости удалённые redirects могут всё ещё понадобиться. Обычный редактор запущен после успешной миграции.
