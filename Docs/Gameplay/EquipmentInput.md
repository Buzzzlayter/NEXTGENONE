# Боевой ввод и Action Bar через GAS

2026-11-10: C++ Equipment/Combat-проход и процедура миграции подготовлены; сборка,
авторинг ассетов и runtime-приёмка не выполнялись. Текущий контракт и оставшиеся шаги —
[Equipment.md](Equipment.md).

Input-компонент обрабатывает ALS-движение и отправляет press/release тегов в ASC. Зависимостей на Combat и Inventory в нём нет. Конкретные команды исполняют способности.

## Назначено через Editor MCP — 2026-11-07

Созданы GA_PrimaryAttack, GA_Equip/GA_Unequip и GA_ActionSlot1..5. Для Equip/Unequip заданы true/false, для слотов индексы 0..4. В DA_CharacterState_Alive добавлены восемь grants без ActivateOnEnter; прежние семь grants и оба IMC сохранены. Девять IA перенесены из NativeInputActions в AbilityInputActions без изменения клавиш. В ID_Pistol/ID_Rifle/ID_Shotgun добавлен Item Actions с NGEquipWeaponItemAbility; прежние fragments сохранены. У ID_Medkit назначены Item Actions и Medical Item (50 HP, 3 секунды). WBP_InventorySlot уже имел LeftMouseButton для drag и RightMouseButton для secondary action.

Двенадцать затронутых Blueprint прошли compile_blueprint с warnings_as_errors=true; все 14 изменённых ассетов сохранены. Индексы пяти Action-слотов и LocalOnly подтверждены обратным чтением после сохранения. Финальная повторная проверка остальных назначений прервана штатным закрытием Editor; результаты отдельных set_properties и save_assets успешны. UBT/Live Coding, PIE и сетевые тесты не запускались. Backup прежних шести ассетов: Saved/EditorSetupBackup/20261107-153919/.

## Настройка после сборки пользователем

В Character InputConfig перенести соответствующие IA из NativeInputActions в AbilityInputActions. Не оставлять одну IA в обоих массивах. IMC и клавиши менять не требуется.

| Действие | InputTag | Способность и источник выдачи |
|---|---|---|
| Основная атака | `Input.Combat.Primary` | `UNGPrimaryAttackAbility`, Alive |
| Прицеливание | `Input.Aim` | Существующая AimAbility оружия, выдаёт Combat |
| Перезарядка | `Input.Combat.Reload` | Существующая ReloadAbility оружия, выдаёт Combat |
| Экипировать первое сохранённое оружие в пустой Weapon | `Input.Equipment.Equip` | BP от `UNGEquipmentControlAbility`, `bEquip=true`, Alive |
| Убрать оружие | `Input.Equipment.Unequip` | Другой BP от `UNGEquipmentControlAbility`, `bEquip=false`, Alive |
| Action 1–5 | `Input.ActionBar.Slot1` … `Slot5` | Пять BP от `UNGActivateActionSlotAbility`, `ActionSlot=0` … `4`, Alive |

Для каждой строки Alive добавить FNGAbilityGrant с указанным InputTag; bActivateOnEnter выключен. Aim/Reload/Fire повторно в Alive не выдавать. Старый маршрут перезарядки `Input.Equipment.Reload` заменить на `Input.Combat.Reload`.

NativeInputActions временно поддерживают общий проброс неизвестных ALS тегов в GAS. Он не выбирает предметы и не вызывает боевые компоненты. Предпочтительная настройка команд — AbilityInputActions.

## Исполнение

- Secondary action панели отправляет только предмет и inventory source через player RPC.
  Серверный обработчик PlayerInventory выбирает по definition слот типизированной
  экипировки (Weapon/Back/Feet/Torso) и выполняет существующий transfer. UI не знает Equipment.
  Сервер проверяет доступ к контейнеру, снимки слотов и equipment preflight; занятый
  несовместимым предметом слот не заменяется между контейнерами. Это не удалённое
  использование расходников. Оружие переносится в Weapon так же, как джетпак в Back. Для предмета
  уже в инвентаре игрока сохраняется прежняя ItemActions ability.

- PrimaryAttack — LocalOnly-способность, которая повторяет выстрел через стандартную `UAbilityTask_WaitDelay` и ждёт отпускания через `WaitInputRelease`. Первый запрос отправляет сразу; для автоматического оружия повторяет Combat.RequestFire с интервалом из WeaponDefinition (минимум 0,05 с). GAS завершает tasks вместе с ability; EndAbility снимает подписку Equipment. Отдельного TimerHandle нет. Без актуального оружия способность завершается.
- Ability подписана на Equipment.OnEquipmentChanged: смена HandsRevision или потеря актуальности установки сразу завершает удержание. Изменение только Back не прерывает Hands. Перед очередным запросом проверяется текущий Avatar и установка; из синхронного callback нельзя продолжить очередь с новым оружием.
- Combat сохраняет подготовку клиентского луча и server-authoritative выстрел через ServerOnly GA_Fire. IA не должна запускать `Input.Combat.Fire` напрямую: это внутренний серверный маршрут после проверки луча.
- EquipmentControl и ActivateActionSlot работают LocalOnly как команды владельца. Они используют существующие RPC с серверными проверками; локальная активация не подтверждает успешную серверную операцию.
- ActivateActionSlot обращается к Inventory, который выбирает предмет и активирует его единственную ActivationAbility. В способности слота нет зависимости на Combat.
- Смена слота и команда экипировки отменяют PrimaryAttack через общий asset-тег `Ability.Type.Weapon.PrimaryInput`.
- UI-блокировка и ShutdownInput отправляют release через существующий input lifecycle и UPlayerInputSubsystem.

После отдельного разрешения проверить удержание/отпускание атаки, автоматический огонь, UI-блокировку, смену слота, перезарядку и смерть во время удержания. Blueprint-компиляция не подтверждает поведение в игре.
