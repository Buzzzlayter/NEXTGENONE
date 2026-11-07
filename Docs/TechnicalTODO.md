# TODO: консолидация текущего функционала

Ревью: 2026-11-06. Основа: main `4895ed5`; отдельно проверена ветка
`fix/als-gas-authority`, HEAD `6692a28668ed27579a01ba223a91ca6a211b2f51`, PR #6.
Первоначальное ревью не меняло код. Реализация пунктов — 2026-11-07; [x] означает изменение кода
и статический просмотр, а не подтверждение сборкой/runtime. Сборка, Blueprint compilation, PIE
и сетевые прогоны текущего пакета не выполнялись.

Исходные замечания №1 (null LocalPlayer в HFSMInputComponent) и №2
(опасные клиентские ALS RPC) исключены из активного списка: исправления есть в PR #6.
Это не означает слияние PR или подтверждённую runtime-готовность.

R1/R2 реализованы в текущем пакете: ограниченная очередь последних locomotion-намерений
и общая серверная CanRequestGetUp для Jump/Ragdoll. Runtime-проверка остаётся в TODO.

## P1 — корректность и надёжность

- [x] Единая политика split/merge stateful stacks для TransferTo и DropInventoryItem.
  StatStacks принадлежат instance целиком, не единице Count. Общие CanSplitStack/CanMergeStackWith
  запрещают split и merge при непустых stats; полный перенос/drop разрешён, swap внутри контейнера сохранён.
  Transfer повторно проверяет snapshot stats после инициализации фрагментов. Статический просмотр выполнен;
  сборка и runtime не проверены. Регрессии split/merge ниже остаются открытыми.
- [x] Обработка неуспешного входа ресурсов FSM: ошибки Definition/mappings/GAS grants/entry ability
  сообщаются машине, очищают RequestedStateTag/Event и ресурсы, освобождают барьер Avatar handoff.
  Ошибка терминальна для данного Pawn: IsGameplayReady=false, без автоматического повторного входа;
  логический тег отказавшего перехода сохранён отдельно (в том числе Dead). Новый Pawn начинает независимо.
  Статический просмотр выполнен, сборка/runtime не проверены. Отсутствующий переход в самом Blueprint-графе
  без вызова resource Enter этим обработчиком не обнаруживается; это отдельная проверка настройки графа.

## P2 — инвентарь, взаимодействия и бой

- [x] Общая чистая проверка вместимости для клиента и authority: свободный подходящий слот,
  AcceptedItems, размер стака. UI подбора не должен считать доступным заведомо запрещённый сервером pickup.
- [x] Определить слот назначения при pickup с auto-equip: стандартный первый свободный Storage
  конфликтует с требованием экипировать только из Action. Учесть заполненные Action-слоты.
- [x] Консолидировать внутренние мутации инвентаря: добавление, списание, deferred notifications,
  регистрацию subobjects, reentrancy и завершение транзакции. Использовать те же операции в Reload.
  AddItemInternal/RemoveQuantityInternal и FMutationScope объединяют пути; callbacks не могут
  повторно мутировать участников незавершённой операции. Reload использует общий consume с credit
  после всех debits и до inventory notifications. Pickup использует deferred add в выбранный слот.
- [x] Возвращать причины отказа inventory RPC и идентификатор запроса; различать отправку запроса
  и подтверждённое серверное выполнение. Не менять серверное состояние оптимистически в UI.
  RequestDropItem/RequestInventoryTransfer возвращают ID (0 — не отправлено); native OnRequestCompleted
  сообщает Succeeded/InvalidRequest/StalePawn/RateLimited/AccessDenied/StaleItem/Rejected.
  Completion не гарантирует, что репликация слотов уже пришла; listen-server completion может быть синхронным.
- [x] Объединять уведомления завершённого набора изменений инвентаря; обновлять затронутые UI-слоты,
  сохранять неизменившиеся подписки и иконки. Замерить эффект до заявления о приросте производительности.
  Один broadcast на mutation/replicated receive; слот пропускает одинаковый presentation snapshot,
  иконка перезагружается только при смене item/icon, подписки панели обновляются разницей наборов.
  Панель ещё формирует полный snapshot. Замеров нет, прирост производительности не заявлен.
- [x] Перенести стартовую выдачу из Combat.BindInventory к владельцу инвентаря; определить
  обработку частичного отказа выдачи. Разделить Fire/Reload ability implementations и обязанности Combat
  без добавления универсального equipment framework.
  GrantStarterInventory принадлежит PlayerInventory; частичный результат сохраняется, ошибки логируются,
  повторной неявной выдачи на respawn нет. После R6 используются явные Inventory.InitialContents;
  старые StarterItems сохраняются только для ручной миграции. Fire/Reload вынесены в NGWeaponAbilities.cpp.
- [x] Настраивать класс dropped pickup через существующую конфигурацию вместо LoadClass с /Game-путём;
  валидировать доступность класса при cook.
  Используется hard class reference Interaction.DroppedPickupClass (native default NGItemPickup);
  null/abstract запрещены на runtime-границе. Назначенный Blueprint попадает в cook по ссылке.
  Capacity/pickup используют одну чистую функцию поиска; Action занят → Storage без auto-equip.
  Статический просмотр выполнен, cook/runtime не проверены.
- [x] Консолидировать повторные проверки текущего Pawn/Controller/ASC небольшими функциями.
  Сохранить специализированную серверную валидацию на каждой входной границе.
  NGPlayerContext.GetCurrentASC используется Combat и Interaction; GetInventoryPawn остаётся транспортной
  проверкой PlayerState/Controller, а access/health/state/rate/snapshot проверки сохранены на границах.
- [x] Добавить валидацию item/weapon definitions: размеры стеков, числовые диапазоны,
  обязательные ссылки и согласованность fragment stats. Не полагаться только на ClampMin в Details.
  ValidateDefinition/IsDataValid проверяют runtime-диапазоны и ссылки; Editor дополнительно проверяет
  soft classes, equipment backlink и magazine/reserve stats. Data Validation и cook не запускались.

## P2 — UI и lifecycle

- [x] Безопасный повторный Enter/Exit UI-компонентов: освобождение старых policy/mapping handles,
  подписок и виджетов; очистка Placeholders при смене layout template; диагностика дубликатов ID.
  Input/Mounter/Layout/ResolutionObserver очищают старые ресурсы перед Enter; router хранит Controller,
  у которого подписался. Layout очищает ссылки после Exit, отложенный fade работает со старым widget;
  mounter запоминает фактический placeholder. Дубликаты ID логируются, сохраняется первый.
- [x] Обнулять OutComponent перед GetClosestComponentInHierarchyUp: отсутствие результата
  не должно возвращать старое значение вызывающего кода.

## P2/P3 — плагины, зависимости и проверки

- [x] GCFSMEditor: снимать три зарегистрированных Details class layout в ShutdownModule.
- [x] Обновить Combat smoke harness под выбор предмета, Action inventory, ADS и текущие оружейные
  настройки; убрать зависимость от старой неявной выдачи и фиксированных ammo/damage.
  Выбирает реальный Action item, ждёт GAS ADS и направления камеры, использует RequestFire со свежим
  view; health/ammo рассчитываются из fixture. Persistent limiter проверяется отдельным direct gate probe,
  не невалидным FireOnServer без remote view. Runner требует настроенный GameMode class path.
  Сценарий не запускался; ограничения fixture и ручные шаги — Docs/Testing/MultiplayerCombat.md.

## Дополнительное ревью 2026-11-07

- [x] R1: терминальный отказ реплицируется, закрывает общий Pawn/ASC gate и активацию GAS;
  снимает ввод и останавливает movement. Последующая смерть переводит ошибочный logical state в Dead,
  даже если граф уже недоступен. Нормальная GA_Death при сломанном графе не гарантируется.
- [x] R5: фактические mapping/policy handles проверяются; отказ снимает частично полученные handles
  и передаётся машине. Reacquire уведомляет о готовности после input acquisition.
- [x] R2/R3: smoke ждёт CanFireNow после limiter probe и проверяет доступность gate после hip-fire;
  клиент подтверждает попытку только с экипировкой и без ADS. ADS retry выполняет release/press,
  только если ADS ещё не активен. Прогон не выполнялся.
- [x] R4: per-request callback регистрируется до отправки; Panel получает server result и ID через
  OnInventoryRequestCompleted (Blueprint event для оформления отказов). Поддержан синхронный listen server;
  результат запроса не меняет данные слотов. Runtime не проверен.
- [x] R6: Inventory.InitialContents содержит ItemClass/Count/SlotTag/SlotIndex; нет классификации
  оружия или чтения WeaponDefinition в PlayerInventory. Старые StarterItems не исполняются; значения
  сохранены для ручного переноса. Требуется Blueprint-сетап InitialContents после сборки.
- [x] R7: один dirty-флаг и OnInventoryChanged без аргументов; удалены map сообщений, delta DTO
  и per-entry replication callbacks. Текущие native подписчики переведены на snapshot invalidation.
- [x] R8: Fire/Reload используют ValidateDefinition как единый контракт параметров; повторные
  finite/range/clamp после успешной проверки убраны. Pawn/ADS/ammo/view/cooldown проверки сохранены.

### Проверки, ожидающие отдельного разрешения

- [ ] Сборки Editor/Game/Server и Blueprint compilation; Data Validation/cook новых definitions.
- [ ] Регрессии split/merge/drop/pickup, rollback transfer при изменении snapshot, Reload cancel,
  повторные Enter/Exit UI и отказ входа Character FSM с последующим respawn.
- [ ] Dedicated server + два клиента: обновлённый smoke, затем отдельно latency/loss,
  ALS roll/get-up, late join/relevancy. Ни один из этих прогонов пока не выполнялся.

Порядок: P1 → инварианты инвентаря → UI lifecycle → консолидация → проверки качества.
Сборки и игровые проверки требуют отдельного разрешения по AGENTS.md.
