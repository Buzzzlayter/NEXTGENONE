#if WITH_EDITOR
#include "NGCombat.h"
#include "NiagaraSystem.h"
#include "NiagaraParameterStore.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "Misc/FileHelper.h"
#include "NiagaraScript.h"
#include "NiagaraEmitter.h"

int32 NGImportWeaponVFX()
{
	const TArray<FString> Roots={
		TEXT("/Game/Effects/Particles/Weapons/NS_WeaponFire_MuzzleFlash_Rifle"),
		TEXT("/Game/Effects/Particles/Weapons/NS_WeaponFire_ShellEject"),
		TEXT("/Game/Effects/Particles/Weapons/NS_WeaponFire_Tracer"),
		TEXT("/Game/Effects/Particles/Weapons/NS_WeaponFire_Tracer_Shotgun"),
		TEXT("/Game/Effects/Particles/Impacts/NS_ImpactConcrete"),
		TEXT("/Game/Effects/Particles/Impacts/NS_ImpactGlass"),
		TEXT("/Game/Effects/Particles/Impacts/NS_ImactSparksCharacter")};
	auto& Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FString> Queue=Roots,Imported; TSet<FString> Visited;
	for (const TCHAR* Mesh:{TEXT("SM_pistolshell"),TEXT("SM_rifleshell"),TEXT("SM_shotgunshell")}) Queue.Add(TEXT("/Game/Effects/Meshes/BulletShells/")+FString(Mesh));
	for (int32 I=0;I<Queue.Num();++I)
	{
		const FString Package=Queue[I]; if (Visited.Contains(Package)) continue; Visited.Add(Package);
		if (!Package.StartsWith(TEXT("/Game/"))) continue;
		const FString Destination=FPackageName::LongPackageNameToFilename(Package,TEXT(".uasset"));
		if (!IFileManager::Get().FileExists(*Destination))
		{
			const FString Source=TEXT("F:/UNREAL/Projects/LyraStarterGame/Content/")+Package.RightChop(6)+TEXT(".uasset");
			if (!IFileManager::Get().FileExists(*Source)) { UE_LOG(LogTemp,Error,TEXT("LYRA_VFX missing %s"),*Package); return 120; }
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Destination),true);
			if (IFileManager::Get().Copy(*Destination,*Source,false)!=COPY_OK) return 121;
			for (const TCHAR* Extension:{TEXT(".uexp"),TEXT(".ubulk")})
			{
				const FString Sidecar=FPaths::ChangeExtension(Source,FString(Extension).RightChop(1));
				const FString Target=FPaths::ChangeExtension(Destination,FString(Extension).RightChop(1));
				if (IFileManager::Get().FileExists(*Sidecar) && !IFileManager::Get().FileExists(*Target) && IFileManager::Get().Copy(*Target,*Sidecar,false)!=COPY_OK) return 122;
			}
			Imported.Add(Package);
		}
		Registry.ScanFilesSynchronous({Destination},true);
		TArray<FName> Dependencies; Registry.GetDependencies(FName(Package),Dependencies);
		for (FName Dependency:Dependencies)
		{
			const FString Path=Dependency.ToString();
			if (Path.StartsWith(TEXT("/Game/"))) Queue.AddUnique(Path);
			else if (Path.StartsWith(TEXT("/Script/Lyra")) || Path.StartsWith(TEXT("/ShooterCore/"))) { UE_LOG(LogTemp,Error,TEXT("LYRA_VFX framework dependency %s"),*Path); return 123; }
		}
		if (Queue.Num()>512) { UE_LOG(LogTemp,Error,TEXT("LYRA_VFX dependency closure exceeds scope")); return 124; }
	}
	if (!Imported.IsEmpty())
	{
		const FString Manifest=FPaths::ProjectSavedDir()/TEXT("Logs/LyraWeaponVFXImports.txt");
		TArray<FString> All; FFileHelper::LoadFileToStringArray(All,*Manifest); for (const auto& Path:Imported) All.AddUnique(Path); FFileHelper::SaveStringArrayToFile(All,*Manifest);
	}
	for (const FString& Path:Roots)
	{
		auto* System=LoadObject<UNiagaraSystem>(nullptr,*(Path+TEXT(".")+FPackageName::GetLongPackageAssetName(Path))); if (!System) return 125;
		TArray<FNiagaraVariable> Parameters; System->GetExposedParameters().GetParameters(Parameters);
		for (const auto& P:Parameters) UE_LOG(LogTemp,Display,TEXT("LYRA_VFX_PARAMETER %s %s %s"),*System->GetName(),*P.GetName().ToString(),*P.GetType().GetName());
	}
	UE_LOG(LogTemp,Display,TEXT("LYRA_VFX_IMPORTED: new=%d closure=%d systems=%d"),Imported.Num(),Visited.Num(),Roots.Num()); return 0;
}
int32 NGConfigureWeaponVFX()
{
	const int32 Result=NGImportWeaponVFX(); if (Result!=0) return Result;
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
			UE_LOG(LogTemp,Display,TEXT("LYRA_VFX_SCRIPT_STATUS %s %s"),*Script->GetPathName(),*StaticEnum<ENiagaraScriptCompileStatus>()->GetNameStringByValue(static_cast<int64>(Script->GetLastCompileStatus())));
		for (auto* Script:Scripts) if (Script && (Script->GetLastCompileStatus()==ENiagaraScriptCompileStatus::NCS_Error || Script->GetLastCompileStatus()==ENiagaraScriptCompileStatus::NCS_Unknown || Script->GetLastCompileStatus()==ENiagaraScriptCompileStatus::NCS_Dirty))
		{
			bCompileError=true;
			for (const auto& Event:Script->GetVMExecutableData().LastCompileEvents)
			{
				if (Event.Severity!=FNiagaraCompileEventSeverity::Error) continue;
				UE_LOG(LogTemp,Error,TEXT("LYRA_VFX_SCRIPT_ERROR %s node=%s %s"),*Script->GetPathName(),*Event.NodeGuid.ToString(),*Event.Message);
			}
		}
		if (bCompileError) return 131;
		if (!System->IsValid() || !Save(System)) { UE_LOG(LogTemp,Error,TEXT("LYRA_VFX failed Niagara compilation/save: %s"),*System->GetName()); return 127; }
		UE_LOG(LogTemp,Display,TEXT("LYRA_VFX_COMPILED %s ready=%d"),*System->GetName(),System->IsReadyToRun());
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
	UE_LOG(LogTemp,Display,TEXT("NG_WEAPON_VFX_SAVED: 7 Lyra systems compiled; muzzle/shell/tracer/world/character impacts bound to 4 weapon definitions")); return 0;
}
#endif
