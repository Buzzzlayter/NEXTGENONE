# Открытые технические проверки

Выполненные пункты и отчёты удалены. Этот список не подтверждает актуальную
runtime-готовность и не разрешает запуск проверок.

- [ ] Пользовательские сборки Editor/Game/Server; отдельно разрешённые Blueprint compilation, Data Validation и cook definitions.
- [ ] Инвентарь: split/merge/drop/pickup, перенос и swap между игроком и коробкой, холодный старт Editor, отказ при изменении snapshot.
- [ ] Lifecycle: reload cancel, повторные Enter/Exit UI, ошибка входа Character FSM, смерть и respawn.
- [ ] Dedicated server + два клиента: ASC owner/avatar, grants без дубликатов, новая жизнь и старый труп, ALS roll/get-up, late join/relevancy.
- [ ] Отдельно согласовать latency/loss и нагрузочные прогоны.

Оружие: [порядок доработок и критерии](Gameplay/WeaponTODO.md).
Исходная спецификация консолидации и её план остаются в
[TechnicalConsolidation.md](Architecture/TechnicalConsolidation.md) и
[TechnicalConsolidationPlan.md](Architecture/TechnicalConsolidationPlan.md).
