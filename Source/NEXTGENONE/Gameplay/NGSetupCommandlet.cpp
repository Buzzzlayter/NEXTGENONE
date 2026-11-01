#include "NGSetupCommandlet.h"

UNGSetupCommandlet::UNGSetupCommandlet() { IsClient=false; IsServer=false; IsEditor=true; LogToConsole=true; }

#if WITH_EDITOR
int32 NGCreateSettingsAssets();
int32 NGRemoveALSMenuAssets();
int32 NGImportWeaponVFX();
int32 NGConfigureWeaponVFX();
#include "NextgenoneCharacter.h"
#include "NextgenonePlayerController.h"
#include "NextgenonePlayerState.h"
#include "NextgenoneGameMode.h"
#include "NGTestGameMode.h"
#include "NGDashAbility.h"
#include "NGJumpAbility.h"
#include "NGMantleAbility.h"
#include "NGWeaponPickup.h"
#include "NGCombat.h"
#include "NGPunchAbility.h"
#include "NGInteraction.h"
#include "NGItemPickup.h"
#include "NGInteractionPrompt.h"
#include "NGAimComponent.h"
#include "NGBallisticProjectile.h"
#include "NGHitReactionComponent.h"
#include "NGConnectionPanel.h"
#include "NGSettingsPanel.h"
#include "Sound/SoundBase.h"
#include "Inventory/NGInventoryItemDefinition.h"
#include "Inventory/NGInventoryFragment_SetStats.h"
#include "Inventory/NGInventoryPanel.h"
#include "Inventory/NGInventoryStateComponent.h"
#include "HFSMState.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerStart.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TimelineComponent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "FileHelpers.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/ObjectRedirector.h"

namespace
{
bool SaveAbilityAsset(UObject* Asset)
{
	const FString File = FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
	FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
	return UPackage::SavePackage(Asset->GetOutermost(), Asset, *File, Args);
}
UBlueprint* MakeAbilityBlueprint(const TCHAR* Name, UClass* Parent, bool& bCreated)
{
	const FString Path = TEXT("/Game/Gameplay/Abilities/") + FString(Name);
	bCreated = !FPackageName::DoesPackageExist(Path);
	if (!bCreated)
	{
		auto* Existing = LoadObject<UBlueprint>(nullptr, *(Path + TEXT(".") + Name));
		return Existing && Existing->GeneratedClass && Existing->GeneratedClass->IsChildOf(Parent) ? Existing : nullptr;
	}
	return FKismetEditorUtilities::CreateBlueprint(Parent, CreatePackage(*Path), FName(Name), BPTYPE_Normal,
		UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), TEXT("NGAbilityAssets"));
}
bool CompileAndSaveAbility(UBlueprint* BP)
{
	FKismetEditorUtilities::CompileBlueprint(BP);
	return BP->Status != BS_Error && SaveAbilityAsset(BP);
}
bool BackupInteractionAsset(UObject* Asset)
{
	const FString File=FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
	if (!IFileManager::Get().FileExists(*File)) return true;
	const FString Directory=FPaths::ProjectSavedDir()/TEXT("RefactorBackup/Interaction"); IFileManager::Get().MakeDirectory(*Directory,true);
	const FString Backup=Directory/(Asset->GetName()+TEXT("_")+FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"))+TEXT(".uasset"));
	return IFileManager::Get().Copy(*Backup,*File,false)==COPY_OK;
}
UBlueprint* MakeInteractionBlueprint(const FString& Path,UClass* Parent,bool& bCreated,bool bWidget=false)
{
	bCreated=!FPackageName::DoesPackageExist(Path);
	if (!bCreated)
	{
		auto* Existing=LoadObject<UBlueprint>(nullptr,*(Path+TEXT(".")+FPackageName::GetLongPackageAssetName(Path)));
		return Existing && Existing->GeneratedClass && Existing->GeneratedClass->IsChildOf(Parent) ? Existing : nullptr;
	}
	return FKismetEditorUtilities::CreateBlueprint(Parent,CreatePackage(*Path),FName(FPackageName::GetLongPackageAssetName(Path)),BPTYPE_Normal,
		bWidget ? UWidgetBlueprint::StaticClass() : UBlueprint::StaticClass(),bWidget ? UWidgetBlueprintGeneratedClass::StaticClass() : UBlueprintGeneratedClass::StaticClass(),TEXT("NGInteractionSetup"));
}
int32 RefreshBodyImpactAssets()
{
	for (const TCHAR* Path:{TEXT("/Game/Blueprints/BP_NextgenoneCharacter.BP_NextgenoneCharacter"),TEXT("/Game/Gameplay/Weapons/BP_BallisticProjectile.BP_BallisticProjectile")})
	{
		auto* BP=LoadObject<UBlueprint>(nullptr,Path); if (!BP || !BackupInteractionAsset(BP) || !CompileAndSaveAbility(BP)) return 114;
	}
	for (const TCHAR* Kind:{TEXT("Pistol"),TEXT("Rifle"),TEXT("Shotgun")})
	{
		const FString Name=Kind,Path=TEXT("/Game/Gameplay/Weapons/DA_")+Name+TEXT(".DA_")+Name;
		auto* Def=LoadObject<UNGWeaponDefinition>(nullptr,*Path); if (!Def || !BackupInteractionAsset(Def)) return 115;
		if (Def->BodyImpactImpulse==1600.f) Def->BodyImpactImpulse=Name==TEXT("Shotgun") ? 500.f : Name==TEXT("Pistol") ? 1200.f : 1800.f;
		if (!SaveAbilityAsset(Def)) return 116;
	}
	auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Blueprints/BP_NextgenoneCharacter.BP_NextgenoneCharacter"));
	const auto* Pawn=Cast<ANextgenoneCharacter>(BP->GeneratedClass->GetDefaultObject());
	if (!Pawn || !Pawn->HitReaction || !Pawn->GetMesh()->GetPhysicsAsset()) return 117;
	UE_LOG(LogTemp,Display,TEXT("NG_BODY_IMPACT_ASSETS_SAVED: HitReaction present; Character PhysicsAsset present; projectile and weapon impulse defaults compiled/saved"));
	return 0;
}
int32 RefreshAimAssets()
{
	for (const TCHAR* Path:{TEXT("/Game/Blueprints/BP_NextgenoneCharacter.BP_NextgenoneCharacter"),
		TEXT("/Game/Gameplay/UI/WBP_InteractionPrompt.WBP_InteractionPrompt"),TEXT("/Game/Gameplay/Abilities/GA_Interact.GA_Interact")})
	{
		auto* BP=LoadObject<UBlueprint>(nullptr,Path);
		if (!BP || !BackupInteractionAsset(BP) || !CompileAndSaveAbility(BP)) return 95;
	}
	auto* CharacterBP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Blueprints/BP_NextgenoneCharacter.BP_NextgenoneCharacter"));
	const auto* Pawn=Cast<ANextgenoneCharacter>(CharacterBP->GeneratedClass->GetDefaultObject());
	if (!Pawn || !Pawn->Aim || !Pawn->Interaction) return 96;
	auto* Prompt=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Gameplay/UI/WBP_InteractionPrompt.WBP_InteractionPrompt"));
	const auto* Class=Cast<UWidgetBlueprintGeneratedClass>(Prompt->GeneratedClass);
	UE_LOG(LogTemp,Display,TEXT("NG_AIM_ASSETS_SAVED: Character Aim present; range=%.1f capture=%.1f; prompt native tick=%d"),Pawn->Interaction->MaxDistance,Pawn->Interaction->CaptureRadius,Class && Class->ClassRequiresNativeTick());
	return 0;
}
int32 CreateInteractionAssets()
{
	bool bCreated;
	const FString AbilityPath=TEXT("/Game/Gameplay/Abilities/GA_Interact");
	UBlueprint* Ability=nullptr;
	if (!FPackageName::DoesPackageExist(AbilityPath))
	{
		if (auto* Old=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Gameplay/Abilities/GA_PickupWeapon.GA_PickupWeapon")))
		{
			Ability=Cast<UBlueprint>(FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().DuplicateAsset(TEXT("GA_Interact"),TEXT("/Game/Gameplay/Abilities"),Old));
			if (!Ability) return 70;
			Ability->ParentClass=UNGInteractionAbility::StaticClass(); FBlueprintEditorUtils::RefreshAllNodes(Ability); FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Ability);
			if (!CompileAndSaveAbility(Ability)) return 71;
		}
	}
	if (!Ability)
	{
		Ability=MakeInteractionBlueprint(AbilityPath,UNGInteractionAbility::StaticClass(),bCreated);
		if (!Ability || (bCreated && !CompileAndSaveAbility(Ability))) return 72;
	}
	auto* Item=MakeInteractionBlueprint(TEXT("/Game/Gameplay/Inventory/ID_TestItem"),UNGInventoryItemDefinition::StaticClass(),bCreated);
	if (!Item) return 73;
	if (bCreated)
	{
		auto* Defaults=CastChecked<UNGInventoryItemDefinition>(Item->GeneratedClass->GetDefaultObject());
		Defaults->DisplayName=NSLOCTEXT("NEXTGENONE","GenericTestItem","Тестовый предмет"); Defaults->MaxStackSize=100;
		Defaults->PickupMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")); Defaults->PickupTransform.SetScale3D(FVector(.25f));
		if (!CompileAndSaveAbility(Item)) return 74;
	}
	auto* Pickup=MakeInteractionBlueprint(TEXT("/Game/Gameplay/Interaction/BP_ItemPickup"),ANGItemPickup::StaticClass(),bCreated);
	if (!Pickup) return 75;
	if (bCreated)
	{
		CastChecked<ANGItemPickup>(Pickup->GeneratedClass->GetDefaultObject())->ItemClass=Item->GeneratedClass;
		if (!CompileAndSaveAbility(Pickup)) return 76;
	}
	auto* Interactive=MakeInteractionBlueprint(TEXT("/Game/Gameplay/Interaction/BP_InteractiveObject"),ANGInteractiveObject::StaticClass(),bCreated);
	if (!Interactive || (bCreated && !CompileAndSaveAbility(Interactive))) return 77;
	auto* Prompt=MakeInteractionBlueprint(TEXT("/Game/Gameplay/UI/WBP_InteractionPrompt"),UNGInteractionPrompt::StaticClass(),bCreated,true);
	if (!Prompt || (bCreated && !CompileAndSaveAbility(Prompt))) return 78;
	auto* Set=LoadObject<UNGAbilitySet>(nullptr,TEXT("/Game/Gameplay/Abilities/DA_PlayerAbilities.DA_PlayerAbilities"));
	if (!Set) return 79;
	int32 ExistingIndex=INDEX_NONE;
	for (int32 Index=0; Index<Set->Abilities.Num(); ++Index)
		if (Set->Abilities[Index].InputTag==TAG_NGInputPickup || Set->Abilities[Index].InputTag==TAG_NGInputInteract)
		{
			if (ExistingIndex!=INDEX_NONE) { UE_LOG(LogTemp,Error,TEXT("INTERACTION_SETUP: multiple interaction grants; resolve before migration")); return 80; }
			ExistingIndex=Index;
		}
	bool bChangeSet=ExistingIndex==INDEX_NONE || Set->Abilities[ExistingIndex].InputTag!=TAG_NGInputInteract;
	if (bChangeSet)
	{
		if (!BackupInteractionAsset(Set)) return 81;
		if (ExistingIndex==INDEX_NONE) ExistingIndex=Set->Abilities.AddDefaulted();
		auto& Grant=Set->Abilities[ExistingIndex];
		if (!Grant.Ability || Grant.Ability->GetPathName()==TEXT("/Game/Gameplay/Abilities/GA_PickupWeapon.GA_PickupWeapon_C")) Grant.Ability=Ability->GeneratedClass;
		if (!Grant.Ability->IsChildOf(UNGInteractionAbility::StaticClass())) { UE_LOG(LogTemp,Error,TEXT("INTERACTION_SETUP: custom interaction grant needs adaptation; preserved")); return 82; }
		Grant.InputTag=TAG_NGInputInteract; if (!SaveAbilityAsset(Set)) return 83;
	}
	auto* StateBP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Blueprints/States/FSMState_GameRuntime.FSMState_GameRuntime"));
	auto* State=StateBP && StateBP->GeneratedClass ? Cast<UHFSMState>(StateBP->GeneratedClass->GetDefaultObject()) : nullptr;
	auto* Property=FindFProperty<FArrayProperty>(UHFSMState::StaticClass(),TEXT("Components"));
	if (!State || !Property) return 84;
	auto* Inner=CastField<FObjectPropertyBase>(Property->Inner); if (!Inner) return 85;
	FScriptArrayHelper Components(Property,Property->ContainerPtrToValuePtr<void>(State));
	bool bHasPrompt=false;
	for (int32 Index=0; Index<Components.Num(); ++Index) bHasPrompt|=Cast<UNGInteractionPromptMounter>(Inner->GetObjectPropertyValue(Components.GetRawPtr(Index)))!=nullptr;
	if (!bHasPrompt)
	{
		if (!BackupInteractionAsset(StateBP)) return 86;
		auto* Mounter=NewObject<UNGInteractionPromptMounter>(State,TEXT("InteractionPrompt"),RF_Transactional);
		auto* ClassProperty=FindFProperty<FClassProperty>(UUIWidgetMounterComponent::StaticClass(),TEXT("WidgetClass"));
		if (!ClassProperty) return 87; ClassProperty->SetPropertyValue_InContainer(Mounter,Prompt->GeneratedClass);
		const int32 Index=Components.AddValue(); Inner->SetObjectPropertyValue(Components.GetRawPtr(Index),Mounter);
		FBlueprintEditorUtils::MarkBlueprintAsModified(StateBP); if (!CompileAndSaveAbility(StateBP)) return 88;
	}
	if (auto* Legacy=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Gameplay/Weapons/BP_WeaponPickup.BP_WeaponPickup")))
	{ if (!BackupInteractionAsset(Legacy) || !CompileAndSaveAbility(Legacy)) return 89; }
	UE_LOG(LogTemp,Display,TEXT("NG_INTERACTION_ASSETS_SAVED: BP_ItemPickup BP_InteractiveObject GA_Interact ID_TestItem WBP_InteractionPrompt; Runtime prompt and single interaction grant saved"));
	return 0;
}
bool ImportLyraArtDependencies()
{
	auto& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.ScanPathsSynchronous({TEXT("/Game/Weapons"),TEXT("/Game/Audio/SoundWaves/Weapons")}, true);
	TArray<FAssetData> Assets; Registry.GetAssetsByPath(TEXT("/Game/Weapons"), Assets, true);
	Registry.GetAssetsByPath(TEXT("/Game/Audio/SoundWaves/Weapons"),Assets,true);
	TArray<FName> Queue; for (const auto& Asset : Assets) Queue.AddUnique(Asset.PackageName);
	TSet<FName> Visited;
	for (int32 Index=0; Index<Queue.Num(); ++Index)
	{
		const FName Package = Queue[Index]; if (Visited.Contains(Package)) continue; Visited.Add(Package);
		TArray<FName> Dependencies; Registry.GetDependencies(Package, Dependencies);
		for (const auto Dependency : Dependencies)
		{
			const FString Path = Dependency.ToString();
			if (!Path.StartsWith(TEXT("/Game/"))) continue;
			if (!FPackageName::DoesPackageExist(Path))
			{
				const FString Source = FString(TEXT("F:/UNREAL/Projects/LyraStarterGame/Content/")) + Path.RightChop(6) + TEXT(".uasset");
				const FString Destination = FPackageName::LongPackageNameToFilename(Path, FPackageName::GetAssetPackageExtension());
				if (!IFileManager::Get().FileExists(*Source)) { UE_LOG(LogTemp, Error, TEXT("LYRA_PORT missing dependency: %s"), *Path); return false; }
				IFileManager::Get().MakeDirectory(*FPaths::GetPath(Destination), true);
				if (IFileManager::Get().Copy(*Destination, *Source, false) != COPY_OK) return false;
				Registry.ScanFilesSynchronous({Destination}, true);
				UE_LOG(LogTemp, Display, TEXT("LYRA_PORT dependency copied: %s"), *Path);
			}
			Queue.AddUnique(Dependency);
		}
		if (Queue.Num() > 1000) { UE_LOG(LogTemp, Error, TEXT("LYRA_PORT unexpected dependency expansion; stopped before importing a framework")); return false; }
	}
	return true;
}
bool BindInventoryRuntimeState()
{
	const FString Path = TEXT("/Game/Blueprints/States/FSMState_GameRuntime");
	auto* BP = LoadObject<UBlueprint>(nullptr, *(Path + TEXT(".FSMState_GameRuntime")));
	auto* State = BP && BP->GeneratedClass ? Cast<UHFSMState>(BP->GeneratedClass->GetDefaultObject()) : nullptr;
	auto* Property = FindFProperty<FArrayProperty>(UHFSMState::StaticClass(), TEXT("Components"));
	if (!State || !Property) return false;
	auto* Inner = CastField<FObjectPropertyBase>(Property->Inner); if (!Inner) return false;
	FScriptArrayHelper Components(Property, Property->ContainerPtrToValuePtr<void>(State));
	for (int32 Index=0; Index<Components.Num(); ++Index)
		if (Cast<UNGInventoryStateComponent>(Inner->GetObjectPropertyValue(Components.GetRawPtr(Index)))) return true;
	const FString BackupDir = FPaths::ProjectSavedDir() / TEXT("RefactorBackup/InventoryUI");
	IFileManager::Get().MakeDirectory(*BackupDir, true);
	const FString Backup = BackupDir / (TEXT("FSMState_GameRuntime_") + FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")) + TEXT(".uasset"));
	const FString Original = FPackageName::LongPackageNameToFilename(Path, FPackageName::GetAssetPackageExtension());
	if (IFileManager::Get().Copy(*Backup, *Original, false) != COPY_OK) return false;
	auto* Component = NewObject<UNGInventoryStateComponent>(State, TEXT("InventoryWindow"), RF_Transactional);
	Component->PanelClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(TEXT("/Game/Gameplay/UI/WBP_InventoryPanel.WBP_InventoryPanel_C")));
	const int32 NewIndex = Components.AddValue(); Inner->SetObjectPropertyValue(Components.GetRawPtr(NewIndex), Component);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	if (!CompileAndSaveAbility(BP)) return false;
	UE_LOG(LogTemp, Display, TEXT("NG_INVENTORY_HFSM_BOUND: GameRuntime component appended; existing graphs/components retained; backup=%s"), *Backup);
	return true;
}
int32 CreateLyraWeaponAssets()
{
	if (!ImportLyraArtDependencies()) return 50;
	bool bCreated;
	auto* Reload = MakeAbilityBlueprint(TEXT("GA_Reload"), UNGReloadAbility::StaticClass(), bCreated);
	if (!Reload || (bCreated && !CompileAndSaveAbility(Reload))) return 51;
	const FString InventoryUIPath = TEXT("/Game/Gameplay/UI/WBP_InventoryPanel");
	if (!FPackageName::DoesPackageExist(InventoryUIPath))
	{
		auto* WidgetBP = FKismetEditorUtilities::CreateBlueprint(UNGInventoryPanel::StaticClass(), CreatePackage(*InventoryUIPath), TEXT("WBP_InventoryPanel"),
			BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass(), TEXT("LyraInventoryPort"));
		if (!WidgetBP || !CompileAndSaveAbility(WidgetBP)) return 57;
	}
	for (int32 Kind=0; Kind<3; ++Kind)
	{
		const FString Name = Kind == 0 ? TEXT("Pistol") : Kind == 1 ? TEXT("Rifle") : TEXT("TestWeapon");
		const FString DefPath = TEXT("/Game/Gameplay/Weapons/DA_") + Name;
		const bool bNewDef = !FPackageName::DoesPackageExist(DefPath);
		auto* Def = bNewDef ? NewObject<UNGWeaponDefinition>(CreatePackage(*DefPath), FName(TEXT("DA_") + Name), RF_Public|RF_Standalone) :
			LoadObject<UNGWeaponDefinition>(nullptr, *(DefPath + TEXT(".DA_") + Name));
		if (!Def) return 52;
		const FString ItemPath = TEXT("/Game/Gameplay/Inventory/ID_") + Name;
		if (bNewDef && Kind < 2)
		{
			Def->InventoryItemClass = TSoftClassPtr<UNGInventoryItemDefinition>(FSoftObjectPath(ItemPath + TEXT(".ID_") + Name + TEXT("_C")));
			Def->Mesh = nullptr;
			Def->SkeletalMesh = LoadObject<USkeletalMesh>(nullptr, *(TEXT("/Game/Weapons/") + Name + TEXT("/Mesh/SK_") + Name + TEXT(".SK_") + Name));
			Def->Overlay = Kind == 0 ? EALSOverlayState::PistolTwoHanded : EALSOverlayState::Rifle;
			Def->bAutomatic = Kind == 1; Def->MagazineCapacity = Kind == 0 ? 15 : 30;
			Def->InitialReserveAmmo = Kind == 0 ? 60 : 120; Def->Damage = Kind == 0 ? 25.f : 20.f;
			Def->FireInterval = Kind == 0 ? .22f : .1f; Def->ReloadDuration = Kind == 0 ? 1.3f : 1.8f;
			Def->VisualTransform = FTransform::Identity;
			const FString AnimationRoot = TEXT("/Game/Weapons/") + Name + TEXT("/Animations/Weap_") + Name;
			Def->WeaponFireAnimation = LoadObject<UAnimSequence>(nullptr, *(AnimationRoot + TEXT("_Fire.Weap_") + Name + TEXT("_Fire")));
			Def->WeaponReloadAnimation = LoadObject<UAnimSequence>(nullptr, *(AnimationRoot + TEXT("_Reload.Weap_") + Name + TEXT("_Reload")));
			if (!Def->SkeletalMesh || !Def->WeaponFireAnimation || !Def->WeaponReloadAnimation) return 53;
		}
		if (!FPackageName::DoesPackageExist(ItemPath))
		{
			auto* BP = FKismetEditorUtilities::CreateBlueprint(UNGInventoryItemDefinition::StaticClass(), CreatePackage(*ItemPath), FName(TEXT("ID_")+Name),
				BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), TEXT("LyraInventoryPort"));
			if (!BP) return 54;
			auto* ItemCDO = CastChecked<UNGInventoryItemDefinition>(BP->GeneratedClass->GetDefaultObject());
			ItemCDO->DisplayName = FText::FromString(Name); ItemCDO->Weapon = Def; ItemCDO->MaxStackSize = 1;
			auto* Stats = NewObject<UNGInventoryFragment_SetStats>(ItemCDO, TEXT("InitialAmmo"), RF_Transactional);
			Stats->InitialItemStats.Add(TAG_NGMagazineAmmo, Def->MagazineCapacity);
			Stats->InitialItemStats.Add(TAG_NGReserveAmmo, Def->InitialReserveAmmo); ItemCDO->Fragments.Add(Stats);
			if (!CompileAndSaveAbility(BP)) return 55;
		}
		if (bNewDef && !SaveAbilityAsset(Def)) return 56;
	}
	if (!BindInventoryRuntimeState()) return 58;
	UE_LOG(LogTemp, Display, TEXT("NG_LYRA_WEAPONS_SAVED: ID_Pistol ID_Rifle ID_TestWeapon, DA_Pistol DA_Rifle, GA_Reload and HFSM inventory UI; actual Lyra art"));
	return 0;
}
int32 CreateBallisticAssets()
{
	if (!ImportLyraArtDependencies()) return 100;
	bool bCreated=false;
	auto* Projectile=MakeInteractionBlueprint(TEXT("/Game/Gameplay/Weapons/BP_BallisticProjectile"),ANGBallisticProjectile::StaticClass(),bCreated);
	if (!Projectile || !CompileAndSaveAbility(Projectile)) return 101;
	for (const TCHAR* Name:{TEXT("Pistol"),TEXT("Rifle"),TEXT("Shotgun")})
	{
		const FString Kind=Name;
		auto* Ammo=MakeInteractionBlueprint(TEXT("/Game/Gameplay/Inventory/ID_Ammo_")+Kind,UNGInventoryItemDefinition::StaticClass(),bCreated);
		if (!Ammo) return 102;
		if (bCreated)
		{
			auto* CDO=CastChecked<UNGInventoryItemDefinition>(Ammo->GeneratedClass->GetDefaultObject());
			CDO->DisplayName=FText::FromString(Kind==TEXT("Pistol") ? TEXT("Пистолетные патроны") : Kind==TEXT("Rifle") ? TEXT("Автоматные патроны") : TEXT("Патроны для дробовика"));
			CDO->MaxStackSize=500; CDO->PickupMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")); CDO->PickupTransform.SetScale3D(FVector(.12,.08,.06));
			if (!CompileAndSaveAbility(Ammo)) return 103;
		}
		const FString DefPath=TEXT("/Game/Gameplay/Weapons/DA_")+Kind;
		const bool bNew=!FPackageName::DoesPackageExist(DefPath);
		auto* Def=bNew ? NewObject<UNGWeaponDefinition>(CreatePackage(*DefPath),FName(TEXT("DA_")+Kind),RF_Public|RF_Standalone) : LoadObject<UNGWeaponDefinition>(nullptr,*(DefPath+TEXT(".DA_")+Kind));
		if (!Def || !BackupInteractionAsset(Def)) return 104;
		if (bNew && Kind==TEXT("Shotgun"))
		{
			Def->Mesh=nullptr; Def->SkeletalMesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Weapons/Shotgun/Mesh/SKM_Shotgun.SKM_Shotgun")); Def->Overlay=EALSOverlayState::Rifle; Def->VisualTransform=FTransform::Identity;
			Def->MagazineCapacity=8; Def->InitialReserveAmmo=32; Def->Damage=9.f; Def->FireInterval=.85f; Def->ReloadDuration=2.2f; Def->Range=6000.f;
			Def->PelletsPerShot=8; Def->SpreadDegrees=4.f; Def->MuzzleSpeed=35000.f;
			Def->WeaponFireAnimation=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Weapons/Shotgun/Animations/Weap_Shotgun_Fire.Weap_Shotgun_Fire"));
			Def->WeaponReloadAnimation=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Weapons/Shotgun/Animations/Weap_Shotgun_Reload.Weap_Shotgun_Reload"));
			if (!Def->SkeletalMesh || !Def->WeaponFireAnimation || !Def->WeaponReloadAnimation) return 105;
		}
		if (Def->MuzzleSpeed==60000.f) Def->MuzzleSpeed=Kind==TEXT("Pistol") ? 45000.f : 85000.f;
		if (Def->AmmoItemClass.IsNull()) Def->AmmoItemClass=Ammo->GeneratedClass;
		if (!Def->ProjectileClass || Def->ProjectileClass==ANGBallisticProjectile::StaticClass()) Def->ProjectileClass=Projectile->GeneratedClass;
		const FString Wave=Kind==TEXT("Pistol") ? TEXT("sfx_Weapon_Pistol_MainLayer_nl_01") : Kind==TEXT("Rifle") ? TEXT("sfx_Weapon_AutoRifle_MainLayer_01") : TEXT("sfx_Weapon_Shotgun_MainLayer_nl_01");
		if (!Def->FireSound) Def->FireSound=LoadObject<USoundBase>(nullptr,*(TEXT("/Game/Audio/SoundWaves/Weapons/")+Wave+TEXT(".")+Wave));
		const FString ItemPath=TEXT("/Game/Gameplay/Inventory/ID_")+Kind;
		auto* Item=MakeInteractionBlueprint(ItemPath,UNGInventoryItemDefinition::StaticClass(),bCreated);
		if (!Item || !BackupInteractionAsset(Item)) return 106;
		auto* CDO=CastChecked<UNGInventoryItemDefinition>(Item->GeneratedClass->GetDefaultObject());
		if (bCreated)
		{
			CDO->DisplayName=FText::FromString(Kind); CDO->Weapon=Def; CDO->MaxStackSize=1;
			auto* Stats=NewObject<UNGInventoryFragment_SetStats>(CDO,TEXT("InitialMagazine"),RF_Transactional); Stats->InitialItemStats.Add(TAG_NGMagazineAmmo,Def->MagazineCapacity); CDO->Fragments.Add(Stats);
		}
		// Reserve rounds now exist as inventory items; preserve every unrelated fragment/stat.
		for (auto Fragment:CDO->Fragments) if (auto* Stats=Cast<UNGInventoryFragment_SetStats>(Fragment)) Stats->InitialItemStats.Remove(TAG_NGReserveAmmo);
		Def->InventoryItemClass=Item->GeneratedClass;
		if (!CompileAndSaveAbility(Item) || !SaveAbilityAsset(Def)) return 107;
	}
	if (auto* Test=LoadObject<UNGWeaponDefinition>(nullptr,TEXT("/Game/Gameplay/Weapons/DA_TestWeapon.DA_TestWeapon")))
	{
		if (!BackupInteractionAsset(Test)) return 108;
		if (Test->AmmoItemClass.IsNull()) Test->AmmoItemClass=TSoftClassPtr<UNGInventoryItemDefinition>(FSoftObjectPath(TEXT("/Game/Gameplay/Inventory/ID_Ammo_Pistol.ID_Ammo_Pistol_C")));
		if (!Test->ProjectileClass || Test->ProjectileClass==ANGBallisticProjectile::StaticClass()) Test->ProjectileClass=Projectile->GeneratedClass;
		if (!SaveAbilityAsset(Test)) return 109;
	}
	auto* Network=MakeInteractionBlueprint(TEXT("/Game/Gameplay/UI/WBP_ServerConnection"),UNGConnectionPanel::StaticClass(),bCreated,true);
	if (!Network || !CompileAndSaveAbility(Network)) return 110;
	auto* Settings=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Gameplay/UI/WBP_GameSettings.WBP_GameSettings"));
	auto* Character=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Blueprints/BP_NextgenoneCharacter.BP_NextgenoneCharacter"));
	if (!Settings || !Character || !BackupInteractionAsset(Settings) || !BackupInteractionAsset(Character)) return 111;
	auto* Screen=CastChecked<UNGSettingsPanel>(Settings->GeneratedClass->GetDefaultObject()); if (!Screen->ConnectionPanelClass) Screen->ConnectionPanelClass=Network->GeneratedClass;
	CastChecked<ANextgenoneCharacter>(Character->GeneratedClass->GetDefaultObject())->Combat->StarterItems.AddUnique(TSoftClassPtr<UNGInventoryItemDefinition>(FSoftObjectPath(TEXT("/Game/Gameplay/Inventory/ID_Shotgun.ID_Shotgun_C"))));
	if (!CompileAndSaveAbility(Settings) || !CompileAndSaveAbility(Character)) return 112;
	for (const TCHAR* Path:{TEXT("/Game/Gameplay/UI/WBP_InventoryPanel.WBP_InventoryPanel"),TEXT("/Game/Gameplay/Abilities/GA_Fire.GA_Fire"),TEXT("/Game/Gameplay/Abilities/GA_Reload.GA_Reload")})
	{
		auto* BP=LoadObject<UBlueprint>(nullptr,Path); if (!BP || !BackupInteractionAsset(BP) || !CompileAndSaveAbility(BP)) return 113;
	}
	UE_LOG(LogTemp,Display,TEXT("NG_BALLISTIC_ASSETS_SAVED: physical projectiles; Pistol/Rifle/Shotgun ammo items; Lyra Shotgun art and shot sounds; dedicated connect UI; Steam stub only"));
	return 0;
}
int32 CreatePunchAssets()
{
	bool bCreated;
	auto* Cooldown = MakeAbilityBlueprint(TEXT("GE_PunchCooldown"), UNGPunchCooldownEffect::StaticClass(), bCreated);
	if (!Cooldown || (bCreated && !CompileAndSaveAbility(Cooldown))) return 40;
	auto* Punch = MakeAbilityBlueprint(TEXT("GA_Punch"), UNGPunchAbility::StaticClass(), bCreated);
	if (!Punch) return 41;
	if (bCreated)
	{
		auto* Property = FindFProperty<FClassProperty>(UGameplayAbility::StaticClass(), TEXT("CooldownGameplayEffectClass"));
		if (!Property) return 42;
		Property->SetPropertyValue_InContainer(Punch->GeneratedClass->GetDefaultObject(), Cooldown->GeneratedClass);
		if (!CompileAndSaveAbility(Punch)) return 43;
	}
	const FString Path = TEXT("/Game/Gameplay/Abilities/DA_PlayerAbilities");
	const bool bExistingSet = FPackageName::DoesPackageExist(Path);
	auto* Set = bExistingSet ? LoadObject<UNGAbilitySet>(nullptr, *(Path + TEXT(".DA_PlayerAbilities"))) :
		NewObject<UNGAbilitySet>(CreatePackage(*Path), TEXT("DA_PlayerAbilities"), RF_Public | RF_Standalone);
	if (!Set) return 44;
	for (const FNGAbilityGrant& Grant : Set->Abilities)
		if (Grant.InputTag == TAG_NGInputPunch) return 0; // Preserve the user's existing choice.
	if (bExistingSet)
	{
		const FString BackupDir = FPaths::ProjectSavedDir() / TEXT("RefactorBackup/Abilities");
		IFileManager::Get().MakeDirectory(*BackupDir, true);
		const FString BackupFile = BackupDir / (TEXT("DA_PlayerAbilities_BeforePunch_") + FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")) + TEXT(".uasset"));
		const FString Source = FPackageName::LongPackageNameToFilename(Path, FPackageName::GetAssetPackageExtension());
		if (IFileManager::Get().Copy(*BackupFile, *Source, false) != COPY_OK) return 45;
	}
	FNGAbilityGrant Grant; Grant.Ability = Punch->GeneratedClass; Grant.InputTag = TAG_NGInputPunch;
	Set->Abilities.Add(Grant);
	if (!SaveAbilityAsset(Set)) return 46;
	UE_LOG(LogTemp, Display, TEXT("NG_PUNCH_ASSETS_SAVED: GA_Punch, GE_PunchCooldown; Input.Combat.Punch appended without replacing existing grants"));
	return 0;
}
bool AdaptCharacterMantle()
{
	auto* CharacterBP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Blueprints/BP_NextgenoneCharacter.BP_NextgenoneCharacter"));
	if (!CharacterBP || !CharacterBP->SimpleConstructionScript) return false;
	for (USCS_Node* Node : CharacterBP->SimpleConstructionScript->GetAllNodes())
	{
		if (!Node->ComponentClass || !Node->ComponentClass->IsChildOf(UALSMantleComponent::StaticClass())) continue;
		if (Node->ComponentClass->IsChildOf(UNGALSMantleExecutor::StaticClass()))
		{
			auto* Property = FindFProperty<FObjectPropertyBase>(UALSMantleComponent::StaticClass(), TEXT("MantleTimeline"));
			if (!Property || !Node->ComponentTemplate) return false;
			auto* Template = Node->ComponentTemplate.Get();
			auto* Timeline = Cast<UTimelineComponent>(Property->GetObjectPropertyValue_InContainer(Template));
			if (!IsValid(Timeline) || Timeline->GetOuter() != Template)
			{
				Timeline = Cast<UTimelineComponent>(Template->GetDefaultSubobjectByName(TEXT("MantleTimeline")));
				if (!IsValid(Timeline)) Timeline = NewObject<UTimelineComponent>(Template, TEXT("RestoredMantleTimeline"), RF_Public | RF_ArchetypeObject);
				Property->SetObjectPropertyValue_InContainer(Template, Timeline);
				FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(CharacterBP);
				if (!CompileAndSaveAbility(CharacterBP)) return false;
			}
			UE_LOG(LogTemp, Display, TEXT("NG_MANTLE_TIMELINE: %s"), *GetNameSafe(Timeline));
			return true;
		}
		auto* SourceBP = Cast<UBlueprint>(Node->ComponentClass->ClassGeneratedBy);
		if (!SourceBP || !Node->ComponentTemplate) return false;
		const FString CopyPath = TEXT("/Game/Gameplay/Components/BP_ALSMantleExecutor");
		UBlueprint* Copy = nullptr;
		if (FPackageName::DoesPackageExist(CopyPath))
			Copy = LoadObject<UBlueprint>(nullptr, *(CopyPath + TEXT(".BP_ALSMantleExecutor")));
		else
		{
			Copy = Cast<UBlueprint>(FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().DuplicateAsset(
				TEXT("BP_ALSMantleExecutor"), TEXT("/Game/Gameplay/Components"), SourceBP));
			if (!Copy) return false;
			Copy->ParentClass = UNGALSMantleExecutor::StaticClass();
			FBlueprintEditorUtils::RefreshAllNodes(Copy);
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Copy);
			if (!CompileAndSaveAbility(Copy)) return false;
		}
		if (!Copy || !Copy->GeneratedClass || !Copy->GeneratedClass->IsChildOf(UNGALSMantleExecutor::StaticClass())) return false;
		// Preserve the user's Character before replacing only this SCS component class.
		const FString OriginalFile = FPackageName::LongPackageNameToFilename(CharacterBP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
		const FString BackupDir = FPaths::ProjectSavedDir() / TEXT("RefactorBackup/Abilities");
		IFileManager::Get().MakeDirectory(*BackupDir, true);
		const FString BackupFile = BackupDir / (TEXT("BP_NextgenoneCharacter_") + FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")) + TEXT(".uasset"));
		if (IFileManager::Get().Copy(*BackupFile, *OriginalFile, false) != COPY_OK) return false;
		auto* OldTemplate = Node->ComponentTemplate.Get();
		UObject* OriginalOuter = OldTemplate->GetOuter(); const FName OriginalName = OldTemplate->GetFName();
		auto* NewTemplate = NewObject<UActorComponent>(OriginalOuter, Copy->GeneratedClass, NAME_None, RF_Public | RF_ArchetypeObject);
		auto* TimelineProperty = FindFProperty<FObjectPropertyBase>(UALSMantleComponent::StaticClass(), TEXT("MantleTimeline"));
		if (!TimelineProperty) return false;
		UObject* NewTimeline = TimelineProperty->GetObjectPropertyValue_InContainer(NewTemplate);
		UEngine::FCopyPropertiesForUnrelatedObjectsParams CopyParams;
		CopyParams.bDoDelta = false; CopyParams.bClearReferences = false;
		UEngine::CopyPropertiesForUnrelatedObjects(OldTemplate, NewTemplate, CopyParams);
		// Keep the replacement's own nested subobject, not the source template's timeline.
		TimelineProperty->SetObjectPropertyValue_InContainer(NewTemplate, NewTimeline);
		OldTemplate->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
		NewTemplate->Rename(*OriginalName.ToString(), OriginalOuter, REN_DontCreateRedirectors | REN_NonTransactional);
		Node->ComponentClass = Copy->GeneratedClass; Node->ComponentTemplate = NewTemplate;
		FBlueprintEditorUtils::RefreshAllNodes(CharacterBP);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(CharacterBP);
		if (!CompileAndSaveAbility(CharacterBP)) return false;
		UE_LOG(LogTemp, Display, TEXT("NG_MANTLE_ADAPTED: copied component Blueprint; Character graphs, settings and AnimBP retained; backup=%s"), *BackupFile);
		return true;
	}
	UE_LOG(LogTemp, Error, TEXT("NG_MANTLE_ADAPT: expected ALS mantle SCS component was not found; Character asset left unchanged"));
	return false;
}
// Asset creation only: does not load a map, spawn gameplay actors, or start PIE.
int32 CreateAbilityAssets()
{
	bool bCreated;
	UBlueprint* Cooldown = MakeAbilityBlueprint(TEXT("GE_DashCooldown"), UNGDashCooldownEffect::StaticClass(), bCreated);
	if (!Cooldown || (bCreated && !CompileAndSaveAbility(Cooldown))) return 20;
	UBlueprint* Jump = MakeAbilityBlueprint(TEXT("GA_Jump"), UNGJumpAbility::StaticClass(), bCreated);
	if (!Jump || (bCreated && !CompileAndSaveAbility(Jump))) return 21;
	UBlueprint* Dash = MakeAbilityBlueprint(TEXT("GA_Dash"), UNGDashAbility::StaticClass(), bCreated);
	if (!Dash) return 22;
	if (bCreated)
	{
		auto* Property = FindFProperty<FClassProperty>(UGameplayAbility::StaticClass(), TEXT("CooldownGameplayEffectClass"));
		if (!Property) return 23;
		Property->SetPropertyValue_InContainer(Dash->GeneratedClass->GetDefaultObject(), Cooldown->GeneratedClass);
		if (!CompileAndSaveAbility(Dash)) return 24;
	}
	UBlueprint* Mantle = MakeAbilityBlueprint(TEXT("GA_Mantle"), UNGMantleAbility::StaticClass(), bCreated);
	if (!Mantle || (bCreated && !CompileAndSaveAbility(Mantle))) return 25;
	UBlueprint* Pickup = MakeAbilityBlueprint(TEXT("GA_PickupWeapon"), UNGPickupWeaponAbility::StaticClass(), bCreated);
	if (!Pickup || (bCreated && !CompileAndSaveAbility(Pickup))) return 26;
	UBlueprint* Fire = MakeAbilityBlueprint(TEXT("GA_Fire"), UNGFireAbility::StaticClass(), bCreated);
	if (!Fire || (bCreated && !CompileAndSaveAbility(Fire))) return 27;
	const FString SetPath = TEXT("/Game/Gameplay/Abilities/DA_PlayerAbilities");
	if (!FPackageName::DoesPackageExist(SetPath))
	{
		auto* Set = NewObject<UNGAbilitySet>(CreatePackage(*SetPath), TEXT("DA_PlayerAbilities"), RF_Public | RF_Standalone);
		auto Add = [Set](UBlueprint* BP, FGameplayTag Tag) { FNGAbilityGrant Grant; Grant.Ability = BP->GeneratedClass; Grant.InputTag = Tag; Set->Abilities.Add(Grant); };
		Add(Jump, TAG_NGInputJump); Add(Dash, TAG_NGInputDash); Add(Mantle, TAG_NGInputMantle); Add(Pickup, TAG_NGInputPickup);
		if (!SaveAbilityAsset(Set)) return 28;
	}
	const FString WeaponPath = TEXT("/Game/Gameplay/Weapons/DA_TestWeapon");
	auto* Definition = FPackageName::DoesPackageExist(WeaponPath) ? LoadObject<UNGWeaponDefinition>(nullptr, *(WeaponPath + TEXT(".DA_TestWeapon"))) : nullptr;
	if (!Definition)
	{
		Definition = NewObject<UNGWeaponDefinition>(CreatePackage(*WeaponPath), TEXT("DA_TestWeapon"), RF_Public | RF_Standalone);
		Definition->Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		Definition->FireAbilityClass = Fire->GeneratedClass;
		if (!SaveAbilityAsset(Definition)) return 29;
	}
	const FString PickupPath = TEXT("/Game/Gameplay/Weapons/BP_WeaponPickup");
	if (!FPackageName::DoesPackageExist(PickupPath))
	{
		auto* BP = FKismetEditorUtilities::CreateBlueprint(ANGWeaponPickup::StaticClass(), CreatePackage(*PickupPath), TEXT("BP_WeaponPickup"),
			BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), TEXT("NGAbilityAssets"));
		if (!BP) return 30;
		CastChecked<ANGWeaponPickup>(BP->GeneratedClass->GetDefaultObject())->Definition = Definition;
		if (!CompileAndSaveAbility(BP)) return 31;
	}
	TArray<UInputAction*> Actions;
	for (const TCHAR* Name : {TEXT("IA_Dash"), TEXT("IA_Mantle"), TEXT("IA_PickupWeapon")})
	{
		const FString Path = TEXT("/Game/Gameplay/Input/") + FString(Name);
		auto* Action = FPackageName::DoesPackageExist(Path) ? LoadObject<UInputAction>(nullptr, *(Path + TEXT(".") + Name)) : nullptr;
		if (!Action)
		{
			Action = NewObject<UInputAction>(CreatePackage(*Path), FName(Name), RF_Public | RF_Standalone);
			Action->ValueType = EInputActionValueType::Boolean;
			if (!SaveAbilityAsset(Action)) return 32;
		}
		Actions.Add(Action);
	}
	const FString InputPath = TEXT("/Game/Gameplay/Input/IMC_Abilities");
	if (!FPackageName::DoesPackageExist(InputPath))
	{
		auto* Mapping = NewObject<UInputMappingContext>(CreatePackage(*InputPath), TEXT("IMC_Abilities"), RF_Public | RF_Standalone);
		Mapping->MapKey(Actions[0], EKeys::Q); Mapping->MapKey(Actions[1], EKeys::V); Mapping->MapKey(Actions[2], EKeys::E);
		if (!SaveAbilityAsset(Mapping)) return 33;
	}
	if (!AdaptCharacterMantle()) return 34;
	if (const int32 PunchResult = CreatePunchAssets(); PunchResult != 0) return PunchResult;
	UE_LOG(LogTemp, Display, TEXT("NG_ABILITY_ASSETS_SAVED: GA_Jump GA_Dash GA_Mantle GA_PickupWeapon GA_Fire, ability set, input and test weapon"));
	return 0;
}

bool MoveGameplayBlueprint(const TCHAR* OldPath, const TCHAR* NewName, const TCHAR* NewFolder=TEXT("/Game/Blueprints"))
{
	const FString Destination=FString(NewFolder)+TEXT("/")+FString(NewName);
	if (FPackageName::DoesPackageExist(Destination)) return true;
	if (!FPackageName::DoesPackageExist(OldPath)) return true;
	const FString OldObjectPath=FString(OldPath)+TEXT(".")+FPackageName::GetLongPackageAssetName(OldPath);
	UObject* Asset=LoadObject<UObject>(nullptr,*OldObjectPath);
	if (!Asset) return false;
	TArray<FAssetRenameData> Renames;
	Renames.Emplace(Asset,NewFolder,NewName);
	return FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().RenameAssets(Renames);
}

bool CopyTestBlueprint(const TCHAR* Source, const TCHAR* Name, UClass* Parent)
{
	const FString PackagePath=TEXT("/Game/Blueprints/")+FString(Name);
	// Never overwrite an existing test asset or unrelated dirty packages.
	if (FPackageName::DoesPackageExist(PackagePath)) return true;
	auto* Original=LoadObject<UBlueprint>(nullptr,Source);
	if (!Original) return false;
	auto* Copy=Cast<UBlueprint>(FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().DuplicateAsset(Name,TEXT("/Game/Blueprints"),Original));
	if (!Copy) return false;
	Copy->ParentClass=Parent;
	FBlueprintEditorUtils::RefreshAllNodes(Copy);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Copy);
	FKismetEditorUtilities::CompileBlueprint(Copy);
	if (Copy->Status==BS_Error) return false;
	FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone;
	return UPackage::SavePackage(Copy->GetOutermost(),Copy,*FPackageName::LongPackageNameToFilename(PackagePath,FPackageName::GetAssetPackageExtension()),Args);
}
}
int32 UNGSetupCommandlet::Main(const FString& Params)
{
	if (FParse::Param(*Params,TEXT("RemoveALSMenu"))) return NGRemoveALSMenuAssets();
	if (FParse::Param(*Params,TEXT("WeaponVFXOnly")))
	{
		const int32 Result=NGConfigureWeaponVFX(); if (Result!=0) return Result;
		for (const TCHAR* Path:{TEXT("/Game/Gameplay/Weapons/BP_BallisticProjectile.BP_BallisticProjectile"),TEXT("/Game/Gameplay/Abilities/GA_Fire.GA_Fire"),TEXT("/Game/Blueprints/BP_NextgenoneCharacter.BP_NextgenoneCharacter")})
		{
			auto* BP=LoadObject<UBlueprint>(nullptr,Path); if (!BP || !BackupInteractionAsset(BP) || !CompileAndSaveAbility(BP)) return 130;
		}
		return 0;
	}
	if (FParse::Param(*Params,TEXT("ImportWeaponVFX"))) return NGImportWeaponVFX();
	if (FParse::Param(*Params,TEXT("BodyImpactOnly"))) return RefreshBodyImpactAssets();
	if (FParse::Param(*Params,TEXT("BallisticsOnly"))) return CreateBallisticAssets();
	if (FParse::Param(*Params,TEXT("AimOnly"))) return RefreshAimAssets();
	if (FParse::Param(*Params,TEXT("SettingsOnly"))) return NGCreateSettingsAssets();
	if (FParse::Param(*Params,TEXT("InteractionOnly"))) return CreateInteractionAssets();
	if (FParse::Param(*Params, TEXT("LyraWeapons"))) return CreateLyraWeaponAssets();
	if (FParse::Param(*Params, TEXT("PunchOnly"))) return CreatePunchAssets();
	if (FParse::Param(*Params, TEXT("AbilitiesOnly"))) return CreateAbilityAssets();
	if (!MoveGameplayBlueprint(TEXT("/Game/GameplayFoundation/BP_NGCharacter"),TEXT("BP_NextgenoneCharacter"))) return 4;
	if (!MoveGameplayBlueprint(TEXT("/Game/GameplayFoundation/BP_NGController"),TEXT("BP_NextgenonePlayerController"))) return 5;
	if (!CopyTestBlueprint(TEXT("/ALSV4_CPP/AdvancedLocomotionV4/Blueprints/CharacterLogic/ALS_CharacterBP.ALS_CharacterBP"),TEXT("BP_NextgenoneCharacter"),ANextgenoneCharacter::StaticClass())) return 1;
	if (!CopyTestBlueprint(TEXT("/ALSV4_CPP/AdvancedLocomotionV4/Blueprints/CharacterLogic/ALS_Player_Controller.ALS_Player_Controller"),TEXT("BP_NextgenonePlayerController"),ANextgenonePlayerController::StaticClass())) return 2;
	if (FParse::Param(*Params,TEXT("Consolidate")))
	{
		auto* Mode=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Blueprints/BP_NextgenoneGameMode.BP_NextgenoneGameMode"));
		if (!Mode) return 6;
		Mode->ParentClass=ANextgenoneGameMode::StaticClass();
		FBlueprintEditorUtils::RefreshAllNodes(Mode);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Mode);
		FKismetEditorUtilities::CompileBlueprint(Mode);
		if (Mode->Status==BS_Error) return 7;
		auto* Defaults=Cast<ANextgenoneGameMode>(Mode->GeneratedClass->GetDefaultObject());
		if (!Defaults) return 8;
		Defaults->DefaultPawnClass=Defaults->GameplayCharacterClass.LoadSynchronous();
		Defaults->PlayerControllerClass=Defaults->GameplayControllerClass.LoadSynchronous();
		Defaults->PlayerStateClass=ANextgenonePlayerState::StaticClass();
		Defaults->HUDClass=Defaults->GameplayHUDClass.LoadSynchronous();
		FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone;
		if (!UPackage::SavePackage(Mode->GetOutermost(),Mode,*FPackageName::LongPackageNameToFilename(Mode->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args)) return 9;
	}
	if (!MoveGameplayBlueprint(TEXT("/Game/GameplayFoundation/Maps/CombatTest"),TEXT("Gameplay"),TEXT("/Game/Maps"))) return 10;
	const FString MapPath=TEXT("/Game/Maps/Gameplay");
	if (!FPackageName::DoesPackageExist(MapPath))
	{
		UPackage* Package=CreatePackage(*MapPath);
		UWorld* World=UWorld::CreateWorld(EWorldType::Editor,false,TEXT("Gameplay"),Package);
		World->GetWorldSettings()->DefaultGameMode=ANextgenoneGameMode::StaticClass();
		auto* Floor=World->SpawnActor<AStaticMeshActor>();
		Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
		Floor->SetActorLocation(FVector(0,0,-20)); Floor->SetActorScale3D(FVector(30,30,.2));
		for (int32 i=0;i<2;++i) World->SpawnActor<APlayerStart>(FVector(i*600,0,100),FRotator::ZeroRotator);
		World->SpawnActor<ADirectionalLight>(FVector(0,0,500),FRotator(-60,-30,0));
		World->SpawnActor<ASkyLight>();
		const bool bSaved = UEditorLoadingAndSavingUtils::SaveMap(World,MapPath);
		World->DestroyWorld(false);
		if (!bSaved) return 3;
	}
	if (FParse::Param(*Params,TEXT("Consolidate")))
	{
		UWorld* World=LoadObject<UWorld>(nullptr,TEXT("/Game/Maps/Gameplay.Gameplay"));
		UClass* MainMode=LoadClass<AGameModeBase>(nullptr,TEXT("/Game/Blueprints/BP_NextgenoneGameMode.BP_NextgenoneGameMode_C"));
		if (!World || !MainMode) return 11;
		World->GetWorldSettings()->DefaultGameMode=MainMode;
		const bool bSaved=UEditorLoadingAndSavingUtils::SaveMap(World,MapPath);
		World->DestroyWorld(false);
		if (!bSaved) return 12;
		TArray<UObjectRedirector*> Redirectors;
		for (const TCHAR* OldObject : {
			TEXT("/Game/GameplayFoundation/BP_NGCharacter.BP_NGCharacter"),
			TEXT("/Game/GameplayFoundation/BP_NGController.BP_NGController"),
			TEXT("/Game/GameplayFoundation/Maps/CombatTest.CombatTest")})
		{
			if (auto* Redirect=LoadObject<UObjectRedirector>(nullptr,OldObject,nullptr,LOAD_NoRedirects)) Redirectors.Add(Redirect);
		}
		if (!Redirectors.IsEmpty()) FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().FixupReferencers(Redirectors,false);
	}
	UE_LOG(LogTemp,Display,TEXT("NG_SETUP_PASS: one gameplay class chain and Gameplay map saved"));
	return 0;
}
#else
int32 UNGSetupCommandlet::Main(const FString& Params) { return 1; }
#endif
