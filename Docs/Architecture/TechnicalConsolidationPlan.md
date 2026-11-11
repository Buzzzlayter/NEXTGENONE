# План реализации TechnicalConsolidation

2026-11-10: C++ Equipment/Combat-проход и процедура миграции подготовлены; сборка,
авторинг ассетов и runtime-приёмка не выполнялись. Текущий контракт и оставшиеся шаги —
[Equipment.md](../Gameplay/Equipment.md).

Дата: 2026-11-07. База: main `19a5ebc`.
Основание: [TechnicalConsolidation.md](TechnicalConsolidation.md). Это декомпозиция спецификации, не новый архитектурный вариант и не отчёт о выполнении.

Уточнено 2026-11-10 для оружия + джетпака (`origin/feature/jetpack`, `1904161`).
Контракты GAS и модель источников — раздел 3 спецификации. Сторонний framework не переносится.
Back/Feet/Torso — типизированные ячейки существующего Inventory, не второй контейнер Equipment.
Изменён порядок Equipment-прохода: полная переработка Inventory не является его предпосылкой.
Документ остаётся планом; runtime-изменения и проверки не выполнены этим уточнением.

## Правила прохода

- Работаем последовательными небольшими пакетами; каждый имеет самостоятельный diff, критерий приёмки и список проверок. Коммиты/PR — по отдельному поручению пользователя.
- Сборку выполняет пользователь. Агент не запускает UBT, Live Coding, Blueprint compilation, Data Validation/cook или игровые/сетевые проверки без согласования соответствующего этапа.
- Статическое выполнение задачи и runtime-приёмка отмечаются отдельно. До приёмки изменения владельцев состояния следующий такой пакет не начинаем.
- Текущие незакоммиченные диагностические правки и пользовательский BP_Layout_Inventory сохраняются; они не становятся частью рефакторинга автоматически.
- Не повторяем уже выполненный вынос Fire/Reload, централизацию тегов, удаление Combat/Inventory из input и унификацию inventory invalidation. Проверяем сохранение их контрактов.

## 0. Закрыть известную регрессию и зафиксировать baseline — этап A

**Результат:** известная причина первого неудачного переноса из коробки после запуска Editor и список реально работающих сценариев до рефакторинга.

1. После пользовательской сборки получить холодный неуспешный и последующий успешный перенос с записями NG_INVENTORY_TRANSFER.
2. Определить участок отказа: Drop → UI → запрос → server access/snapshot → TransferTo → replication/UI refresh. Не объявлять загрузку ассетов установленной причиной без данных.
3. Исправить обнаруженную причину отдельным пакетом, затем убрать лишнюю временную диагностику. Обновить Docs/Testing/MultiplayerCombat.md фактическим результатом.
4. Зафиксировать относящиеся к сценарию assets: Inventory Layout игрока/коробки, InitialContents, панели/слоты, Alive/Dead definitions, InputConfig, ItemDefinition и WeaponDefinition.
5. Использовать матрицу из спецификации: equip/unequip/re-equip, смена A→B, reload, item transfer/drop, medkit, UI-block/death, respawn и owner/proxy presentation. Для каждого указать проверено / не проверено / известный дефект.

**Приёмка:** холодный перенос проверен повторно; известные дефекты не скрыты формулировкой «baseline работает». Без этого можно готовить read-only API, но не переписывать мутации Inventory.

## 1. Один read-only snapshot Inventory — первая часть B

**Файлы:** GameplayFoundation/Inventory/NGInventoryManagerComponent.{h,cpp}, NEXTGENONE/Gameplay/Inventory/NGInventoryPanel.cpp.

1. Добавить один C++ read API, возвращающий snapshot слотов за один обход Entries: логический SlotIndex, правила и текущее содержимое/Count. Snapshot не хранится как второе состояние.
2. Пустые и ещё не разрешённые item references сохраняют слот; UI не считает их удалёнными.
3. Перевести Refresh панели на этот API; фильтрацию групп и преобразование в FUIInventorySlotData оставить в проектном UI adapter.
4. Не обращаться к Entries[SlotIndex] как к сетевому контракту и не добавлять постоянно синхронизируемую index map.

**Приёмка:** панель получает данные одним обходом; порядок/фильтры/пустые слоты и поздний OnRep item definition сохраняются. Виджеты не приобретают новые readiness-подписки.

## 2. Manager — координатор мутаций Inventory — вторая часть B

**Файлы:** общий InventoryManager и FNGInventoryList, проектные PlayerInventoryManager и NGInteraction.cpp (pickup/drop).

1. Составить короткую таблицу публичных add/restore/consume/transfer методов и всех реальных вызывающих мест, включая deferred add и ручной Notify.
2. Перенести создание item, исполнение fragments и проверки операции из FastArray в Manager. FastArray отвечает за записи, сериализацию и dirty marking.
3. Сузить публичный API; скрыть детали bNotify и внутренние варианты add/remove. Сохранить нужную pickup/drop границу commit до перевода её вызывающих мест; не заменять её преждевременной публикацией.
4. Сохранить FMutationScope, проверки после fragment callbacks, регистрацию subobjects, rollback при отказе и запрет повторного входа в мутацию.

**Приёмка:** pickup публикуется после переноса, drop — после debit, transfer показывает целиком итог обоих контейнеров. Stateful split/merge, Count/StatStacks и прежние правила занятых слотов не меняются. Нового transaction/service слоя и completion RPC нет.

## 3. Контракт и данные Equipment — подготовка C

**Предлагаемые файлы:** NEXTGENONE/Gameplay/Equipment/NGEquipmentDefinition и NGEquipmentInstance; существующие NGCombat.h, NGWeaponDefinition.cpp и общие GAS grants/handles; план миграции assets.

1. Сразу описать независимые Hands/Back. Оружие сначала переносится с прежним правилом Action; джетпак применяется из типизированной inventory-ячейки Back. Очистку/сохранение equipment-ячеек после смерти и повторное применение на respawn уточнить до соответствующей реализации. Inventory/ASC остаются на PlayerState, runtime equipment — на Pawn.
2. Отдельно согласовать изменение поведения при невалидном новом equip: спецификация рекомендует сохранять прежний предмет после preflight-отказа. Для отказа уже после начала замены описать явный результат; не обещать восстановления старого активного действия.
3. Описать запись SlotTag/definition/EquipmentInstance, поколение конкретной установки, GrantedASC и принадлежащие handles. SourceObject=EquipmentInstance; instance связывает исходный item с текущим Avatar. Remembered selection хранится отдельно от active record. Снятие Back не инвалидирует Hands.
4. Ввести общий EquipmentDefinition; WeaponDefinition расширяет его оружейными параметрами. Definition задаёт grants/effects и политику startup activation данными. Использовать существующие FNGAbilityGrant/FNGGrantedHandles; общий AbilitySet asset вводить без обязательной переделки FSM. Броня и двуручность остаются отдельными задачами.
5. До переноса сериализуемых полей составить соответствие старых/новых полей, backup и Core Redirect/resave-процедуру. Учесть текущие Fire/Reload/Aim native defaults.
6. Описать типизированные inventory-ячейки в существующем UNGInventoryLayoutDefinition: SlotTag + AcceptedItems; EquipmentDefinition задаёт требуемый тип ячейки. Back/Feet/Torso хранят предметы в том же FastArray, без второго Equipment-контейнера. Общая equip ability и drag инициируют серверный перенос; Equipment материализует применение после размещения и снимает ресурсы при выходе. Согласовать preflight до commit и swap/refusal при отсутствии места для прежнего предмета. Примеры настройки — раздел 3 спецификации.

**Приёмка:** описаны источник истины, политика отказа, private/public snapshots, GAS-контракт раздела 3 спецификации и план сохранения authored данных. Различаются grant, запуск при установке и запуск по вводу/событию; ASC-теги ношения предоставляются GE. Никакие redirects или legacy поля не удаляются до миграции и проверки ссылок.

## 4. Один владелец runtime экипировки — основная часть C

**Предлагаемые файлы:** NEXTGENONE/Gameplay/Equipment/NGEquipmentComponent.{h,cpp}. Изменяемые: NGCombat, NGItemActivateAbility, NGEquipmentControlAbilities, NextgenoneCharacter.

1. Перенести из Combat selection/active record, inventory observation и grant/revoke в Equipment-компонент Pawn. В тот же пакет подключить вызовы; не оставлять двух изменяемых копий записи.
2. Сначала preflight item/допустимой inventory-ячейки/definition/abilities/resources, затем замена и выдача полного набора. Для Back/Feet/Torso назначение определяет Inventory, для оружия сохраняется Action-selection. При неполной выдаче снять только свои handles и не публиковать частичный equip как успешный.
3. Unequip сначала инвалидирует поколение, затем отменяет свои действия и снимает grants/effects/presentation. Cleanup идемпотентен и не снимает grants Alive/Dead.
4. Item.bEquipped обновляет только Equipment как UI-проекцию. Inventory notifications не вызывают рекурсивную мутацию контейнера.
5. На время перехода старые getters Combat допустимы только как forwarding API без собственного состояния. Удалять после перевода всех C++/BP-потребителей.
6. До startup activation подготовить item/Avatar source, все grants и обязательные effects. Активация учитывает NetExecutionPolicy и readiness владельца. Обязательный startup в первом пакете исполняется сервером; локальный/predicted startup не подтверждает серверный equip и не требует нового completion RPC. На каждом callback проверять актуальность установки; отказ обязательного startup очищает новую установку. Не обещать rollback Instant GE или совершённых действий.
7. Накапливать handles всех наборов. Обеспечить независимые while-equipped GE, сохранение общего ASC-тега при снятии одного из двух источников и явное отклонение неоднозначных command InputTags. Cleanup использует захваченный GrantedASC.

**Приёмка:** в Combat нет authoritative record, WeaponHandles или собственного grant/revoke пути. Unequip/re-equip сохраняет selection и magazine, повторная выдача отсутствует. Постоянный fire-rate gate остаётся на ASC.

## 5. Привязка способностей и shot context — этап D

**Файлы:** NGGameplayAbilityFromEquipment, NGWeaponAbilities, NGAimAbility, NGCombat, NGPrimaryAttackAbility и баллистический projectile.

1. Equipment abilities получают SourceObject=конкретный EquipmentInstance и через него исходный item. Перед действием проверяют активную запись, поколение и текущий Avatar. Одноразовая ItemActivateAbility сохраняет SourceObject=item.
2. Убрать разрешение «текущее оружие Combat» после ожидания. Перед delayed commit заново проверить Avatar, поколение, item и принадлежащий spec.
3. Сохранить fresh view RPC → server validation → ServerOnly GA_Fire. Захватить один неизменяемый shot snapshot до callbacks и использовать его для всех pellets/cosmetics.
4. Reload сохраняет reserve debit + magazine credit до уведомления; отмена не расходует патроны. Старый callback не может изменить ammo нового equip.
5. PrimaryAttack пока сохраняет LocalOnly press/release и существующий auto-fire timer. Остановка при release/UI/death/смене записи обязательна; перенос timer в ability не входит автоматически.

**Приёмка:** A→B во время fire/reload/ADS не переносит действие на B. Переэкипировка не сбрасывает cooldown. Поздний снаряд сохраняет source-life защиту. ADS-required fire, spread и server projectile anchor не меняются.

## 6. Presentation и завершение миграции — этап E

**Файлы:** Equipment definitions/component, Character.ApplyCombatPresentation, ALS attachment adapter, Reticle/UI consumers и соответствующие assets.

1. Equipment публикует owner-only identity и достаточный общедоступный appearance snapshot. Simulated proxies не требуют private item subobject.
2. В одном проектном месте применяются mesh, attachment и ALS overlay. Для AttachSocket выбрать и реализовать реальный контракт либо мигрировать неиспользуемое поле; существующее поле само по себе поддержкой не считать.
3. Проверить обновления при initial replication, смене equip, mantle/ragdoll и late relevancy. Не возвращать polling presentation в Character.Tick.
4. Пересохранить затронутые assets по согласованной миграции, затем убрать forwarding API и только подтверждённо ненужные legacy поля/redirects.
5. Разделить логическое место и способ visual: socket attachment для устройств/оружия, modular skeletal part с привязкой к позе тела для одежды. Зафиксировать поддерживаемый skeleton/Leader Pose контракт. Первое подключение джетпака реализует socket-путь; runtime модульной одежды и её assets — отдельный пакет на том же контракте. Одежда не меняет оружейный ALS overlay, поздний callback не возвращает visual снятой установки.

**Приёмка:** owner/proxy видят корректный visual, UI остаётся на общем item snapshot, Inventory не знает оружие/ALS. C–E завершаются как один переход без постоянного двойного API.

## 7. Второй тип и адресные упрощения — F/G

- **F — выбран джетпак.** Добавить предмет в Back через общий equip lifecycle, выдачу способности тяги и независимое крепление на спине. Постоянную выдачу Jetpack ability из Alive убрать; gameplay IMC остаётся у Character FSM. Надетое устройство и активная тяга имеют разные ASC-теги. Снятие/drop/transfer/death отменяют только принадлежащие устройству действия и effects. Параметры из definition согласовать с SavedMove/replay; не менять параметры прошлых moves на параметры другого предмета. Оружие в Hands продолжает работать независимо. Топливо, броня и persistence не добавляются автоматически.
- **Приёмка F:** общий путь поддерживает passive GE, ASC-теги и явную startup activation без switch по типам в Combat/Inventory/UI. Матрица включает два источника одного тега, несколько наборов grants, startup failure, owner/proxy/late replication и снятие при активной тяге. Это будущие проверки после отдельного разрешения.
- **G1 — Medkit:** компактный snapshot операции и одна идемпотентная cleanup-функция; сохранение всех причин отмены. Общую delayed-item базу не создавать без второго потребителя.
- **G2 — FSM resources:** описать фактические Enter/Acquire/Activate/Ready/Suspend/Exit/Failed, выделить AcquireLocalInput/ReleaseLocalInput, если это убирает дублирование. Не заменять независимые server/client состояния одним enum и не возвращать общий HFSM rollback.
- **G3 — preload:** отдельный пакет только при необходимости; загрузка и cook-доступность weapon refs проверяются отдельно от equip lifecycle. Не считать каждый LoadSynchronous повторным чтением диска.
- **G4 — модульная одежда:** после базового Equipment реализовать modular skeletal presentation и подключить сапоги в Feet и футболку в Torso через конфигурацию, definitions и общую equip ability. Приёмка: одновременное ношение, замена только своей записи, следование ALS-позе, независимость от Hands/Back, owner/proxy/late join. Новые характеристики/armor damage, слои одежды и многослотовые предметы — отдельные задачи.

## Порядок и окончание прохода

Для Equipment: статическая актуализация baseline/контракта → 3 → 4 → 5 → 6 → F.
Runtime baseline и адресная приёмка по этапу 0 остаются нужны до заявления о сохранённом поведении;
их запуск требует отдельного разрешения. Шаги 1–2 и G выполняются отдельно и не блокируют
проектирование Equipment; дефекты Inventory, препятствующие владению/переносу, устраняются адресно.

Equipment-проход завершён, когда Equipment — единственный владелец активных записей/grants,
Combat исполняет только оружейную механику, способности и presentation используют проверенный контекст,
оружие и джетпак сосуществуют, а теги/effects/startup abilities расширяются через данные и свои GAS-классы.
Узкий Inventory API остаётся отдельным результатом шагов 1–2. Сокращение числа строк не является критерием.

Для пакетов 2 и 4–6 нужны отдельно согласованные адресные проверки из исходной матрицы: обычная сборка, BP/assets migration, два клиента и сервер, смена Pawn/late events и initial replication. До их фактического выполнения в статусе остаётся «реализовано, runtime не подтверждён».
