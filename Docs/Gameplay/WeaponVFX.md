# Weapon particles

Imported Epic sample content in /Game/Effects preserves package paths: 82 assets including seven Niagara systems, emitters, modules, materials, textures, curves and three shell meshes. Original ALS assets and AnimBP were not modified.

## Connected effects

- NS_WeaponFire_MuzzleFlash_Rifle: muzzle flash/smoke for the current weapons.
- NS_WeaponFire_ShellEject: per-weapon pistol/rifle/shotgun shell mesh.
- NS_WeaponFire_Tracer and NS_WeaponFire_Tracer_Shotgun: driven by moving ballistic projectile segments.
- NS_ImpactConcrete: default world impact.
- NS_ImactSparksCharacter: stylized character sparks, not blood.
- NS_ImpactGlass: imported and compiled for manual assignment; automatic physical-surface/glass routing is not implemented.

DA_Pistol, DA_Rifle, DA_Shotgun and DA_TestWeapon receive unset MuzzleFX, ShellEjectFX, TracerFX, ImpactFX, CharacterImpactFX and ShellMesh fields. Existing custom assignments are preserved. MuzzleFXSocket defaults to Muzzle, ShellEjectFXSocket to ShellEject. If the socket is absent, the helper spawns at the server launch anchor. All fields remain editable in the weapon definition.

## Integration

NGWeaponVFX spawns the effects on clients/listen host only; it exits on dedicated server. Shot/impact messages carry the original weapon definition, so an impact in flight does not select FX from a newly equipped weapon. The ballistic projectile replicates its initial TracerFX reference and locally updates User.MuzzlePosition, User.ImpactPositions and the bool User.Trigger using actual movement segments. It does not render the entire intended shot as an immediate hitscan beam. At impact/expiry/EndPlay the tracer deactivates and its tail is cleaned up.

Standalone muzzle/shell/impact instances stop emission after a short window and have bounded cleanup, accommodating the source systems' trigger-driven emitters. Native tracer mesh stays as fallback when no Niagara instance is created. Shot and impact events continue to broadcast existing Blueprint cosmetic delegates. No effect applies damage, changes ammo or starts physical body simulation; existing GAS/ballistics/HitReaction retain those responsibilities.

Parameters were read from loaded Niagara systems, not guessed. Muzzle/shell/tracer trigger uses NiagaraBool. Tracer ImpactPositions is ArrayFloat3 and MuzzlePosition is NiagaraPosition. Impact positions are ArrayPosition, normals ArrayFloat3, NumberOfHits Int32. This distinction is reflected in the setters.

## Verification and limits

Offline C++ build and Niagara/Blueprint asset compilation were executed; no gameplay or rendering test was run. NGSetup -WeaponVFXOnly imports dependencies, compiles systems, records each active script's compile status, rejects Error/Unknown/Dirty status and saves weapon bindings. Disabled emitters are excluded from active-script validation. In particular the disabled glass FakeDecal scripts can remain Unknown in an all-script dump.

Impact GPU emitters produce LWC-related diagnostic messages in their CPU script compilation paths, with UpToDateWithWarnings statuses; GPU compute scripts compile UpToDate. These messages are retained and are not represented as a clean-warning result. ReadyToRun=false is expected for this commandlet because Niagara explicitly returns false when FApp::CanEverRender() is false under NullRHI. No claim of rendered visual correctness follows from commandlet success. GPU shader/cook behavior, socket orientation, trigger timing, LWC at distant coordinates and particle budget remain pending visual/runtime checks after separate user approval.

A future visual pass should check muzzle direction, shell scale, normal orientation and character spark style, and measure one Niagara component per visible projectile/pellet. Cosmetic multicast remains unreliable, so FX can be lost under packet loss; server damage does not depend on them. No GPU/render session, PIE, multiplayer, smoke test or packet-loss run was started.

## Files

New source: Source/NEXTGENONE/Gameplay/Combat/NGWeaponVFX.h/.cpp and NGWeaponVFXAssets.cpp.
Updated source: NGCombat.h/.cpp, NGBallisticProjectile.h/.cpp, NGSetupCommandlet.cpp; NEXTGENONE.Build.cs adds the required Niagara runtime dependency. NEXTGENONE.uproject explicitly enables Niagara. No NiagaraEditor framework dependency remains.

Updated project assets: DA_Pistol, DA_Rifle, DA_Shotgun, DA_TestWeapon, BP_BallisticProjectile, GA_Fire and BP_NextgenoneCharacter. Backups are in Saved/RefactorBackup/WeaponVFX and Saved/RefactorBackup/Interaction. Logs: Saved/Logs/NGWeaponVFXBuild.log and NGWeaponVFXAssets.log.
Final delivered verification: NEXTGENONEEditor Win64 Development UBT exit 0 / PASS, no compiler diagnostics. NGSetup -WeaponVFXOnly exit 0; summary 0 errors / 14 ALS/environment warnings. Active Niagara script statuses: 58 UpToDate, 14 UpToDateWithWarnings, zero Error/Unknown/Dirty. LWC diagnostics remain associated with the warning statuses; no graph-rewrite fix is claimed. Seven systems and four weapon definitions saved. This is asset/code compilation evidence, not a visual or network test.