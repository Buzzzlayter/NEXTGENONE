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

## GA_Punch — 2026-11-01, runtime ожидает апрува

Выполнена только реализация и сборка: UBT Editor Development PASS, без compiler diagnostics (NGPunchBuild.log); создание/компиляция ассетов NGSetup -PunchOnly exit 0, 0 errors, 10 warnings (NGPunchAssets.log). В DA_PlayerAbilities добавлена GA_Punch без замены остальных записей, исходный набор скопирован в Saved/RefactorBackup/Abilities. PIE/сетевые/автоматические gameplay tests этого прохода не запускались, Editor MCP при закрытом редакторе недоступен.

После отдельного апрува проверить в согласованном объёме:

- Два клиента + сервер: U, подойти к противнику, ЛКМ. Серверное попадание должно изменить HP 100→80; наблюдатели и WBP_HealthPanel видят обновление. Цель смещается от атакующего, collision не пропускает через стену.
- Промах, стенка между игроками, цель позади, несколько игроков друг за другом: не более одной цели, не сквозь препятствия. Capsule sweep имеет объём, это не точное попадание по костям кулака.
- Spam/cancel/equip/unequip не обходят 0.55-секундный cooldown; во время Windup повторная активация и конфликтующие действия не запускаются.
- Death/unpossess/respawn атакующего до момента попадания: старый Pawn не наносит отложенный удар. Corpse не повреждает новую жизнь, смертельный удар не включает движение мёртвой цели.
- Попадание в цель во время Dash/Mantle: movement ability отменяется, root motion не подавляет knockback и старый mantle timeline не возвращает Walking.
- При экипировке ЛКМ стреляет; после U — бьёт. Косметические delegates/опциональный PunchMontage видны обоим клиентам; урон не зависит от монтажа/AnimNotify.

Непроверенные ограничения: коррекции owning client после server LaunchCharacter, latency, packet loss, ragdoll, произвольная gravity, точная синхронизация выбранного пользователем montage с Windup. Предсказание/rewind удара и физическое отбрасывание corpse не реализованы. Требуемое ручное действие для анимации — назначить совместимый PunchMontage в GA_Punch; без него игровая логика остаётся доступна, но взмах кулака не анимирован.

## Уточнение Air Dash — 2026-11-01

Деш изменён по запросу пользователя: активация только при ALS InAir и CharacterMovement.IsFalling, с повторной проверкой Falling перед Commit. Root motion сохраняет горизонтальный импульс после окончания через MaintainLastRootMotionVelocity; ограничение до walking MaxSpeed снято. Gravity продолжает действовать. Кнопка Q, скорость, длительность и cooldown остаются прежними. Вертикальный рывок и управление направлением WASD не добавлены.

Компиляция в открытом редакторе: Live Coding succeeded / Reload-Reinstancing Complete, No object changes detected. Подтверждение сохранено в Saved/Logs/NGAirDashLiveCoding.log; полная offline-сборка этого изменения не выполнялась.

В LiveCodingConsole также есть диагностические сообщения Cannot find image section .voltbl; несмотря на них, итоговый статус — Patch creation successful и Live coding succeeded. Копия вывода сохранена как Saved/Logs/NGAirDashLiveCodingConsole.log. Это не подтверждение runtime-результата.

## Lyra inventory / pistol / rifle — 2026-11-01, до тестового апрува

Добавлена реализация переноса, описанная в Docs/Research/LyraPort.md. UBT Editor Development и обработка ассетов NGSetup -LyraWeapons выполнены; логи NGLyraPortBuild.log / NGLyraWeaponAssets.log. Командлет exit 0, 0 errors / 10 warnings, содержит NG_INVENTORY_HFSM_BOUND и NG_LYRA_WEAPONS_SAVED. Первоначальная сборка порта имела deprecation warning NonInstanced и затем недостающий SlateCore link symbol; оба исправлены. Итоговый compiler result фиксируется в NGLyraPortBuild.log. Игровые тесты не запускались, исторические Combat PASS не подтверждают этот порт.

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

Manual preparation: place actors on the existing Gameplay map; choose ItemClass on each BP_ItemPickup (inventory ID class, not weapon DA); set StackCount. Existing Runtime HFSM mounts WBP_InteractionPrompt. BP_InteractiveObject exposes OnInteractedOnServer and OnStateChanged. Custom interactables must replicate and block the Interaction trace channel. Camera parallax, pickup volume size, drop placement near walls/slopes and legacy BP_WeaponPickup component migration remain unverified at runtime. Arbitrary gravity support is limited to gravity-aware drop placement here; a full gravity integration is not claimed. Packaging/cook and physics throwing are not verified or included.
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
## Lyra particles — 2026-11-01

Executed: C++ editor compilation and headless Niagara/Blueprint asset compilation/import. No gameplay/rendering/PIE/network tests. Effects include muzzle, shell ejection, ballistic movement-driven tracer, default concrete and character-sparks impacts. Glass imported for manual assignment, not auto-surface routing. Compile diagnostics and disabled-emitter handling are described in Docs/Gameplay/LyraWeaponVFX.md; do not interpret NullRHI ReadyToRun=false as a rendered test or claim that LWC warnings were eliminated.

Pending after explicit approval: pistol/rifle/shotgun muzzle sockets/direction, shell mesh/scale/ejection, tracer alignment with arc and impact, multiple pellets, character vs world impact placement, switching weapons while bullets are in flight, cleanup after death/range/EndPlay and Runtime travel, no Niagara spawning on dedicated server, client visibility/relevance and high-fire-rate GPU/component budget. LWC, shader/cook, packet loss and latency need separate scope. No such runs were started.
## Tab menu removal / settings diagnostics — 2026-11-01

Executed: Editor C++ build PASS; offline asset migration/Blueprint compilation NGSetup -RemoveALSMenu exit 0, 0 errors / 13 ALS/environment warnings. No PIE, UI gameplay, multiplayer or regression run. Runtime menu-spawn code removed, Tab/legacy debug action mappings removed from a project copy, diagnostics nested in Settings.

After separate approval: confirm Tab no longer displays ALS HUD, Q/V/I/U still route only to gameplay actions, no demo overlay appears, Esc/Back and cursor/focus work repeatedly, current Pawn information follows respawn and diagnostic toggles use the intended ALS debug functions. Also check Runtime transition cleanup, graphics Apply/Back unaffected and Shipping hides diagnostics. No such checks were started.
# Input refactor, 2026-11-02 — проверки ещё не запускались

FSM каркас: сборка PASS, но граф/Definition assets пользователь ещё должен создать по Docs/Gameplay/CharacterStateMachine.md. После отдельного апрува проверить server + owning + simulated роли, initial state/late join/relevancy, UnPossess без остановки body FSM, повторный possession, отложенный local input, rollback Enter/Abort, отказ дубликатов abilities/stacked или Instant GE, снятие ресурсов до смены ASC Avatar и отсутствие воздействия старого state на новую жизнь. Ragdoll/GA_Death ещё не реализованы этим пакетом и не объявляются проверенными.

Следующий пакет подключил базовые grants из CharacterDefinition с server-only выдачей и снятием по handles. Полная сборка NEXTGENONEEditor — PASS, лог Saved/Logs/NGCharacterAbilitySetsBuild.log; предыдущий перенос Input также скомпилирован. Игровые проверки не выполнялись. После отдельного апрува дополнительно нужны: повторный TryInitialize без дубликатов/лечения; смена Pawn и поздний Unbind старого; смерть; повторный possession того же Pawn без повторных Instant эффектов; снятие Duration/Infinite эффектов; несколько Ability Sets; отказ при отсутствующем/некорректном Definition. Эти сценарии не объявляются подтверждёнными сборкой.

Добавлены GameplayInputComponent на Pawn, ALS/Combat адаптер, context/policy handles в UPlayerInputSubsystem. Удалены runtime клавиши/action bindings из Controller; автоогонь перемещён в CombatComponent, server limiter PlayerState сохранён. Новый input требует заполнения CharacterDefinition/InputConfig и UI IA/IMC: Docs/Gameplay/CharacterInputSetup.md.

После отдельного апрува и авторинга ассетов необходимо проверить клавиатуру/геймпад, releases и курки, Jump/Aim без двойного вызова, инвентарь/настройки во время удержания огня/движения, повторный вход/выход UI, respawn и поздние события старого Pawn, отсутствие лишних contexts/bindings и сохранение server fire-rate limit. При UIOnly отдельно проверить Back/Confirm и focus с геймпада. До этого сборка подтверждает только C++/линковку, не работу игры. Старый NGCombatSmoke автоматически не запускался и не считается подтверждением новой схемы.

