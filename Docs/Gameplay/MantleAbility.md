# Mantle: ALS executor + GAS wrapper

Актуальный контракт — 2026-11-03. Предыдущая реализация с собственной геометрией/Root Motion Source в абилке удалена после сообщения пользователя, что залезание не работает. Причина runtime-сбоя той версии не установлена; вместо её расширения возвращён исходный ALS-пайплайн.

## Разделение ответственности

- `GA_Mantle` — ServerOnly GAS-обёртка: проверка возможности активации, Commit, запуск компонента, ожидание его завершения, отмена. В ней нет traces, расчёта траектории, curve assets или движения Pawn.
- `BP_ALSMantleExecutor` на Pawn — восстановленный Blueprint с прежними графами выбора mantle assets, наследник `UNGALSMantleExecutor : UALSMantleComponent`.
- Исходный `UALSMantleComponent` выполняет `MantleCheck`, `MantleStart`, `MantleUpdate`, `MantleEnd`: геометрию, local-space ledge, correction curves, Timeline, монтаж и движение. Его реализация в плагине не изменена и не скопирована в способность.
- Проектный наследник только связывает ALS с GAS/сетью: старт/отмена на клиентах, монитор завершения, восстановление ссылки Timeline старого Blueprint, автозахват через запрос GAS. Отдельного MantleAdapter нет.

## Сетевой маршрут

Сервер активирует GA_Mantle и вызывает StartFromAbility. Компонент вызывает оригинальный ALS MantleCheck. Его синхронный серверный результат доставляется владельцу и proxies; сервер не запускает MantleStart второй раз. Старый Server_MantleStart удалён из ALS. ReplicateMantleStart теперь обычный virtual callback: проектный executor распространяет результат только внутри собственной серверной проверки. См. [ALSAuthority.md](../Networking/ALSAuthority.md).

Прямой Jump delegate исходного компонента отсоединён. Вместо автоматического ALS Tick-пути компонент запрашивает выданную GAS-абилку в падении; геометрию по-прежнему ищет ALS при активации. Без выданной способности залезание не запускается этим путём. Отмена/смерть/потеря ASC или Controller останавливают Timeline и собственный montage. Компонент остаётся на исходном Pawn, поэтому callback не выбирает новый Avatar из переиспользованного ASC.

## Где настраивать

Открыть `/Game/Gameplay/Components/BP_ALSMantleExecutor`:

- GroundedTraceSettings / FallingTraceSettings, MantleObjectDetectionProfile, WalkableSurfaceDetectionChannel — поиск уступа средствами ALS.
- MantleTimelineCurve и существующий GetMantleAsset Blueprint graph — кривые, montage, offsets и выбор по типу/overlay.
- bAutomaticFallingCatch / AutoCatchInterval — запрос автозахвата через GAS.

В `/Game/Gameplay/Abilities/GA_Mantle` остаются GAS-настройки. Alive по-прежнему выдаёт эту способность; маршрут Input.Mantle/IMC не менялся. В Pawn добавлен ровно один mantle-компонент; остальные графы, пользовательские настройки и AnimBP сохранены. Бывшие Animations/BlendInCurve и прочие поля самостоятельного executor из GA_Mantle удалены.

## Проверено и ограничения

Офлайн NEXTGENONEEditor Win64 Development: PASS, UBT exit 0, без compiler diagnostics (Saved/Logs/AgentBuild.log). Авторинг NGSetup -MantleAbilityOnly: exit 0, 0 errors / 11 ALS/environment warnings (Saved/Logs/MantleALSRestoreAssets.log). Component/Ability/Pawn Blueprint скомпилированы и сохранены. Лог подтвердил один ALS-derived компонент и исходный выбор assets. Бэкап текущего Pawn/абилки перед восстановлением: Saved/MantleALSRestore/20261103-205730/; component Blueprint восстановлен из Saved/MantleAbilityMigration/Before/.

PIE, игровые и сетевые тесты агентом не запускались. Подтверждена сборка и авторинг, а не работоспособность в игре. После отдельного апрува проверить ручное/автоматическое залезание, низкий/высокий уступ, разные overlays, owner/proxy воспроизведение, отмену при смерти/UnPossess/выходе Alive и смену Avatar. Сохраняются world-Z допущения ALS; произвольная гравитация не поддержана этим пакетом. GAS Commit выполняется перед ALS-попыткой; если позже назначить стоимость/cooldown, они будут расходоваться и при отсутствии подходящего уступа. Сетевой late join посреди короткого mantle отдельно не проверен.
# Событийное восстановление оружия (2026-11-04)

Покадровая проверка оружия в Character.Tick удалена. Project executor после Super.BeginPlay назначает TimelineFinished на собственный callback: сначала вызывает исходный ALS MantleEnd (включая Blueprint UpdateHeldObject), затем Character.ApplyCombatPresentation. Это действует на всех ролях, не только на сервере. При отмене presentation обновляется после смены movement mode. На Character также используются Equipment/ADS events, OnOverlayStateChanged после Super, конец BeginPlay и RagdollEnd после Super. Геометрия, Timeline-кривые и AnimBP сохраняются; сетевой вход исходного plugin изменён по ALSAuthority.md. Сборка и игровые проверки этой правки не запускались.
