#if WITH_EDITOR
#include "Gameplay/Combat/NGCombat.h"
#include "NiagaraSystem.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "NiagaraScript.h"
#include "NiagaraEmitter.h"

int32 NGConfigureWeaponVFX()
{
	auto Load=[](const TCHAR* Path) { const FString P=Path; return LoadObject<UNiagaraSystem>(nullptr,*(P+TEXT(".")+FPackageName::GetLongPackageAssetName(P))); };
	auto* Muzzle=Load(TEXT("/Game/Effects/Particles/Weapons/NS_WeaponFire_MuzzleFlash_Rifle"));
	auto* Shell=Load(TEXT("/Game/Effects/Particles/Weapons/NS_WeaponFire_ShellEject"));
	auto* Tracer=Load(TEXT("/Game/Effects/Particles/Weapons/NS_WeaponFire_Tracer"));
	auto* ShotgunTracer=Load(TEXT("/Game/Effects/Particles/Weapons/NS_WeaponFire_Tracer_Shotgun"));
	auto* Concrete=Load(TEXT("/Game/Effects/Particles/Impacts/NS_ImpactConcrete"));
	auto* Character=Load(TEXT("/Game/Effects/Particles/Impacts/NS_ImactSparksCharacter"));
	auto* Glass=Load(TEXT("/Game/Effects/Particles/Impacts/NS_ImpactGlass"));
	const FString BackupDir=FPaths::ProjectSavedDir()/TEXT("RefactorBackup/WeaponVFX"); IFileManager::Get().MakeDirectory(*BackupDir,true);
	auto Save=[&](UObject* Asset)
	{
		const FString File=FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(),TEXT(".uasset"));
		if (IFileManager::Get().FileExists(*File))
		{
			const FString Backup=BackupDir/(Asset->GetName()+TEXT("_")+FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"))+TEXT(".uasset"));
			if (IFileManager::Get().Copy(*Backup,*File,false)!=COPY_OK) return false;
		}
		FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone; return UPackage::SavePackage(Asset->GetOutermost(),Asset,*File,Args);
	};
	for (auto* System:{Muzzle,Shell,Tracer,ShotgunTracer,Concrete,Character,Glass})
	{
		if (!System) return 126;
		System->RequestCompile(false); System->WaitForCompilationComplete(false,false);
		TArray<UNiagaraScript*> Scripts={System->GetSystemSpawnScript(),System->GetSystemUpdateScript()};
		for (const auto& Handle:System->GetEmitterHandles()) if (Handle.GetIsEnabled()) if (auto* Data=Handle.GetEmitterData()) Data->GetScripts(Scripts,true,true);
		bool bCompileError=false;
		for (auto* Script:Scripts) if (Script)
			UE_LOG(LogTemp,Display,TEXT("NG_WEAPON_VFX_SCRIPT_STATUS %s %s"),*Script->GetPathName(),*StaticEnum<ENiagaraScriptCompileStatus>()->GetNameStringByValue(static_cast<int64>(Script->GetLastCompileStatus())));
		for (auto* Script:Scripts) if (Script && (Script->GetLastCompileStatus()==ENiagaraScriptCompileStatus::NCS_Error || Script->GetLastCompileStatus()==ENiagaraScriptCompileStatus::NCS_Unknown || Script->GetLastCompileStatus()==ENiagaraScriptCompileStatus::NCS_Dirty))
		{
			bCompileError=true;
			for (const auto& Event:Script->GetVMExecutableData().LastCompileEvents)
			{
				if (Event.Severity!=FNiagaraCompileEventSeverity::Error) continue;
				UE_LOG(LogTemp,Error,TEXT("NG_WEAPON_VFX_SCRIPT_ERROR %s node=%s %s"),*Script->GetPathName(),*Event.NodeGuid.ToString(),*Event.Message);
			}
		}
		if (bCompileError) return 131;
		if (!System->IsValid() || !Save(System)) { UE_LOG(LogTemp,Error,TEXT("NG_WEAPON_VFX failed Niagara compilation/save: %s"),*System->GetName()); return 127; }
		UE_LOG(LogTemp,Display,TEXT("NG_WEAPON_VFX_COMPILED %s ready=%d"),*System->GetName(),System->IsReadyToRun());
	}
	for (const TCHAR* Name:{TEXT("Pistol"),TEXT("Rifle"),TEXT("Shotgun"),TEXT("TestWeapon")})
	{
		const FString Kind=Name,P=TEXT("/Game/Gameplay/Weapons/DA_")+Kind+TEXT(".DA_")+Kind;
		auto* Def=LoadObject<UNGWeaponDefinition>(nullptr,*P); if (!Def) return 128;
		if (!Def->MuzzleFX) Def->MuzzleFX=Muzzle;
		if (!Def->ShellEjectFX) Def->ShellEjectFX=Shell;
		if (!Def->TracerFX) Def->TracerFX=Kind==TEXT("Shotgun") ? ShotgunTracer : Tracer;
		if (!Def->ImpactFX) Def->ImpactFX=Concrete;
		if (!Def->CharacterImpactFX) Def->CharacterImpactFX=Character;
		const FString MeshName=Kind==TEXT("Shotgun") ? TEXT("SM_shotgunshell") : Kind==TEXT("Rifle") ? TEXT("SM_rifleshell") : TEXT("SM_pistolshell");
		if (!Def->ShellMesh) Def->ShellMesh=LoadObject<UStaticMesh>(nullptr,*(TEXT("/Game/Effects/Meshes/BulletShells/")+MeshName+TEXT(".")+MeshName));
		if (!Def->ShellMesh || !Save(Def)) return 129;
	}
	UE_LOG(LogTemp,Display,TEXT("NG_WEAPON_VFX_SAVED: 7 systems compiled; muzzle/shell/tracer/world/character impacts bound to 4 weapon definitions")); return 0;
}
#endif
