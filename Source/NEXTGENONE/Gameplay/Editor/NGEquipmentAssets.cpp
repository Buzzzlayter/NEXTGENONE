#if WITH_EDITOR
#include "Gameplay/Equipment/NGEquipmentDefinition.h"
#include "Gameplay/Equipment/NGJetpackDefinition.h"
#include "Gameplay/Combat/NGCombat.h"
#include "Gameplay/Abilities/NGItemActivateAbility.h"
#include "Gameplay/Abilities/NGJetpackAbility.h"
#include "Gameplay/Player/NextgenonePlayerState.h"
#include "Gameplay/Character/NextgenoneCharacter.h"
#include "Gameplay/Character/NGCharacterMovementComponent.h"
#include "Inventory/NGInventoryItemDefinition.h"
#include "Inventory/NGInventoryFragment_ItemActions.h"
#include "Inventory/NGInventoryLayoutDefinition.h"
#include "Input/NGGameplayInputConfig.h"
#include "Character/NGControlStateDefinition.h"
#include "GameplayTags/NGGameplayTags.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "UObject/SavePackage.h"

/** Explicit, bounded offline authoring after the user's full build. Never called by runtime. */
int32 NGMigrateEquipmentAssets()
{
	const FString BackupRoot = FPaths::ProjectSavedDir() / TEXT("RefactorBackup/Equipment") / FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	IFileManager::Get().MakeDirectory(*BackupRoot, true);
	TArray<UObject*> ToSave;
	TArray<FString> Manifest;
	auto Load = [](const TCHAR* Package) -> UObject*
	{
		const FString P = Package; if (!FPackageName::DoesPackageExist(P)) return nullptr; return LoadObject<UObject>(nullptr, *(P + TEXT(".") + FPackageName::GetLongPackageAssetName(P)));
	};
	auto Prepare = [&](UObject* Asset) -> bool
	{
		if (!Asset) return false;
		if (ToSave.Contains(Asset)) return true;
		const FString Package = Asset->GetOutermost()->GetName();
		const FString File = FPackageName::LongPackageNameToFilename(Package, TEXT(".uasset"));
		if (IFileManager::Get().FileExists(*File))
		{
			const FString Backup = BackupRoot / Package.RightChop(6) + TEXT(".uasset");
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true);
			if (IFileManager::Get().Copy(*Backup, *File, false) != COPY_OK) return false;
		}
		Asset->Modify(); ToSave.Add(Asset); Manifest.Add(Package); return true;
	};
	auto CreateAsset = [&](UClass* Class, const TCHAR* Package) -> UObject*
	{
		if (auto* Existing = Load(Package)) return Existing->IsA(Class) ? Existing : nullptr;
		const FString P = Package;
		auto* Asset = NewObject<UObject>(CreatePackage(*P), Class, *FPackageName::GetLongPackageAssetName(P), RF_Public | RF_Standalone);
		FAssetRegistryModule::AssetCreated(Asset); return Asset;
	};
	auto* Layout = Cast<UNGInventoryLayoutDefinition>(Load(TEXT("/Game/Gameplay/Inventory/Layouts/DA_InventoryLayout_Player")));
	auto* Input = Cast<UNGGameplayInputConfig>(Load(TEXT("/Game/Gameplay/Input/DA_Input_Character")));
	auto* Alive = Cast<UNGControlStateDefinition>(Load(TEXT("/Game/Gameplay/States/DA_CharacterState_Alive")));
	auto* PlayerBP = Cast<UBlueprint>(Load(TEXT("/Game/Blueprints/BP_NextgenonePlayerState")));
	auto* PawnBP = Cast<UBlueprint>(Load(TEXT("/Game/Blueprints/BP_NextgenoneCharacter")));
	if (!Layout || !Input || !Alive || !PlayerBP || !PawnBP || !PlayerBP->GeneratedClass || !PawnBP->GeneratedClass || !Prepare(Layout) || !Prepare(Input) || !Prepare(Alive) || !Prepare(PlayerBP) || !Prepare(PawnBP)) return 210;
	for (const TCHAR* Name : {TEXT("DA_Pistol"), TEXT("DA_Rifle"), TEXT("DA_Shotgun"), TEXT("DA_TestWeapon")})
	{
		const FString Package = TEXT("/Game/Gameplay/Weapons/") + FString(Name);
		auto* Weapon = Cast<UNGWeaponDefinition>(Load(*Package));
		if (!Weapon || !Prepare(Weapon)) return 211;
		// Old AttachSocket was ignored; retain the actual ALS placement for the legacy default.
		if (Weapon->AttachSocket == TEXT("hand_r") || Weapon->AttachSocket.IsNone()) Weapon->AttachSocket = TEXT("VB RHS_ik_hand_gun");
	}
	const FString ItemPackage = TEXT("/Game/Gameplay/Inventory/ID_Jetpack");
	auto* Jetpack = Cast<UNGJetpackDefinition>(CreateAsset(UNGJetpackDefinition::StaticClass(), TEXT("/Game/Gameplay/Equipment/ED_Jetpack")));
	if (!Jetpack || !Prepare(Jetpack)) return 212;
	// Mesh remains an authored choice. Existing definitions/settings are preserved on rerun.
	auto* ItemBP = Cast<UBlueprint>(Load(*ItemPackage));
	if (!ItemBP)
	{
		ItemBP = FKismetEditorUtilities::CreateBlueprint(UNGInventoryItemDefinition::StaticClass(), CreatePackage(*ItemPackage), TEXT("ID_Jetpack"), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
		if (!ItemBP) return 213;
		FAssetRegistryModule::AssetCreated(ItemBP);
	}
	if (!Prepare(ItemBP) || !ItemBP->GeneratedClass || !ItemBP->GeneratedClass->IsChildOf(UNGInventoryItemDefinition::StaticClass())) return 214;
	auto* Item = GetMutableDefault<UNGInventoryItemDefinition>(ItemBP->GeneratedClass);
	Item->Modify(); Item->MaxStackSize = 1; Item->ItemTags.AddTag(TAG_NGItemEquipmentBack);
	if (Item->DisplayName.IsEmpty()) Item->DisplayName = FText::FromString(TEXT("Jetpack"));
	auto* EquipmentFragment = const_cast<UNGInventoryFragment_Equipment*>(Cast<UNGInventoryFragment_Equipment>(Item->FindFragmentByClass(UNGInventoryFragment_Equipment::StaticClass())));
	if (!EquipmentFragment) { EquipmentFragment = NewObject<UNGInventoryFragment_Equipment>(Item); Item->Fragments.Add(EquipmentFragment); }
	if (EquipmentFragment->Definition && EquipmentFragment->Definition != Jetpack) return 215;
	EquipmentFragment->Definition = Jetpack;
	auto* Actions = const_cast<UNGInventoryFragment_ItemActions*>(Cast<UNGInventoryFragment_ItemActions>(Item->FindFragmentByClass(UNGInventoryFragment_ItemActions::StaticClass())));
	if (!Actions) { Actions = NewObject<UNGInventoryFragment_ItemActions>(Item); Item->Fragments.Add(Actions); }
	if (!Actions->ActivationAbility) Actions->ActivationAbility = UNGEquipWeaponItemAbility::StaticClass();
	for (const auto& Pair : {TPair<FGameplayTag, FGameplayTag>(TAG_NGSlotEquipmentBack, TAG_NGItemEquipmentBack),
		TPair<FGameplayTag, FGameplayTag>(TAG_NGSlotEquipmentFeet, TAG_NGItemEquipmentFeet), TPair<FGameplayTag, FGameplayTag>(TAG_NGSlotEquipmentTorso, TAG_NGItemEquipmentTorso)})
	{
		// Append only: never renumber existing Storage/Action slots or overwrite an authored filter.
		if (!Layout->Slots.ContainsByPredicate([&](const auto& Slot) { return Slot.SlotTag == Pair.Key; }))
		{ FNGInventorySlotDefinition Slot; Slot.SlotTag = Pair.Key; Slot.AcceptedItems = FGameplayTagQuery::MakeQuery_MatchTag(Pair.Value); Layout->Slots.Add(Slot); }
	}
	Alive->Abilities.RemoveAll([](const FNGAbilityGrant& Grant) { return Grant.Ability && Grant.Ability->IsChildOf(UNGJetpackAbility::StaticClass()); });
	auto* Action = Cast<UInputAction>(CreateAsset(UInputAction::StaticClass(), TEXT("/Game/Gameplay/Input/Configured/IA_NG_Jetpack")));
	auto* Context = Cast<UInputMappingContext>(CreateAsset(UInputMappingContext::StaticClass(), TEXT("/Game/Gameplay/Input/Configured/IMC_NG_Jetpack")));
	if (!Action || !Context || !Prepare(Action) || !Prepare(Context)) return 216;
	Action->ValueType = EInputActionValueType::Boolean;
	Input->NativeInputActions.RemoveAll([&](const auto& Route) { return Route.InputTag == TAG_NGInputJetpack || Route.InputAction == Action; });
	Input->AbilityInputActions.RemoveAll([&](const auto& Route) { return Route.InputTag == TAG_NGInputJetpack || Route.InputAction == Action; });
	FNGTaggedInputAction Route; Route.InputTag = TAG_NGInputJetpack; Route.InputAction = Action; Input->AbilityInputActions.Add(Route);
	bool bHasJetpackMapping = false;
	for (const auto& Entry : Alive->MappingContexts) if (Entry.MappingContext)
		for (const auto& Mapping : Entry.MappingContext->GetMappings()) if (Mapping.Action == Action) bHasJetpackMapping = true;
	if (!bHasJetpackMapping && Context->GetMappings().IsEmpty())
	{
		for (const auto Key : {EKeys::J, EKeys::Gamepad_RightThumbstick})
		{
			bool Used = false;
			for (const auto& Entry : Alive->MappingContexts) if (Entry.MappingContext)
				for (const auto& Mapping : Entry.MappingContext->GetMappings()) if (Mapping.Key == Key) Used = true;
			if (!Used) Context->MapKey(Action, Key);
			else UE_LOG(LogTemp, Warning, TEXT("Equipment: key %s already mapped; assign a free jetpack key in IMC."), *Key.ToString());
		}
	}
	if (!bHasJetpackMapping && !Alive->MappingContexts.ContainsByPredicate([&](const auto& Entry) { return Entry.MappingContext == Context; }))
	{ FNGInputMappingContextEntry Entry; Entry.MappingContext = Context; Entry.Priority = 25; Alive->MappingContexts.Add(Entry); }
	auto* PS = GetMutableDefault<ANextgenonePlayerState>(PlayerBP->GeneratedClass);
	if (!PS || !PS->Inventory) return 217;
	PS->Inventory->Modify();
	if (!PS->Inventory->InitialContents.ContainsByPredicate([&](const auto& Entry) { return Entry.ItemClass == ItemBP->GeneratedClass; }))
	{ FNGInventoryStarterEntry Entry; Entry.ItemClass = ItemBP->GeneratedClass; Entry.Count = 1; Entry.SlotTag = TAG_NGSlotStorage; PS->Inventory->InitialContents.Add(Entry); }
	// Compile/resave only these three Blueprints; no map, UI layout, AnimBP or plugin assets.
	for (auto* BP : {ItemBP, PlayerBP, PawnBP})
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
		FKismetEditorUtilities::CompileBlueprint(BP);
		if (BP->Status == BS_Error) return 218;
	}
	const auto* Pawn = GetDefault<ANextgenoneCharacter>(PawnBP->GeneratedClass);
	if (!Pawn || !Pawn->Equipment || !Cast<UNGCharacterMovementComponent>(Pawn->GetCharacterMovement())) return 219;
	for (auto* Asset : ToSave)
	{
		const FString File = FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), TEXT(".uasset"));
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
		FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
		if (!UPackage::SavePackage(Asset->GetOutermost(), Asset, *File, Args)) return 220;
	}
	FFileHelper::SaveStringArrayToFile(Manifest, *(BackupRoot / TEXT("Packages.txt")));
	UE_LOG(LogTemp, Display, TEXT("Equipment migration saved %d assets; backup=%s. No gameplay checks performed."), ToSave.Num(), *BackupRoot);
	return 0;
}

/** Weapon-slot migration only: no starter grants, input, Pawn, map or jetpack changes. */
int32 NGMigrateWeaponSlotAssets()
{
	const FString BackupRoot = FPaths::ProjectSavedDir() / TEXT("RefactorBackup/WeaponSlot") / FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	TArray<UObject*> Assets;
	TArray<UNGWeaponDefinition*> Weapons;
	TArray<UBlueprint*> Items;
	auto Load = [](const FString& Package) -> UObject*
	{
		return LoadObject<UObject>(nullptr, *(Package + TEXT(".") + FPackageName::GetLongPackageAssetName(Package)));
	};
	auto* Layout = Cast<UNGInventoryLayoutDefinition>(Load(TEXT("/Game/Gameplay/Inventory/Layouts/DA_InventoryLayout_Player")));
	if (!Layout) return 220;
	Assets.Add(Layout);
	for (const auto& Pair : {
		TPair<const TCHAR*, const TCHAR*>(TEXT("DA_Pistol"), TEXT("ID_Pistol")),
		TPair<const TCHAR*, const TCHAR*>(TEXT("DA_Rifle"), TEXT("ID_Rifle")),
		TPair<const TCHAR*, const TCHAR*>(TEXT("DA_Shotgun"), TEXT("ID_Shotgun")),
		TPair<const TCHAR*, const TCHAR*>(TEXT("DA_TestWeapon"), TEXT("ID_TestItem"))})
	{
		auto* Weapon = Cast<UNGWeaponDefinition>(Load(FString(TEXT("/Game/Gameplay/Weapons/")) + Pair.Key));
		auto* ItemBP = Cast<UBlueprint>(Load(FString(TEXT("/Game/Gameplay/Inventory/")) + Pair.Value));
		if (!Weapon || !ItemBP || !ItemBP->GeneratedClass || !ItemBP->GeneratedClass->IsChildOf(UNGInventoryItemDefinition::StaticClass())) return 221;
		const auto* Item = GetDefault<UNGInventoryItemDefinition>(ItemBP->GeneratedClass);
		if (Item->GetEquipmentDefinition() != Weapon) return 222;
		Weapons.Add(Weapon); Items.Add(ItemBP); Assets.Add(Weapon); Assets.Add(ItemBP);
	}
	TArray<FString> Manifest;
	for (auto* Asset : Assets)
	{
		const FString Package = Asset->GetOutermost()->GetName();
		const FString File = FPackageName::LongPackageNameToFilename(Package, TEXT(".uasset"));
		const FString Backup = BackupRoot / Package.RightChop(6) + TEXT(".uasset");
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true);
		if (IFileManager::Get().Copy(*Backup, *File, false) != COPY_OK) return 223;
		Manifest.Add(Package);
	}
	FFileHelper::SaveStringArrayToFile(Manifest, *(BackupRoot / TEXT("Packages.txt")));
	Layout->Modify();
	auto* WeaponSlot = Layout->Slots.FindByPredicate([](const auto& Slot) { return Slot.SlotTag == TAG_NGSlotEquipmentWeapon; });
	if (!WeaponSlot)
	{
		WeaponSlot = &Layout->Slots.AddDefaulted_GetRef();
		WeaponSlot->SlotTag = TAG_NGSlotEquipmentWeapon;
	}
	WeaponSlot->AcceptedItems = FGameplayTagQuery::MakeQuery_MatchTag(TAG_NGItemEquipmentWeapon);
	for (int32 I = 0; I < Weapons.Num(); ++I)
	{
		Weapons[I]->Modify(); Weapons[I]->SlotTag = TAG_NGSlotEquipmentWeapon;
		Items[I]->Modify();
		auto* Item = GetMutableDefault<UNGInventoryItemDefinition>(Items[I]->GeneratedClass);
		Item->Modify(); Item->MaxStackSize = 1; Item->ItemTags.AddTag(TAG_NGItemEquipmentWeapon);
		FBlueprintEditorUtils::MarkBlueprintAsModified(Items[I]);
	}
	for (auto* Asset : Assets)
	{
		const FString File = FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), TEXT(".uasset"));
		FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
		if (!UPackage::SavePackage(Asset->GetOutermost(), Asset, *File, Args)) return 224;
	}
	UE_LOG(LogTemp, Display, TEXT("Weapon slot migration saved %d assets; original slots retained, no starter changes; backup=%s"), Assets.Num(), *BackupRoot);
	return 0;
}
#endif
