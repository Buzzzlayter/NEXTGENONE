# Ballistic weapons and ammunition

Implemented 2026-11-01. No gameplay tests executed; runtime/network results remain unverified.

## Weapon path

GA_Fire remains ServerOnly. The ability owns required/blocked state tags and commits a GAS cooldown effect (`Cooldown.Combat.Fire`) with the weapon's FireInterval. The effect lives on the PlayerState ASC and survives ability reactivation, unequip and weapon changes; there is no separate NextFireTime timer in ASC or Combat. Empty magazines and invalid definitions are rejected before commit. A committed attempt that later fails geometric/spawn validation keeps its cooldown and consumes no ammunition. Combat validates the current Pawn/ASC, equipment/item membership, ammo and finite weapon parameters. The server uses an eye trace to choose an initial aim point, checks its stable character-relative launch anchor against walls, prepares all pellets, then consumes one magazine round. That aiming trace does not deal damage. Physical projectile collision is the only ranged damage path.

BP_BallisticProjectile derives from ANGBallisticProjectile with UNGBallisticMovement (UProjectileMovementComponent subclass). Swept sphere collision and forced substeps integrate initial velocity, gravity and linear drag. Gravity direction is sampled from CharacterMovement on launch; gravity magnitude uses world gravity times weapon GravityScale. Velocity/drop/drag are simulated server-side; replicated movement plus cosmetic client integration shows the projectile. The projectile ignores its shooter and uses a separate BallisticProjectile object channel so projectiles ignore each other and world pickups remain non-blocking.

On first authoritative collision a projectile stops, disables collision, applies the existing GAS damage effect through ApplyDamageToPawn, emits an impact cosmetic event, and expires. Old corpse/ASC validation remains in the shared damage helper. The shooter ASC AvatarGeneration is captured (incremented internally for each new server Avatar): a projectile cannot attribute damage through a different shooter life after respawn. Cosmetic clients never apply damage. Max travel distance and lifespan bound the projectile; there is no penetration, ricochet, lag compensation, damage falloff, wind or dynamic gravity-volume integration. The current tracer is a simple replaceable mesh. Impact FX subscribe to Combat.OnProjectileImpact; shot audio and existing fire animations use OnShotCosmetic's path.

WeaponDefinition fields: ProjectileClass, MuzzleSpeed (cm/s), GravityScale, LinearDrag (1/s), PelletsPerShot (1..16), SpreadDegrees (cone half-angle), Damage per pellet, Range, AmmoItemClass and FireSound. FireInterval remains persistent server-limited. Server launch anchor is independent of dedicated-server animation tick; character fire/reload montage retargeting onto ALS remains manual.

## Ammunition

/Game/Gameplay/Inventory/ID_Ammo_Pistol, ID_Ammo_Rifle, ID_Ammo_Shotgun are separate inventory definitions. StackCount is the number of rounds/shells, MaxStackSize 500. Place BP_ItemPickup and choose one of these ItemClasses to put ammo into the world. Existing pickup/drop logic handles these stacks.

Magazine ammo remains a stat on each weapon instance. Reserve is now the count of matching ammo items in the same existing PlayerState inventory. New weapon fragments no longer grant reserve stat stacks. Legacy reserve stats on previously existing instances are ignored, not duplicated into inventory. No persistent save-game migration is implemented. TestWeapon uses pistol ammo. CompleteReload consumes available matching ammo stacks quietly, fills the still-current weapon magazine and then notifies observers; a reload guard prevents recursive completion. Incompatible ammo is never consumed. Inventory UI displays magazine / matching bag ammo.

Starter guns and their initial reserve items are granted once per PlayerState, not replenished on every Pawn/bind. The existing debug F equip fallback is still a development tool. Selection 1/2/3 filters weapon items so ammo stacks do not displace gun selection.

## Imported weapon assets

The existing pistol/rifle are retained. Added DA_Shotgun and ID_Shotgun using imported shotgun mesh, skeleton/physics asset, material/textures and weapon fire/reload sequences. Default shotgun: eight pellets, nine damage per pellet, 4-degree cone, 350 m/s, eight-shell magazine. Pistol and rifle use individual projectile speeds and matching ammo definitions. Imported individual shot SoundWaves for all three guns. These are native adaptations, not copies of an external damage/inventory framework or its main-menu graph. No ALS plugin/AnimBP edits.

## Build and assets

Editor C++ build: PASS, no compiler diagnostics. NGSetup -BallisticsOnly: exit 0, 0 errors / 14 ALS/environment warnings. Logs: Saved/Logs/NGBallisticsBuild.log and NGBallisticsAssets.log. Existing authored values and unrelated fragments are preserved; modified assets were backed up under Saved/RefactorBackup/Interaction. The import manifest is Saved/Logs/BallisticImportedAssets.json. No dedicated binary, game package or gameplay session was built/run.

## Files

New: Gameplay/NGBallisticProjectile.h/.cpp, NGConnectionSubsystem.h/.cpp, NGConnectionPanel.h/.cpp (under Source/NEXTGENONE); Source/NEXTGENONEServer.Target.cs; Scripts/StartDedicatedServer.ps1, StartDedicatedServer.sh, PackageDedicatedServer.ps1; Config/Steam.example.ini; this document and Docs/Networking/DedicatedServer.md.

Updated: Gameplay/NGCombat.h/.cpp, NextgenonePlayerState.h, NextgenonePlayerController.h/.cpp, NGSettingsPanel.h/.cpp, NGSetupCommandlet.cpp, Inventory/NGInventoryPanel.cpp; Config/DefaultEngine.ini; architecture, multiplayer testing documentation.

New project assets: Gameplay/Weapons/BP_BallisticProjectile, DA_Shotgun; Gameplay/Inventory/ID_Shotgun, ID_Ammo_Pistol, ID_Ammo_Rifle, ID_Ammo_Shotgun; Gameplay/UI/WBP_ServerConnection. Updated existing weapon definitions, Pistol/Rifle item fragments, Character, settings/inventory UI and fire/reload abilities. Added 11 selected shotgun assets plus three SoundWaves (see manifest). Main map and project GameMode defaults were not switched.
# Прицеливание от камеры (2026-11-04)

## Удаление встроенного тестового оружия

Character больше не создаёт TestWeapon и не загружает куб. Поле Combat.TestWeapon и неиспользуемый TryPickupOnServer удалены. SetEquippedOnServer не создаёт item: сначала выбрать имеющийся предмет через слот/RequestEquipItem, затем экипировать. При пустом инвентаре персонаж остаётся безоружным.

Нативный StarterItems теперь пуст; стартовые предметы задаются явно на Combat в Pawn Blueprint. Сохранённые значения существующих Blueprint агент не менял. WeaponDefinition.InventoryItemClass назначается явно для compatibility WeaponPickup; универсальный ItemPickup уже использует ItemClass. FireAbilityClass/ReloadAbilityClass больше не инициализируются /Game-путями; при пустых полях используются UNGFireAbility/UNGReloadAbility. Для собственных BP-способностей назначить классы в WeaponDefinition. ProjectileClass/AimAbilityClass сохраняют native defaults. Старые тестовые Content assets в этом пакете не удалялись. Blueprint настроить/пересохранить пользователю; сборку и игровые тесты агент не запускал.

## Прицеливание через GAS

`UNGAimAbility` — LocalPredicted equipment ability. Оружие выдаёт её по `WeaponDefinition.AimAbilityClass` (по умолчанию native UNGAimAbility) с Input.Aim, снимает вместе с Fire/Reload по своим handles. Не добавлять второй экземпляр в Alive. SourceObject — существующий CombatComponent; проверяется текущий ASC Avatar и экипировка.

На активации CommitAbility и Infinite `UNGAimingEffect` дают State.Weapon.Aiming. Effect нужен для репликации состояния также simulated proxies, независимо от настройки репликации ActivationOwnedTags. Отпускание через WaitInputRelease завершает способность и снимает только её effect handle. Снятие оружия/UnPossess/смерть отменяют её существующим lifecycle; появление blocked tags Dash/Mantle/Reload/Dead завершает активный ADS. После отмены нужно отпустить и нажать ввод заново. При смене Avatar Combat отписывается от старого ASC.

`Combat.IsAiming` читает GAS-тег только для текущего Avatar и экипировки. OnAimChanged/ALS presentation обновляются от tag event. Отдельные replicated bAiming и ServerSetAiming удалены. `RequestAim(bool)` и Character.AimAction оставлены только как input→GAS переходники, без собственного состояния или сетевого протокола.

Blueprint-сетап пользователя: в CharacterDefinition.InputConfig перенести IA прицеливания из NativeInputActions в AbilityInputActions с тегом Input.Aim. IMC остаётся у ControlStateDefinition; клавиши/геймпад задаются пользователем. Если оставить прежний Native маршрут, он тоже вызывает GAS; одновременно оба маршрута не назначать. Удалить старые Blueprint-вызовы ServerSetAiming, если существовали. При желании создать BP от UNGAimAbility и указать его в WeaponDefinition.AimAbilityClass, сохранив native lifecycle.

Fire остаётся ServerOnly со свежим лучом в ServerRequestFire, серверным снарядом/уроном/патронами/rate limit. ADS не является условием выстрела. Разброс и текущая точность не изменены: возможность точного попадания от бедра пока сохраняется по явному решению пользователя. Сборка и runtime/сетевая работа нового GAS lifecycle не проверены; Blueprint и input assets агент не менял.

Сервер самостоятельно проверяет направление выстрела и создаёт физический снаряд; клиентский TargetData не определяет попадание и урон.

`NGAimComponent.GetTargetingOrigin` теперь одинаково используется локальным прицеливанием и серверным выстрелом: переносит начало вдоль того же луча вперёд до Pawn, сохраняя экранное направление. Валидация камеры дополнительно делает sphere sweep с origin/radius/channel исходного `AALSBaseCharacter.GetThirdPersonTraceParams`; исходный eye-to-camera trace также сохранён. ALS plugin и Camera AnimBP не изменены. Это проверка столкновений и границ доверия, а не точное серверное восстановление камеры: pivot/rotation lag и offsets исходной камеры зависят от AnimBP-кривых, плечо в исходном ALS локальное. Произвольная камера внутри разрешённого объёма всё ещё возможна. Для более строгой политики нужен общий серверно-доступный профиль камеры с синхронизированным выбором режима/плеча, а не запуск Camera AnimBP на dedicated server.

Новые sweep-проверки могут отклонять выстрел у вплотную прижатой к стене камеры; сетевой допуск и реальные параметры необходимо проверить отдельно. Сборка и тесты не запускались.

Выстрел игрока проходит через `Combat.RequestFire` (штатный Input.Combat.Primary уже вызывает его). `NGAimComponent.GetViewRay` даёт луч центра камеры, тот же источник используется для локального прицеливания. Клиент отправляет серверу origin/direction и revision экипировки, не Actor/попадание/урон.

Сервер проверяет текущий ASC Avatar, смерть, экипировку, конечность/нормализацию луча, расстояние origin от глаз, угол относительно ControlRotation и отсутствие стены между глазами и origin. Лимиты на AimComponent: MaxViewOriginDistance=600 см, MaxViewAngleDifference=35 градусов; для нестандартных камер их нужно настроить в Blueprint. Это ограничение доверия клиенту, не полная защита от подмены камеры внутри допустимой области.

После проверки сервер синхронно активирует существующую ServerOnly GA_Fire. Луч хранится только на время этой активации и потребляется одним FireOnServer. Для удалённого игрока прямая активация GA_Fire без свежего запроса не стреляет; Blueprint не должен добавлять задержку до FireOnServer. Для AI и локального authority оставлен расчёт текущего луча без сетевого запроса.

Сервер трассирует камеру, выбирает первую точку фокуса и направляет снаряд из существующего серверного якоря к ней. Позиция якоря пока прежняя (от капсулы, не socket ствола). Проверка препятствия до якоря сохранена, дальнейшие столкновения вычисляет снаряд. Цель позади якоря отклоняется. Разброс/гравитация остаются; идеального совпадения попадания с точкой на любых расстояниях нет. Rate limit по PlayerState не сбрасывается при активации/смене оружия.

UMG-плейсхолдер точки должен быть в центре игровой области. Сетевые задержки, смена плеча, ADS, стены у камеры/ствола и split-screen ещё не проверены. Сборку выполняет пользователь; агент не запускал сборку или игровые тесты.
