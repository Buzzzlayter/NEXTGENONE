# Combat Foundation: фактическая проверка

## Диагностика первого переноса после холодного старта Editor

Симптом пользователя: drag начинается, но перенос из коробки не применяется; повторная интеракция восстанавливает работу, следующие PIE в том же Editor проблему не воспроизводят. Удаление кеша PlayerInventory в панели не устранило симптом; первопричина пока не установлена.

Временные диагностические логи переноса удалены по запросу пользователя. Проверки доступа, Avatar, snapshot и ограничения частоты сохранены. Холодный запуск и последующий успешный перенос агентом не проверялись; исправление первопричины не подтверждено.

## Текущий harness — 2026-11-07, не запускался

Код runner обновлён под Action inventory, выбранный item и GAS ADS. Исторические PASS ниже относятся к старым версиям и не подтверждают этот пакет. Сборка и запуск требуют отдельного разрешения.

Клиентские команды и отчётные RPC вынесены из PlayerController в `UNGCombatSmokeComponent` (`Gameplay/Debug`). Тестовый GameMode создаёт реплицируемый компонент на контроллерах участников только внутри включённого smoke-runner; основной Controller не содержит тестовых RPC, проверок или зависимостей на инвентарь/Combat. Перенос проверен только статически, сборка и сетевой запуск не выполнялись.

Подготовка после сборки: создать отдельный Blueprint-наследник `NGCombatTestGameMode`; назначить ему тот же Character/Controller и `PlayerStateClass=BP_NextgenonePlayerState`, что используются игрой. Проверить Layout и явные **Inventory.InitialContents**: оружие с Count=1/SlotTag=Inventory.Slot.Action, отдельные записи патронов с SlotTag=Inventory.Slot.Storage и Count в пределах MaxStackSize. Старые StarterItems больше не выдаются автоматически. Основной GameMode и defaults проекта не переключать.

Fixture: открытый ровный участок возле координат (0,0,100) и (600,0,100), обычная гравитация, Alive/Dead FSM, отсутствие внешнего урона. В Action должно быть оружие с одним projectile и нулевым spread; magazine содержит достаточно патронов, чтобы убить цель без Reload. HP, damage, magazine и ожидание полёта берутся из текущих данных. Неподходящий fixture заканчивается FAIL, оружие автоматически не создаётся. Таймаут сценария — 120 секунд.

После отдельного разрешения runner запускается с явным классом настроенного тестового режима, например:

```powershell
Tools/Testing/Run-NGCombatSmoke.ps1 -GameMode /Game/Blueprints/BP_CombatTestGameMode.BP_CombatTestGameMode_C
```

Этот пример не означает, что Blueprint уже создан. Runner не собирает проект. Использует три Editor-процесса с nullrhi, завершает только собственные процессы. Проверяет выбор Action item, отсутствие расхода при обычном RequestFire без ADS, GAS ADS, сходимость camera ray, настоящий клиентский RequestFire, расход магазина, серверный урон, состояние на обоих клиентах и защиту новой жизни от старого Pawn/context. Client snapshots ожидают репликацию в пределах общего deadline.

Persistent fire limiter проверяется отдельным серверным TryCommitFire/CanFireNow probe до и после re-equip. Это не hostile RPC burst test и не доказательство всей защиты при latency/loss. Прямой FireOnServer без свежего remote view больше не используется. Reload/cancel, inventory race, packet loss, произвольная гравитация и визуал ALS требуют отдельных прогонов.

**Статус после последующих изменений:** перечисленные ниже сетевые PASS относятся к Combat Foundation до объединения классов в `Nextgenone*`. После закрытия редактора пользователем рефакторинг собран: UBT PASS, Blueprint/map migration exit 0, 0 errors и 24 warnings. Логи: `Saved/Logs/NGConsolidationBuild.log` и `NGConsolidation.log`. Новые PIE/сетевые тесты не запускались: для этого требуется отдельный апрув.

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
- Новые `Source/NEXTGENONE/Gameplay/Combat/NGCombat.h`, `NGCombat.cpp`.
- Новые `Source/NEXTGENONE/Gameplay/NGPlayerController.h`, `NGPlayerController.cpp`.
- Новые `Source/NEXTGENONE/Gameplay/GameModes/NGTestGameMode.h`, `NGTestGameMode.cpp`.
- Новые `Source/NEXTGENONE/Gameplay/Editor/NGSetupCommandlet.h`, `NGSetupCommandlet.cpp`.
- Новые `Content/GameplayFoundation/BP_NGCharacter.uasset`, `BP_NGController.uasset`, `Maps/CombatTest.umap`.
- Новый `Tools/Testing/Run-NGCombatSmoke.ps1`.
- Обновлён `Docs/Architecture/GameplayFoundation.md`, создан `Docs/Testing/MultiplayerCombat.md`.

Прочие изменения Git status существовали до начала реализации; они не являются частью этого прохода. Логи и build artifacts находятся в Saved/Binaries/Intermediate и не предназначены для source commit.

## NGJumpAbility — реализация 2026-11-01, runtime не проверен

Добавлены LocalPredicted прыжок, input tags и освобождение input/cancel при teardown avatar. Старые combat assertions обновлены: проверяют одну Fire ability и одну Input.Jump ability, а не единственную ability во всём ASC. Изменение harness не означает выполнение теста.

Выполнена только компиляция NEXTGENONEEditor Win64 Development через UBT; полный результат находится в Saved/Logs/AgentBuild.log, копия финального build — Saved/Logs/NGJumpBuild.log. Автотесты, PIE и сетевые прогоны этого изменения не запускались. Предыдущие combat PASS не являются результатом проверки новой ability.

После отдельного апрува согласовать короткий объём проверки:

- Два клиента + сервер: Space, короткое/долгое удержание; нет двойного запуска и повторной выдачи после повторного possession.
- Успешный mantle не вызывает дополнительный Jump; stand-up и ragdoll get-up сохранились.
- Отмена, смерть, respawn: старая ability не изменяет новый Pawn; удерживаемый input очищается.
- UI-only, потеря фокуса и смена mapping context во время удержания: не остаётся активная ability/залипшее нажатие. Этот сценарий пока является известным непроверенным местом интеграции.
- Репликация и предсказание прыжка, в том числе порядок доставки movement и GAS activation. Произвольная гравитация не проверялась и не заявлена как поддержанная.

## Dash/Mantle/Pickup ability assets — 2026-11-01, до апрува тестирования

Реализация расширена реальными Blueprint GA_Jump/GA_Dash/GA_Mantle/GA_PickupWeapon/GA_Fire, Ability Set, Input Mapping и BP_WeaponPickup. Подробнее — GameplayFoundation.md, раздел 18. F заменяет старый E для debug equip; E теперь делает настоящий серверный pickup, V — mantle. Smoke harness вызывает NGEquip напрямую, поэтому смена клавиши его команд не меняет.

Фактически выполнено: UBT Editor Development PASS (NGAbilityFoundationBuild.log); создание, компиляция и сохранение ассетов NGSetup -AbilitiesOnly exit 0, 0 errors/14 warnings (NGAbilityAssets.log). При первоначальном создании исправлен native constructor cooldown GE; это зафиксировано в NGAbilityAssets_FirstFailure.log. Gameplay tests не запускались, старые PASS не распространяются на этот проход.

Ожидают отдельного апрува и согласованного объёма:

- Dash: столкновение со стеной, server/owner/proxy, отмена смертью и unpossess, повторная активация до cooldown, переэкипировка и respawn; расход/индикация cooldown.
- Mantle: сервер выбирает поверхность, старый ledge RPC не перемещает Pawn, нет второго auto/direct ALS запуска, владельцу и proxy приходит один старт. Death/cancel/watchdog не возвращают мёртвому Walking. Сохранены ALS montage/curve settings.
- Pickup: два игрока одновременно, wall/дистанция/неверный target, заполненный слот, availability/mesh для late join, выдача ровно одной weapon ability, урон и server fire limiter после подбора.
- UI/focus/mapping changes во время удержания; новая жизнь и поздние события старого Pawn.
- Mid-mantle late join и произвольная gravity: известные ограничения текущего среза, не обещанные PASS.

Подготовка вручную: разместить /Game/Gameplay/Weapons/BP_WeaponPickup в Gameplay; для Q/V/E менять /Game/Gameplay/Input/IMC_Abilities. Карта автоматически не пересохранялась. До апрува не нажимать Play от имени агента и не запускать Run-NGCombatSmoke.ps1.

Наблюдённая ошибка после открытия редактора: логи NGAbilityEditor.log и NEXTGENONE.log содержат PIE crash в ALSMantleComponent::BeginPlay из-за потерянного MantleTimeline после замены SCS class. Агент не инициировал PIE. Добавлены восстановление ссылки перед ALS BeginPlay и исправление миграции nested subobject; ассет пересохранён, финальный UBT PASS, NGSetup -AbilitiesOnly exit 0 (NG_MANTLE_TIMELINE: MantleTimeline). Повторный PIE после исправления не выполнялся; подтверждение устранения этого краша — первый обязательный пункт будущего согласованного тестового этапа. Editor MCP после падения не был доступен; редактор оставлен закрытым.

## Уточнение Air Dash — 2026-11-01

Деш изменён по запросу пользователя: активация только при ALS InAir и CharacterMovement.IsFalling, с повторной проверкой Falling перед Commit. Root motion сохраняет горизонтальный импульс после окончания через MaintainLastRootMotionVelocity; ограничение до walking MaxSpeed снято. Gravity продолжает действовать. Кнопка Q, скорость, длительность и cooldown остаются прежними. Вертикальный рывок и управление направлением WASD не добавлены.

Компиляция в открытом редакторе: Live Coding succeeded / Reload-Reinstancing Complete, No object changes detected. Подтверждение сохранено в Saved/Logs/NGAirDashLiveCoding.log; полная offline-сборка этого изменения не выполнялась.

В LiveCodingConsole также есть диагностические сообщения Cannot find image section .voltbl; несмотря на них, итоговый статус — Patch creation successful и Live coding succeeded. Копия вывода сохранена как Saved/Logs/NGAirDashLiveCodingConsole.log. Это не подтверждение runtime-результата.

## Inventory / pistol / rifle — 2026-11-01, до тестового апрува

Реализация инвентаря и оружия добавлена 2026-11-01. UBT Editor Development и адресный авторинг ассетов выполнены; исторические логи находятся в Saved/Logs. Игровые тесты этого прохода не запускались.

После отдельного апрува согласовать и выполнить:

- Два клиента + сервер: inventory и item subobjects видит только владелец; другая сторона видит выбранное оружие/aim. Порядок прихода item reference/definition/stat stacks не оставляет пустую UI или нулевые патроны навсегда.
- 1/2: pistol/rifle mesh, Rifle/PistolTwoHanded ALS poses; ПКМ hold/release и сброс aim при unequip/death/switch. Указание места mesh и визуальная совместимость требуют фактического осмотра.
- Пистолет single shot, автомат hold/release; release/canceled/focus/UI/смена Pawn останавливают timer. Серверный limiter не обходится переключением предмета, re-equip или повторной активацией.
- Стрельба уменьшает magazine конкретного item; смена оружия/unequip возвращает тот же остаток. Reload переносит только доступный reserve и не превышает capacity; полный/пустой запас, смена/удаление item, смерть и respawn во время reload не меняют новый item.
- Pickup при занятом слоте добавляет предмет в bag; только пустые руки auto-equip. Полный bag/гонка подбора двух игроков не дублируют предмет. Удаление текущего item снимает equipment grants. Общий stack consume не частично расходует при нехватке.
- I/Esc: окно owning player через Runtime HFSM, кнопка equip проверяется сервером, ammo/count обновляются без polling. Закрытие/выход из Runtime/abort восстанавливают input/cursor, не плодят bindings; health panel остаётся на своём placeholder.
- Old Pawn/PlayerState lifecycle и corpses, late join, потеря focus, latency/loss — только в отдельно согласованном объёме. Iris, межвладельческий transfer, drop/split/grid/контейнеры/Extraction не заявлены реализованными.

Ручное перед будущей проверкой: открыть Gameplay; definitions — Gameplay/Weapons/DA_Pistol и DA_Rifle, item fragments — Gameplay/Inventory/ID_*, UI — Gameplay/UI/WBP_InventoryPanel и Inventory Window component в FSMState_GameRuntime. Для полноценных персонажных fire/reload montage выполнить совместимый retarget на ALS и назначить CharacterFireMontage/CharacterReloadMontage. Проигрывание механизмов оружия уже связано с импортированными weapon sequences; AnimBP не заменён. Editor MCP при закрытом редакторе недоступен; агент не запускал PIE для проверки результата.

Игровые/сетевые тесты не запускались. После отдельного апрува проверить запрет Q на земле (без расхода cooldown), Q на подъёме/падении после прыжка, сохранение скорости после burst, gravity, приземление и коллизию со стеной; отдельно server/owning client/proxy и прерывание ударом. Это перечень непроверенных сценариев, а не результат PASS.

## Universal interaction / pickup / drop — 2026-11-01

Implementation and offline asset authoring are complete. NGSetup -InteractionOnly saved BP_ItemPickup, BP_InteractiveObject, GA_Interact, ID_TestItem and WBP_InteractionPrompt; updated the existing Ability Set, Runtime HFSM prompt component and legacy weapon pickup Blueprint. Commandlet summary: 0 errors, 10 warnings (file journal, MCP notice, ALS enum names exposed to Python). Log: Saved/Logs/NGInteractionAssets.log. Final C++ build evidence: Saved/Logs/NGInteractionBuild.log.

**No PIE, multiplayer, automated gameplay or smoke tests were run. Runtime correctness is not a PASS claim. Separate user approval is required before starting the following checks.**

Pending initial scope: one server and two clients on Gameplay, manually placed universal pickup (ID_TestItem and ID_Pistol) and BP_InteractiveObject. Check E prompt and interaction, simultaneous pickup by both clients (one winner), inventory-full rejection without consuming world object, drop then other-player pickup with exact count/ammo preservation, equipped drop removing equipment, replicated switch state and late join. Verify owner-only bag visibility, wall/distance rejection, stale Pawn requests after respawn and unavailable/consumed target rejection. Partial stateful stack drops must fail without mutation. Do not add latency/loss or regression runs without agreement on scope.

Manual preparation: place actors on the existing Gameplay map; choose ItemClass on each BP_ItemPickup (inventory ID class, not weapon DA); set StackCount. Existing Runtime HFSM mounts WBP_InteractionPrompt. Custom interactables must replicate and block the Interaction trace channel. Camera parallax, pickup volume size, drop placement near walls/slopes and legacy BP_WeaponPickup component migration remain unverified at runtime. Arbitrary gravity support is limited to gravity-aware drop placement here; a full gravity integration is not claimed. Packaging/cook and physics throwing are not verified or included.
## Settings / debug state ownership — 2026-11-01

Implemented: WBP_GameSettings on FSMState_GameSettings, local Settings/Back transitions in the existing HUD Game FSM, and Runtime-owned ALS HUD/overlay menu through a project debug-component copy. Full C++ build and offline Blueprint compilation/migration were executed; logs are Saved/Logs/NGSettingsBuild.log and NGSettingsAssets.log. No PIE, gameplay, multiplayer, smoke or automated runtime tests were run for this change.

Pending, after separate approval: Esc -> settings -> Back repeatedly; child button/combo keyboard focus; exit via other state/abort; inventory closing when leaving Runtime; fire/aim stopping; move/look input and cursor restored; respawn produces one debug HUD and rebinds overlay; overlay closes on state exit/Pawn replacement. Check Apply/Back/defaults and persistence of display, quality, FPS/VSync, volume, sensitivity and inversion across restart. Settings must affect only the local player and never pause the server. Gamepad, standalone, packaged cook and resolution changes remain unverified. Do not broaden this scope or run network/latency checks without user approval.
## Wider interaction capture and center reticle — 2026-11-01

No gameplay tests run. Pending approval: a small pickup near the center is acquired within CaptureRadius, exact center target wins, walls reject both exact/assisted focus, maximum range matches the server ability, unavailable items show amber, consumed targets clear promptly. Check center placement across resolutions/letterboxing/split screen, possession/respawn, death, inventory/settings transitions and Runtime exit/re-entry. Verify original Blueprint overrides remain editable. Confirm server validation still rejects stale Pawn, occluded/out-of-range actor requests. Local aim trace is not authoritative weapon damage; camera/muzzle parallax remains unverified. Do not start these scenarios without explicit approval.
## Ballistics / ammo / dedicated connection — 2026-11-01

Actual executed verification: NEXTGENONEEditor Win64 Development compiled and linked (UBT exit 0 / PASS, no compiler diagnostics); NGSetup -BallisticsOnly compiled/saved assets (exit 0, 0 errors / 14 ALS/environment warnings). Logs: Saved/Logs/NGBallisticsBuild.log and NGBallisticsAssets.log. No gameplay tests, PIE, server process or clients were run. Server target Windows/Linux and packaging are unverified; current engine is installed, not a source server build. Launch/package scripts were prepared but not executed.

After separate explicit approval, agree the exact test scope first. Candidate initial scope: one dedicated server + two clients on Gameplay; pistol, rifle and shotgun actual projectile flight/collision/GAS damage/death; wall obstruction and muzzle near-wall rejection; ammo consumption one round per shot vs shotgun pellets; global fire limiter across repeated GA activation/re-equip; correct-type reload across multiple ammo stacks; incompatible/empty ammo rejection; equipped weapon drop and ammo drop/pickup preserving counts; simultaneous requests without duplication; respawn with old corpse/projectile and no starter refill; both clients' projectile/equipment/health presentation. Check impact effects via OnProjectileImpact and local shot audio. Existing hitscan-oriented smoke assumptions have not been validated against the projectile timing/cosmetic changes.

Connection UI pending: direct 127.0.0.1 / LAN address join, invalid address rejection, timeout, failed travel, disconnect/rejoin, status persistence across travel, proper settings input/focus cleanup. Steam remains a disabled placeholder and cannot receive a PASS for sessions/authentication. Linux package/toolchain, WAN/public UDP access, dedicated-source build, services, deployment, high projectile counts/performance and latency/loss require their own approved scope. No such checks were initiated.
## Bone-based bullet impact — 2026-11-01

Implemented physics-body projectile collision, GAS damage followed by multicast physical-animation recoil and per-weapon impulse. Full offline NEXTGENONEEditor compilation passed; asset authoring uses NGSetup -BodyImpactOnly. Logs: Saved/Logs/NGBodyImpactBuild.log and NGBodyImpactAssets.log. No PIE, gameplay, multiplayer, ragdoll or smoke tests executed.

Only after separate approval: one server/two clients, visible reaction and HP reduction for arms/legs/torso/head; no damage from a capsule-only miss; wall occlusion; multiple shotgun pellets and overlapping/repeated reactions; fade back to ALS without lasting simulation or capsule displacement; existing ALS ragdoll, mantle and get-up; lethal hit and old corpse/respawn rejection; server bone pose while off-screen; UI HP updates. Visual strength, constraint stability, dedicated-server physics/body hit registration, other meshes/Physics Assets, bandwidth/performance, latency/loss and custom gravity are unverified. Do not initiate these runs without approval.
## Weapon particles — 2026-11-01

Executed: C++ editor compilation and headless Niagara/Blueprint asset compilation/import. No gameplay/rendering/PIE/network tests. Effects include muzzle, shell ejection, ballistic movement-driven tracer, default concrete and character-sparks impacts. Glass imported for manual assignment, not auto-surface routing. Compile diagnostics and disabled-emitter handling are described in Docs/Gameplay/WeaponVFX.md; do not interpret NullRHI ReadyToRun=false as a rendered test or claim that LWC warnings were eliminated.

Pending after explicit approval: pistol/rifle/shotgun muzzle sockets/direction, shell mesh/scale/ejection, tracer alignment with arc and impact, multiple pellets, character vs world impact placement, switching weapons while bullets are in flight, cleanup after death/range/EndPlay and Runtime travel, no Niagara spawning on dedicated server, client visibility/relevance and high-fire-rate GPU/component budget. LWC, shader/cook, packet loss and latency need separate scope. No such runs were started.
## Tab menu removal / settings diagnostics — 2026-11-01

Executed: Editor C++ build PASS; offline asset migration/Blueprint compilation NGSetup -RemoveALSMenu exit 0, 0 errors / 13 ALS/environment warnings. No PIE, UI gameplay, multiplayer or regression run. Runtime menu-spawn code removed, Tab/legacy debug action mappings removed from a project copy, diagnostics nested in Settings.

After separate approval: confirm Tab no longer displays ALS HUD, Q/V/I/U still route only to gameplay actions, no demo overlay appears, Esc/Back and cursor/focus work repeatedly, current Pawn information follows respawn and diagnostic toggles use the intended ALS debug functions. Also check Runtime transition cleanup, graphics Apply/Back unaffected and Shipping hides diagnostics. No such checks were started.
# Input refactor, 2026-11-02 — проверки ещё не запускались

FSM каркас: сборка PASS, но граф/Definition assets пользователь ещё должен создать по Docs/Gameplay/CharacterStateMachine.md. После отдельного апрува проверить server + owning + simulated роли, initial state/late join/relevancy, UnPossess без остановки body FSM, повторный possession, отложенный local input, rollback Enter/Abort, отказ дубликатов abilities/stacked или Instant GE, снятие ресурсов до смены ASC Avatar и отсутствие воздействия старого state на новую жизнь. Ragdoll/GA_Death ещё не реализованы этим пакетом и не объявляются проверенными.

Следующий пакет подключил базовые grants из CharacterDefinition с server-only выдачей и снятием по handles. Полная сборка NEXTGENONEEditor — PASS, лог Saved/Logs/NGCharacterAbilitySetsBuild.log; предыдущий перенос Input также скомпилирован. Игровые проверки не выполнялись. После отдельного апрува дополнительно нужны: повторный TryInitialize без дубликатов/лечения; смена Pawn и поздний Unbind старого; смерть; повторный possession того же Pawn без повторных Instant эффектов; снятие Duration/Infinite эффектов; несколько Ability Sets; отказ при отсутствующем/некорректном Definition. Эти сценарии не объявляются подтверждёнными сборкой.

Добавлены GameplayInputComponent на Pawn, ALS/Combat адаптер, context/policy handles в UPlayerInputSubsystem. Удалены runtime клавиши/action bindings из Controller; автоогонь перемещён в CombatComponent, server limiter PlayerState сохранён. Новый input требует заполнения CharacterDefinition/InputConfig и UI IA/IMC: Docs/Gameplay/CharacterInputSetup.md.

После отдельного апрува и авторинга ассетов необходимо проверить клавиатуру/геймпад, releases и курки, Jump/Aim без двойного вызова, инвентарь/настройки во время удержания огня/движения, повторный вход/выход UI, respawn и поздние события старого Pawn, отсутствие лишних contexts/bindings и сохранение server fire-rate limit. При UIOnly отдельно проверить Back/Confirm и focus с геймпада. До этого сборка подтверждает только C++/линковку, не работу игры. Старый NGCombatSmoke автоматически не запускался и не считается подтверждением новой схемы.


## GA_Death / local ALS ragdoll — 2026-11-03

Implemented, not gameplay-tested. Grant NGDeathAbility through the existing CharacterDefinition AbilitySet, with no InputTag. Zero health records death and requests FSM Dead; only Dead entry sends the server GAS event State.Dead to the already-granted ability. The ability owns the death sequence and starts ragdoll. Only Pawn bDead and the ragdoll-start phase replicate; body simulation is local. The server-anchor component and all sample/correction logic were removed by user request. No PIE, clients/server, smoke tests or editor gameplay runs were started by the agent.

After separate approval, agree a bounded scope: lethal damage activates death once; Blueprint delay starts ordinary ALS ragdoll; UnPossess during that delay finalizes presentation; new Pawn health/abilities remain independent; late ALS get-up is rejected. Cover both orders of bDead/ragdoll-phase replication and an early activation before BeginPlay. Different final body poses/positions are expected; late join restores death presentation, not corpse position. Actor relevancy remains at the original death location. Dedicated physics, custom gravity, exact corpse interaction, large corpse counts and resurrection remain unverified. Implemented, still untested: Dead entry owns activation. Cover early UnPossess before queued transition, immediate possession of the next Pawn while the old ASC is retained, cancelled/delayed GA_Death, stale pending Pawn, and old-body destruction. A misconfigured/stopped Dead graph must be diagnosed as an authoring error rather than bypassed with direct activation.
Compilation: the previous GA_Death package passed offline NEXTGENONEEditor Win64 Development (UBT exit 0). The final local-only simplification also passed offline NEXTGENONEEditor Win64 Development, UBT exit 0, no compiler diagnostics (Saved/Logs/AgentBuild.log). Actual UBT toolchain was MSVC 14.44.35228. The baseline was missing NGAbilitySystem.cpp: its implementation was recovered from e044d90, adapted to the existing GameplayFoundation interfaces and kept independent of NEXTGENONE/ALS/UI; grants retain the bApplyInstantEffects lifecycle option and damage validates its intended Avatar plus State.Dead. This file must be included in the next commit. Editor/PIE were not launched. Manual AbilitySet configuration is still required before gameplay verification.
FSM activation package: offline NEXTGENONEEditor Win64 Development PASS (UBT exit 0); FSM_CharacterDead Blueprint compiled, Character Death Ability appended after resources, native NGDeathAbility granted once without InputTag in DA_PlayerAbilities; both assets saved/read back through MCP. Backup: Saved/DeathFSMBackup/20261103-183753/. No gameplay scenarios were executed by the agent.

Final build after retaining BoundASC for the pre-BeginPlay death/UnPossess case: NEXTGENONEEditor Win64 Development PASS, UBT exit 0, no compiler diagnostics. Editor closed normally; no gameplay test stage executed by the agent.

## Current contract: direct state grants, no AbilitySet — 2026-11-03

Supersedes earlier base AbilitySet/death-component setup notes. UNGAbilitySet, DA_PlayerAbilities and NGCharacterDeathStateComponent are removed. CharacterDefinition contains InputConfig only. Alive owns the five previous gameplay grants; Dead owns NGDeathAbility, no InputTag, bActivateOnEnter=true. Existing shared resource adapter grants effects/specs, auto-activates flagged specs, and removes owned handles on Exit/Abort/Unbind. Equipment keeps its existing owner via direct Fire/Reload grants. Health initialization is once per new life, not every Alive entry.

Actually completed: full offline NEXTGENONEEditor Win64 Development build PASS, UBT exit 0, no compiler diagnostics; Alive/Dead/Pawn Blueprints compiled; migrated state/character assets saved and read back through MCP. Backup and original grants: Saved/StateGrantMigration/Before and snapshot.json. No gameplay tests, PIE or server/client runs initiated by the agent.

After separate approval, agree a bounded scope: Pawn starts with zero character abilities before FSM entry; Alive gives each configured spec once; Exit removes only its handles/effects; return to Alive restores them without health reset; Dead auto-grants/activates GA_Death once and takes no live input; effect or auto-activation rejection rolls back; equipment remains independent with no fire-limiter bypass; UI release suppresses held ability input. Cover early UnPossess before Dead transition, immediate next Pawn, pre-BeginPlay death, source Avatar protection, state hierarchy overlap rejection, cancelled/delayed death, replication order and late join. Local corpse positions/poses may differ by design. These scenarios are unverified, not PASS results.

State-owned IMC authoring (2026-11-03): IMC_NG_Character priority 10 moved to Alive.MappingContexts; InputConfig base contexts empty, same IMC retained in StateMappingContexts catalog. Saved/read-back via MCP, no C++ changes/build or gameplay tests. Pending approved coverage: context absent before Alive, present exactly once in Alive, removed on Exit/UnPossess, restored on re-entry, held-input release and unchanged UI/gamepad policies. Parent-state leases remain independent.

InputConfig IMC removal (2026-11-03): C++ full editor build PASS, UBT exit 0. Reflection/read-back confirmed obsolete fields absent, 13 native/3 GAS action routes retained, Alive context priority 10 retained. State-entry cross-validation is implemented but its runtime rejection/rollback and input/possession/UI behavior were not exercised; they require separate approval. Asset backup: Saved/InputConfigCleanup/20261103-193514/.

## Current death lifecycle without Character flags — 2026-11-03

Supersedes earlier bDead/phase replication and delayed-montage notes. Character stores no replicated death flag or ability-activation protocol. IsDead uses the Pawn FSM StateTag. Dead grants NGDeadStateEffect (State.Dead) and auto-activates GA_Death. Its configured NGDeathRagdollCue starts local ALS ragdoll. Proxies restore presentation from the replicated FSM on entry/BeginPlay, independently of the former ASC. Generic state-resource readiness gates early UnPossess/next-life handoff. Body physics remain unsynchronized.

Actually performed: offline NEXTGENONEEditor Win64 Development build PASS, UBT exit 0, no compiler diagnostics (Saved/Logs/AgentBuild.log). NGSetup -StateLifecycleOnly authoring exit 0, 0 errors / 11 existing ALS/environment warnings (Saved/Logs/DeathLifecycleAssets.log). Alive/Dead/Pawn Blueprints compiled and saved. Alive saved with State.Alive, 5 abilities, 0 effects, 1 context; Dead with State.Dead, 1 auto-activated ability, 1 effect, 0 contexts and a valid presentation cue. InputConfig force-resaved with 13 native / 3 ability routes, without temporary route edits. Backup: Saved/DeathLifecycleRefactor/20261103-200722/.

No PIE, gameplay, networking, smoke or automated tests were started. Await separate approval for a bounded server + two-client run: lethal transition once; GAS tag/effect cleanup before new Avatar; old corpse damage cannot affect new life; early UnPossess and immediate respawn; pre-BeginPlay death; late join/relevancy and either cue/state arrival order; duplicate activation and cleanup failures. Configuration with failed/stopped graph can still block pending handoff. Delayed death presentation requires explicit FSM phases; it is not provided by this immediate-death package.

## Mantle without ActorComponents — 2026-11-03

UNGMantleAdapterComponent/UNGALSMantleExecutor and the project BP_ALSMantleExecutor are removed. Pawn SCS no longer owns ALS MantleComponent. GA_Mantle owns authority probes, grant-scoped auto-catch, replicated CharacterMovement Root Motion Source, ASC montage and cue lifecycle. Legacy ALS assets/AnimBP are unchanged. Authoring extracted 39 Type/Overlay animation configurations and the blend curve; all montages are in-place (HasRootMotion=false). AssetRegistry reported no remaining package referencers before deleting the legacy component Blueprint.

Actually performed: offline NEXTGENONEEditor Win64 Development build PASS, UBT exit 0, no compiler diagnostics (AgentBuild.log); initial targeted asset migration exit 0 / 0 errors / 11 ALS/environment warnings (MantleAbilityAssets.log); legacy asset retirement exit 0 / 0 errors / 10 warnings (MantleRetireAssets.log). No PIE, gameplay or network tests started. Backups: Saved/MantleAbilityMigration/Before/ and Saved/RefactorBackup/Interaction/.

Fresh authoring load after removing native component classes: exit 0 / 0 errors / 11 ALS/environment warnings (MantleAbilityFinalAssets.log). GA_Mantle/Pawn Blueprint compiled and saved; 39 configurations retained and no mantle components in Pawn SCS/CDO. Legacy component file physically removed after matching the backup SHA256. This authoring check does not establish runtime/network correctness.

Pending separate approval: grounded/high/falling-catch mantle; overlay montage selection; obstruction/blocked destination; source removal on cancel/death/UnPossess/Alive exit; held-input/UI restrictions; repeated grants without duplicate auto-catch timers; old-Avatar callbacks after respawn; server/owner/proxy montage, cue ordering and movement corrections. Static ledges and gravity -Z only; moving platforms/arbitrary gravity and client prediction are not implemented. Previous manual confirmation of mantle does not validate this replacement.

## Current mantle: original ALS executor restored — 2026-11-03

Supersedes the previous component-free implementation. User reported that version appeared not to work; its runtime failure cause was not established. GA_Mantle is now a thin GAS wrapper. One restored BP_ALSMantleExecutor : UNGALSMantleExecutor : UALSMantleComponent owns the original ALS geometry, Timeline, animation selection and motion. Custom FNGMantleRootMotion, montage/curve configuration in the ability and native mantle cue are removed. Project subclass only routes GAS initiation/completion/cancellation and server-validated results to clients; legacy client-supplied ledge RPC is rejected. Plugin source and AnimBP unchanged.

Actually performed: full offline NEXTGENONEEditor Win64 Development PASS, UBT exit 0, no compiler diagnostics (Saved/Logs/AgentBuild.log). Targeted authoring exit 0, 0 errors / 11 ALS/environment warnings (Saved/Logs/MantleALSRestoreAssets.log); Component/Ability/Pawn Blueprint compiled and saved, one ALS-derived component present. Backup before restoration: Saved/MantleALSRestore/20261103-205730/.

No agent PIE/gameplay/network tests. Await separate approval for manual and automatic mantle, low/high/falling catch, overlays, owner/proxy execution, rejected client ledge, cancellation/death/UnPossess/Alive exit and old-Pawn isolation after respawn. Original ALS world-Z assumptions remain; arbitrary gravity and late-join behavior mid-mantle are unverified. Commit currently precedes the ALS probe (no cost/cooldown configured by this package).

## Generic HFSM Blueprint input/context — 2026-11-03

HFSMPlayerStateComponent removed; State and StateComponent own Blueprint GetPlayerController/GetLocalPlayer and input-binding lifecycle. UI/layout/inventory derive directly from StateComponent. Owner-derived local-player resolution replaces the old captured player context; no first-player fallback. Each Blueprint input receiver has an isolated transient EnhancedInputComponent on its owner's stack; scoped bindings are released on exit/abort/rollback and refreshed after controller availability changes. Existing mapping/policy ownership in PlayerInputSubsystem is unchanged.

Actually completed: full NEXTGENONEEditor Win64 Development build PASS, UBT exit 0 (AgentBuild.log). Authoring compiled 13 related Blueprint, exit 0 / 0 errors / 16 warnings (HFSMInputAssets.log). Six FSM assets resaved; unrelated authoring writes restored from backup, including BP_Button with pre-existing invalid transition-tag warnings. Compatibility ClassRedirect retained. Backup/report: Saved/HFSMInputMigration/20261103-214303/.

No tests run. Pending separate approval: Blueprint IA events in state and component, repeated entry and rollback without duplicate input, inherited bindings, consume/propagation, keyboard/gamepad, UIOnly focused-widget handling, two local players, controller arrival/replacement/UnPossess and no local input on remote/dedicated roles. Existing test source was updated for the new base hierarchy and isolated bindings, but was not executed.

Simplification requested by user: bHasBlueprintInputEvents and State/Component RefreshBlueprintInputBindings/ReleaseBlueprintInputBindings removed. Binding now occurs once on Enter; Exit/Abort/rollback/destruction release the scope. There is no Tick-based controller polling/rebinding. A controller must be available at Enter; availability/replacement requires state re-entry. Earlier automatic-refresh notes describe the superseded implementation. No runtime tests performed for this simplification.

Inventory window simplification (2026-11-03): NGInventoryWindowMounter removed; InventoryStateComponent no longer uses UIWidgetMounter, project Controller, input actions/subsystems/policies or runtime /Game fallback paths. State Enter/Exit owns the configured NGInventoryPanel window. Explicit Close/RequestClose removes it and queues the configured named FSM event; state Exit/Abort does not emit another close event. Panel-native key routing removed. Existing item equip/drop transport is unchanged. User will configure Blueprint state placement, PanelClass, CloseEvent, input policy/actions/focus and window close button. No Blueprint authoring, PIE or tests initiated by the agent for this package.

Setup cleanup (2026-11-03): NGSettingsAssets.cpp deleted; NGSetupCommandlet reduced to explicit WeaponVFXOnly dispatch. Historical automatic setup, map/default rewriting and one-time asset migrations removed. Earlier NGSetup command lines in this document are historical evidence, not supported current operations. Full offline NEXTGENONEEditor Win64 Development build PASS, UBT exit 0, no compiler diagnostics (Saved/Logs/AgentBuild.log). No commandlet runs, Blueprint edits, PIE or tests performed for this cleanup. Source backup: Saved/SetupCleanup/20261103-225303/.

Inventory mounting revision (2026-11-03): InventoryStateComponent now derives from UIWidgetMounterComponent and uses its WidgetClass/PlaceholderID/Layer and mount/demount lifecycle. Own PanelClass/ZOrder/window creation removed. Close only queues CloseEvent; state Exit/Abort demounts through the base class and unbinds the panel delegate. No project Controller or input-system dependency added. Blueprint setup remains manual. Full offline NEXTGENONEEditor Development build PASS, UBT exit 0 (AgentBuild.log); no Blueprint authoring or tests performed.

HFSM void lifecycle (2026-11-03, explicitly approved scope): all StateComponent Enter overrides now return void; State already used void Enter. Shared enter failure/rollback, AbortEnter, ActiveComponents and HasEnteredSuccessfully/OnEnterFailed removed. State invokes each configured component; invalid input/layout/widget/GAS configuration is handled locally, without rolling back other components. GAS grant/effect validation and owned-handle cleanup remain. Existing test sources were adapted to the new API and continuation semantics, not executed. User handles removal of stale Blueprint OnEnterFailed/HasEnteredSuccessfully nodes. Full offline NEXTGENONEEditor Win64 Development build PASS, UBT exit 0, no compiler diagnostics (Saved/Logs/AgentBuild.log). No Blueprint edits, PIE, gameplay or network tests. Previous rollback/AbortEnter instructions are historical.
