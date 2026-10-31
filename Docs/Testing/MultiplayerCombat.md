# Combat Foundation: фактическая проверка

**Статус после последующих изменений:** перечисленные ниже сетевые PASS относятся к Combat Foundation до объединения классов в `Nextgenone*` и удаления voxel. После закрытия редактора пользователем рефакторинг собран: UBT PASS, Blueprint/map migration exit 0, 0 errors и 24 warnings. Логи: `Saved/Logs/NGConsolidationBuild.log` и `NGConsolidation.log`. Новые PIE/сетевые тесты не запускались: для этого требуется отдельный апрув.

Дата: 2026-11-01 (Europe/Moscow). Проект NEXTGENONE, ветка `research/gameplay-foundation`.

## Сборка

Оба этапа прошли полную UBT-сборку и link `NEXTGENONEEditor Win64 Development` при закрытом редакторе. Использован `.agents/skills/ue-build/Verify-Build.ps1`. GAS build сохранён в `Saved/Logs/NGStage1Build.log`; последний полный результат — `Saved/Logs/AgentBuild.log`, RESULT PASS, без compiler diagnostics. Runtime сообщает UE 5.8.3, CL 58210709; первоначальное исследование фиксировало Build.version 5.8.0, поэтому для результатов этого прохода ориентироваться на фактический runtime.

Первый UBT-запуск был заблокирован Live Coding до закрытия редактора пользователем. Это не успешная компиляция. Последующие реальные compiler errors исправлены: имя параметра для DOREPLIFETIME, shadowing Pawn, TSubclassOf conditional, include AHUD и повторное объявление InitGame. Сторонние GCFSM deprecated ModuleRules/StructUtils dependency warnings из начального UBT makefile pass не исправлялись в чужих модулях.

NGSetup commandlet повторно завершился с exit 0: `Saved/Logs/NGSetupVerified.log`, 0 errors, 39 warnings. Созданы BP_NGCharacter, BP_NGController и CombatTest.umap. Первый setup выявил startup Zen DDC недоступность и отсутствие CleanupWorld при уничтожении созданного editor world; commandlet исправлен, повторная проверка успешна. Для тестовых запусков используется штатный `-ddc=InstalledNoZenLocalFallback`, настройки проекта не меняются.

## Сетевой сценарий

Запускается **один dedicated server и два отдельных клиентских процесса** через UnrealEditor.exe `-server` / `-game`, uncooked assets, `-nullrhi -nosound -unattended`. Это реальная engine networking/replication, но не packaged-build проверка и не визуальная оценка рендера.

Воспроизводимый runner: `Tools/Testing/Run-NGCombatSmoke.ps1`. Он создаёт уникальную папку Server/Client1/Client2 логов на каждый запуск и в finally завершает только процессы, запущенные им самим.

Сценарий:

1. Сервер ждёт обоих игроков; каждый owning client проверяет ASC owner=свой PlayerState, avatar=свой Pawn, HP=100.
2. Клиент A экипирует тестовое оружие. На сервере повторный bind не создаёт вторые grants.
3. A посылает серию GAS activation requests вперемешку с equip/unequip. Сервер допускает один выстрел: ammo 12→11, B HP 100→75. После перестановок остаётся один fire spec.
4. Оба клиента проверяют replicated HP=75 и equipped shooter; владелец A дополнительно проверяет ammo=11.
5. A делает ещё три выстрела. Сервер применяет damage GE, HP B=0 и один death transition. Оба клиента подтверждают смерть и equipped shooter; A подтверждает ammo=8.
6. Сервер сохраняет старый corpse, respawn-ит B с тем же PlayerState, вызывает поздние init/teardown старого Pawn и пытается применить direct damage и GE с context старого тела к новому ASC. Новая жизнь остаётся HP=100 с корректным avatar.
7. Финальный усиленный сценарий дополнительно делает hitscan по трупу и немедленно повторяет fire с действующими grants, затем после unequip/equip. Повторные попытки обязаны не расходовать ammo и не обходить cooldown. Маркер `NG_LIMITER_PASS`.
8. Оба клиента после respawn вновь подтверждают ASC owner/avatar и HP=100. Runner также проверяет наличие cosmetic shot event в обоих клиентских логах.

Ранние неуспешные запуски сохранены в Saved/Logs. Исправленные проблемы: controller RPC мог приходить до client Pawn replication; Visibility collision скопированного ALS Blueprint не блокировала trace; старый лог мог ошибочно читаться при повторном запуске runner. Неуспешные попытки не считаются PASS.

## Результаты

| Конфигурация | Факт |
|---|---|
| 0 ms, loss 0 | PASS базового сценария, `Saved/Logs/NGCombat_0ms_0loss_20261101_004243/Server.log`; HP/equipment/death подтверждены обоими клиентами, server respawn/corpse guard пройдены. Это до усиления client-respawn/cosmetic/valid-grant limiter assertions |
| PktLag=100, loss 0 | PASS, `Saved/Logs/NGCombat_100ms_0loss_20261101_004536/Server.log`; есть reports 0/4/5/6 от обоих клиентов, cosmetic events в обоих логах, delayed-GE guard. В startup присутствовали engine MTAccessDetector handled ensures; запуск нельзя считать полностью чистым |
| Финальная DLL, PktLag=200, loss 0 | PASS полного усиленного сценария, `Saved/Logs/NGCombat_200ms_0loss_20261101_004939/`; 0 errors/ensures во всех трёх логах, NG_LIMITER_PASS, client-respawn reports, по 5 cosmetic events на клиент |
| Финальная DLL, PktLag=100, loss 3% | PASS полного усиленного сценария, `Saved/Logs/NGCombat_100ms_3loss_20261101_005138/`; 0 errors/ensures во всех трёх логах, по 5 cosmetic events на клиент |
| Финальная DLL, PktLag=0, loss 0 | PASS полного усиленного сценария, `Saved/Logs/NGCombat_0ms_0loss_20261101_005306/`; 0 errors/ensures во всех трёх логах, NG_LIMITER_PASS, client-respawn reports, по 5 cosmetic events на клиент |

`PktLag` задаётся серверу и каждому клиенту как задержка исходящих packets. PktLag=100 приблизительно добавляет 200 ms round-trip; PktLag=200 — 400 ms round-trip, без учёта tick/локальной нагрузки. Это **не** утверждение, что измеренный RTT равен 100/200 ms. PktLoss задаётся отдельно в процентах. Настройки подтверждаются строками NetDriver `PktLag set to ...` в логах.

## Вручную в редакторе

После отдельного апрува на тестирование открыть `/Game/Maps/Gameplay`. Её World Settings использует BP_NextgenoneGameMode, единый основной режим. Выбрать Play As Client, 2 players, отдельный dedicated server; можно также запустить отдельные процессы. E — equip, U — unequip, ЛКМ — один выстрел. При обычном PIE флаг `-NGCombatSmoke` не требуется и автоматические действия не выполняются. Автоматический runner использует ту же карту с URL `?game=/Script/NEXTGENONE.NGCombatTestGameMode`; отдельной тестовой копии карты нет.

Для UI использовать OnHealthChanged/OnDeathChanged на Character и OnEquipmentChanged/OnAmmoChanged/OnShotCosmetic на Combat. При создании widget прочитать текущий snapshot, подписаться, при смене Pawn/Exit снять подписки. Ammo приватно для владельца. Existing HFSM mounting/context не менять. Виджет HP/ammo пока требует этой проектной привязки; сетевые assertions проверяют данные, а не пиксели HUD.

## Непроверенные сценарии и ограничения

- Ручной визуальный smoke ALS movement/mantle/camera/overlays, gamepad, focus/UI-only input и внешний weapon pack. Headless тест блокирует движение, чтобы обеспечить воспроизводимый aim, и не доказывает locomotion regression.
- Packaged client/server, Shipping, Linux server, reconnect/seamless travel, late join, длительный soak и hostile/fuzzed RPC traffic.
- Пустой магазин, стены/near-muzzle obstruction и полный перебор collision profiles пока не автоматизированы; production code проверяет ammo и делает два traces, но это не равно пройденному тесту всех сцен.
- Нет lag compensation, client aim samples, prediction/reconciliation для recoil, reload, automatic fire, inventory и grenades. Урон фиксированный, capsule-based, без headshots/armor/teams.
- Test weapon — engine cube. Confirmed trace/impact debug visuals и delegates есть; ragdoll и полноценные weapon animations не подключены.
- Существующие ALS/engine asset warnings о недоступном `/Engine/EngineMeshes/Humanoid` и устаревших animation modifiers не исправлялись в оригинальном плагине.
- Engine startup ensures раннего 100 ms запуска зафиксированы отдельно. После переноса loading в InitGame финальные 200 ms и 100 ms + loss запуски имеют 0 errors/ensures. Это наблюдение по логам, не символизированное доказательство причины первоначального engine race.

Defaults основного проекта и пользовательские изменения сохранены. Публичная публикация лицензий/assets, commit/push и дальнейшие системы в этот проход не входят.

## Изменённые файлы этого прохода

- `NEXTGENONE.uproject`: добавлено только включение GameplayAbilities поверх пользовательской версии.
- `Source/NEXTGENONE/NEXTGENONE.Build.cs`: GAS/ALS dependencies и условные editor dependencies.
- Новые `Source/NEXTGENONE/Gameplay/NGAbilitySystem.h`, `NGAbilitySystem.cpp`.
- Новые `Source/NEXTGENONE/Gameplay/NGCharacter.h`, `NGCharacter.cpp`.
- Новые `Source/NEXTGENONE/Gameplay/NGCombat.h`, `NGCombat.cpp`.
- Новые `Source/NEXTGENONE/Gameplay/NGPlayerController.h`, `NGPlayerController.cpp`.
- Новые `Source/NEXTGENONE/Gameplay/NGTestGameMode.h`, `NGTestGameMode.cpp`.
- Новые `Source/NEXTGENONE/Gameplay/NGSetupCommandlet.h`, `NGSetupCommandlet.cpp`.
- Новые `Content/GameplayFoundation/BP_NGCharacter.uasset`, `BP_NGController.uasset`, `Maps/CombatTest.umap`.
- Новый `Tools/Testing/Run-NGCombatSmoke.ps1`.
- Обновлён `Docs/Architecture/GameplayFoundation.md`, создан `Docs/Testing/MultiplayerCombat.md`.

`LyraAnalysis.md` сохранён как утверждённое исследование. Прочие изменения Git status существовали до начала реализации; они не являются частью этого прохода. Логи и build artifacts находятся в Saved/Binaries/Intermediate и не предназначены для source commit.
