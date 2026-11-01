# Ballistic weapons and ammunition

Implemented 2026-11-01. No gameplay tests executed; runtime/network results remain unverified.

## Weapon path

GA_Fire remains ServerOnly. Combat validates the current Pawn/ASC, equipment/item membership, action tags, ammo and finite weapon parameters. The persistent PlayerState fire limiter is shared across reactivation, equip and weapon changes. The server uses an eye trace to choose an initial aim point, checks its stable character-relative launch anchor against walls, prepares all pellets, then consumes one magazine round. That aiming trace does not deal damage. Physical projectile collision is the only ranged damage path.

BP_BallisticProjectile derives from ANGBallisticProjectile with UNGBallisticMovement (UProjectileMovementComponent subclass). Swept sphere collision and forced substeps integrate initial velocity, gravity and linear drag. Gravity direction is sampled from CharacterMovement on launch; gravity magnitude uses world gravity times weapon GravityScale. Velocity/drop/drag are simulated server-side; replicated movement plus cosmetic client integration shows the projectile. The projectile ignores its shooter and uses a separate BallisticProjectile object channel so projectiles ignore each other and world pickups remain non-blocking.

On first authoritative collision a projectile stops, disables collision, applies the existing GAS damage effect through ApplyDamageToPawn, emits an impact cosmetic event, and expires. Old corpse/ASC validation remains in the shared damage helper. Shooter PlayerState LifeSerial is captured: a projectile cannot attribute damage through a different shooter life after respawn. Cosmetic clients never apply damage. Max travel distance and lifespan bound the projectile; there is no penetration, ricochet, lag compensation, damage falloff, wind or dynamic gravity-volume integration. The current tracer is a simple replaceable mesh. Impact FX subscribe to Combat.OnProjectileImpact; shot audio and existing fire animations use OnShotCosmetic's path.

WeaponDefinition fields: ProjectileClass, MuzzleSpeed (cm/s), GravityScale, LinearDrag (1/s), PelletsPerShot (1..16), SpreadDegrees (cone half-angle), Damage per pellet, Range, AmmoItemClass and FireSound. FireInterval remains persistent server-limited. Server launch anchor is independent of dedicated-server animation tick; character fire/reload montage retargeting onto ALS remains manual.

## Ammunition

/Game/Gameplay/Inventory/ID_Ammo_Pistol, ID_Ammo_Rifle, ID_Ammo_Shotgun are separate inventory definitions. StackCount is the number of rounds/shells, MaxStackSize 500. Place BP_ItemPickup and choose one of these ItemClasses to put ammo into the world. Existing pickup/drop logic handles these stacks.

Magazine ammo remains a stat on each weapon instance. Reserve is now the count of matching ammo items in the same existing PlayerState inventory. New weapon fragments no longer grant reserve stat stacks. Legacy reserve stats on previously existing instances are ignored, not duplicated into inventory. No persistent save-game migration is implemented. TestWeapon uses pistol ammo. CompleteReload consumes available matching ammo stacks quietly, fills the still-current weapon magazine and then notifies observers; a reload guard prevents recursive completion. Incompatible ammo is never consumed. Inventory UI displays magazine / matching bag ammo.

Starter guns and their initial reserve items are granted once per PlayerState, not replenished on every Pawn/bind. The existing debug F equip fallback is still a development tool. Selection 1/2/3 filters weapon items so ammo stacks do not displace gun selection.

## Lyra assets

The existing pistol/rifle are retained. Added DA_Shotgun and ID_Shotgun using actual local Lyra Shotgun mesh, skeleton/physics asset, material/textures and weapon fire/reload sequences. Default shotgun: eight pellets, nine damage per pellet, 4-degree cone, 350 m/s, eight-shell magazine. Pistol and rifle use individual projectile speeds and matching ammo definitions. Imported individual Lyra shot SoundWaves for all three guns. These are native adaptations, not copies of Lyra's damage/inventory framework or its main-menu graph. No ALS plugin/AnimBP edits.

## Build and assets

Editor C++ build: PASS, no compiler diagnostics. NGSetup -BallisticsOnly: exit 0, 0 errors / 14 ALS/environment warnings. Logs: Saved/Logs/NGBallisticsBuild.log and NGBallisticsAssets.log. Existing authored values and unrelated fragments are preserved; modified assets were backed up under Saved/RefactorBackup/Interaction. The import manifest is Saved/Logs/BallisticImportedAssets.json. No dedicated binary, game package or gameplay session was built/run.

## Files

New: Gameplay/NGBallisticProjectile.h/.cpp, NGConnectionSubsystem.h/.cpp, NGConnectionPanel.h/.cpp (under Source/NEXTGENONE); Source/NEXTGENONEServer.Target.cs; Scripts/StartDedicatedServer.ps1, StartDedicatedServer.sh, PackageDedicatedServer.ps1; Config/Steam.example.ini; this document and Docs/Networking/DedicatedServer.md.

Updated: Gameplay/NGCombat.h/.cpp, NextgenonePlayerState.h, NextgenonePlayerController.h/.cpp, NGSettingsPanel.h/.cpp, NGSetupCommandlet.cpp, Inventory/NGInventoryPanel.cpp; Config/DefaultEngine.ini; architecture, multiplayer testing and LyraPort docs.

New project assets: Gameplay/Weapons/BP_BallisticProjectile, DA_Shotgun; Gameplay/Inventory/ID_Shotgun, ID_Ammo_Pistol, ID_Ammo_Rifle, ID_Ammo_Shotgun; Gameplay/UI/WBP_ServerConnection. Updated existing weapon definitions, Pistol/Rifle item fragments, Character, settings/inventory UI and fire/reload abilities. Added 11 selected Lyra Shotgun assets plus three SoundWaves (see manifest). Main map and project GameMode defaults were not switched.