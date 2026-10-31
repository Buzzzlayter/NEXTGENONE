#include "NGSetupCommandlet.h"

UNGSetupCommandlet::UNGSetupCommandlet() { IsClient=false; IsServer=false; IsEditor=true; LogToConsole=true; }

#if WITH_EDITOR
#include "NextgenoneCharacter.h"
#include "NextgenonePlayerController.h"
#include "NextgenonePlayerState.h"
#include "NextgenoneGameMode.h"
#include "NGTestGameMode.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Engine/Blueprint.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerStart.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "FileHelpers.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/ObjectRedirector.h"

namespace
{
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
