# Перенос Lyra inventory и оружия — 2026-11-01

Источник: локальный `F:/UNREAL/Projects/LyraStarterGame`, EngineAssociation `5.4`. Цель: NEXTGENONE, UE 5.8.3. Исходный проект Lyra не изменялся. Copyright Epic сохранён в перенесённых исходниках.

## Что фактически перенесено

| Источник Lyra | Код NEXTGENONE |
|---|---|
| System/GameplayTagStack.h/.cpp | Gameplay/Inventory/NGGameplayTagStack.h/.cpp |
| Inventory/LyraInventoryItemDefinition.h/.cpp | Gameplay/Inventory/NGInventoryItemDefinition.h/.cpp |
| Inventory/LyraInventoryItemInstance.h/.cpp | Gameplay/Inventory/NGInventoryItemInstance.h/.cpp |
| Inventory/LyraInventoryManagerComponent.h/.cpp | Gameplay/Inventory/NGInventoryManagerComponent.h/.cpp |
| Inventory/InventoryFragment_SetStats.h/.cpp | Gameplay/Inventory/NGInventoryFragment_SetStats.h/.cpp |
| Equipment/LyraGameplayAbility_FromEquipment.h/.cpp | Gameplay/NGGameplayAbilityFromEquipment.h/.cpp |

Сохранены Fast Array-модель, definition/instance/fragments, stat tag stacks, equipment ability SourceObject и выдача/снятие по handles. Владельцем inventory выбран существующий NextgenonePlayerState; CombatComponent остаётся единственным владельцем текущей экипировки, его SourceObject используется GAS-способностями. Собственный ASC/HealthSet на PlayerState и уже реализованный owner/avatar lifecycle сохранены. Второй ASC или LyraCharacter не создавались.

Перенос исправляет незавершённые места оригинала: AddEntry(instance) реализован для экземпляров своего владельца; проверяются authority, ёмкость и stack limit; total учитывает StackCount; consume сначала проверяет наличие всего количества и затем уменьшает stacks. Нет частичного расходования при нехватке. Количество предметов и их subobjects реплицируется только владельцу. Registered-subobject path также использует COND_OwnerOnly; текущая проектная проверка сборки относится к Generic replication, не к Iris.

GameplayMessageSubsystem заменён существующему UI удобными BlueprintAssignable delegates. Добавлены уведомления о stat changes, позднем разрешении item references и ItemDef. UI получает начальный snapshot и последующие изменения. Native игрок-клиент не может изменить stat stacks через BlueprintAuthorityOnly функцию: добавлены runtime authority guards.

## Оружейный контент

Перенесены 42 `.uasset` (около 118 MiB) в исходных package paths `/Game/Weapons/Generic`, `/Game/Weapons/Pistol`, `/Game/Weapons/Rifle`, плюс texture dependency `/Game/Weapons/Spawnpad/Texture/T_LaunchPadShape`. Это реальные меши, skeleton/physics assets, материалы, текстуры и weapon fire/reload sequences/montages из Lyra. Существующие файлы не перезаписывались. Asset Registry использован для замыкания зависимостей; исходные package paths сохранены.

Созданы проектные assets:

- `/Game/Gameplay/Weapons/DA_Pistol`, `DA_Rifle`;
- `/Game/Gameplay/Inventory/ID_Pistol`, `ID_Rifle`, `ID_TestWeapon`;
- `/Game/Gameplay/Abilities/GA_Reload`;
- `/Game/Gameplay/UI/WBP_InventoryPanel`.

Существующие GA_Fire, WeaponDefinition, pickup actor и CombatComponent адаптированы к item instances. Поле MagazineAmmo в CombatComponent удалено; единственный runtime источник магазина/запаса — stat tags конкретного предмета. Переключение и unequip не восстанавливают патроны. PlayerState NextAllowedFireTime остаётся общим серверным limiter и не сбрасывается при переэкипировке. Reload — ServerOnly GAS ability, запоминает исходный item/Combat и при завершении повторно проверяет текущий avatar и предмет; при смене/смерти weapon grant снимается вместе с активной reload ability.

Стрельба сохраняет существующий серверный hitscan и damage GE с защитой old-Pawn/new-life. Оригинальные Lyra character-dependent fire Blueprint, camera stack, experience/game-feature framework и CommonUI не импортировались в качестве параллельной игровой цепочки. Это адаптация используемых частей к нашей GAS/ALS, а не запуск LyraGame внутри NEXTGENONE. Полная система spread/heat, Lyra client target-data prediction/rewind и все оружейные классы sample не заявлены как перенесённые.

## ALS и управление

| Кнопка | Поведение |
|---|---|
| 1 | Первый предмет стартового набора — пистолет |
| 2 | Второй предмет стартового набора — автомат |
| ПКМ | Удержание серверного aim state и ALS Aiming |
| ЛКМ | Пистолет — один выстрел; автомат — повторные GAS activation requests при удержании; без оружия — Punch |
| R | Перезарядка из запаса конкретного item |
| U | Убрать оружие, сохранив предмет и боезапас |
| F | Старый debug equip тестового оружия; при наличии CurrentItem экипирует его |
| I / Esc | Открыть/закрыть inventory / закрыть открытое окно |

StarterItems на CombatComponent выдаёт отсутствующие стартовые определения при связывании нового Pawn с inventory; существующие экземпляры повторно не создаются. Это текущая прототипная respawn/loadout policy, а не окончательное правило потери предметов в Extraction.

Для поз используются существующие ALS Rifle/PistolTwoHanded overlays, aim sweeps и тот же AnimBP. Меш Lyra размещается через существующий ALS HeldObjectRoot, дополнительного weapon visual actor/mesh слоя нет. WeaponFireAnimation/WeaponReloadAnimation проигрывают механизмы самого оружия. CharacterFireMontage/CharacterReloadMontage оставлены настраиваемыми: персонажные монтажи Lyra нельзя объявлять совместимыми с ALS без проверки skeleton/retargeting. Подходящие ретаргетированные монтажи нужно назначить в DA_Pistol/DA_Rifle; исходный ALS plugin/AnimBP не редактировался.

## Инвентарь и существующая HFSM

Добавлены UNGInventoryPanel, UNGInventoryStateComponent и вспомогательный UIWidgetMounter subclass. В существующий FSMState_GameRuntime добавлен один instanced Inventory Window component. Остальные компоненты/графы сохранены, backup: `Saved/RefactorBackup/InventoryUI/FSMState_GameRuntime_20261101_165459.uasset`.

I испускает InventoryUIRequested на owning controller. Компонент активного Runtime-state слушает его, использует существующий UIWidgetMounter для WBP_InventoryPanel, освобождает window/input/listeners при выходе из состояния или отмене входа. Пока отдельного inventory placeholder нет, применяется штатный owning-player fallback монтера с ZOrder 50; health placeholder не занят инвентарём. Input mode переключается через существующий PlayerInputSubsystem, предыдущий режим/курсор восстанавливаются. Движение/камера и основные gameplay actions блокируются локально на время окна. Это локальный UI flow; авторитетные ограничения матча должны отдельно задаваться сервером.

Панель содержит простые кнопки предметов, stack count и magazine/reserve, обновляется по inventory/stat delegates. Кнопка посылает item reference, а сервер проверяет принадлежность предмета inventory текущего PlayerState. Пользовательский дизайнер может добавить VerticalBox с именем ItemsList или менять производный WBP. Отдельного CommonUI/menu stack не добавлено. OnInventoryRequested остаётся Blueprint hook для последующего авторского GCFSM flow.

## Результат и границы проверки

- Полная сборка NEXTGENONEEditor Win64 Development выполнена через UBT; финальный вывод — `Saved/Logs/NGLyraPortBuild.log`.
- Создание предметов/оружия/UI и компиляция изменённого FSM-state: NGSetup -LyraWeapons, exit 0, 0 errors / 10 warnings окружения и ALS Python names, `Saved/Logs/NGLyraWeaponAssets.log`.
- Игровые/сетевые/автоматические тесты агентом не запускались; требуется отдельный апрув. Editor MCP при закрытом редакторе не использовался, ассеты обработаны через Unreal commandlet.

Непроверены фактические network ownership/subobject arrival, UI focus/close, ammo preservation, automatic fire, reload cancellation, pose/mesh alignment и визуальная совместимость с ALS. Сундуки/перенос между владельцами, drop, split/merge UI, рюкзаки/подсумки, сохранение, Extraction и произвольная gravity пока не реализованы. AddItemInstance допускает только тот же owner: межвладельческий transfer требует явной операции копирования/переноса состояния, а не смены Outer уже реплицируемого UObject. Общая FSM квестов/миссии не переносилась.

## Изменённые файлы

Новые: шесть пар портированных `.h/.cpp` из таблицы, `Gameplay/Inventory/NGInventoryPanel.h/.cpp`, `NGInventoryStateComponent.h/.cpp`, семь проектных assets из списка и 42 resource assets в Content/Weapons.

Изменены: `NGCombat.h/.cpp`, `NGAbilitySystem.h/.cpp`, `NextgenoneCharacter.h/.cpp`, `NextgenonePlayerState.h/.cpp`, `NextgenonePlayerController.h/.cpp`, `NGWeaponPickup.cpp`, `NGSetupCommandlet.cpp`, `NEXTGENONE.Build.cs`, `Content/Blueprints/States/FSMState_GameRuntime.uasset`; обновлены AGENTS.md, GameplayFoundation.md, MultiplayerCombat.md. Source/UIModule, оригинальный ALS plugin, основная карта и project defaults не изменялись.

## Ballistic / Shotgun pass — 2026-11-01

Added eleven selected Lyra Shotgun assets (mesh, skeleton/physics, material, four textures and mechanical fire/reload sequences) and three individual weapon shot SoundWaves. Preserved original /Game package paths; dependency closure was resolved by the existing import helper. Initial copied-file manifest: Saved/Logs/BallisticImportedAssets.json. Project-native definitions/abilities integrate this art into the same ALS/GAS/inventory path. Ballistic projectile code is native NEXTGENONE code, not a claim that Lyra's hitscan code was copied as physical ammunition.

Reviewed CommonUser/Private/CommonSessionSubsystem.cpp and official Common Session docs for request/status separation. Implemented a direct dedicated-server connection subsystem/widget using current UMG/HFSM, without importing CommonUser/CommonUI/Experience/EOS dependencies or Lyra's main-menu assets. Steam was explicitly requested as a placeholder with no App ID. No Steam functionality or online session runtime success is claimed.
## Weapon Niagara port — 2026-11-01

82 selected effect assets were copied, including seven systems for muzzle flash, shell ejection, regular/shotgun tracers, concrete/glass impacts and character sparks. Package paths preserved; manifest: Docs/Research/LyraWeaponVFXAssets.txt. Native cosmetic adapters drive their inspected parameter contracts from existing shot/projectile-impact hooks. No Lyra Blueprint gameplay framework or source Lyra files were changed. Niagara is the only newly required runtime module/plugin. Visual/runtime success remains unverified; see Docs/Gameplay/LyraWeaponVFX.md for headless compilation status and remaining LWC diagnostics.