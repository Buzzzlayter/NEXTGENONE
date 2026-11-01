#include "NextgenonePlayerController.h"
#include "NextgenoneCharacter.h"
#include "NGCombat.h"
#include "NGTestGameMode.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "EngineUtils.h"
#include "NGAbilitySystem.h"
#include "NextgenonePlayerState.h"
#include "NGInteraction.h"
#include "NGSettingsPanel.h"

void ANextgenonePlayerController::BeginPlay() { Super::BeginPlay(); if (IsLocalController()) GetDefault<UNGInputPreferences>()->ApplyAudio(GetWorld()); }

void ANextgenonePlayerController::AddYawInput(float Value) { Super::AddYawInput(Value*GetDefault<UNGInputPreferences>()->LookSensitivity); }
void ANextgenonePlayerController::AddPitchInput(float Value)
{
	const auto* P=GetDefault<UNGInputPreferences>(); Super::AddPitchInput(Value*P->LookSensitivity*(P->bInvertY ? -1.f : 1.f));
}

ANextgenonePlayerController::ANextgenonePlayerController()
{
	AbilityMappingContext = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(TEXT("/Game/Gameplay/Input/IMC_Abilities.IMC_Abilities")));
	DashInput = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Gameplay/Input/IA_Dash.IA_Dash")));
	MantleInput = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Gameplay/Input/IA_Mantle.IA_Mantle")));
	PickupInput = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Gameplay/Input/IA_PickupWeapon.IA_PickupWeapon")));
}

void ANextgenonePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	auto* Input = Cast<UEnhancedInputComponent>(InputComponent);
	if (!Input) return;
	if (!CombatMapping)
	{
		UInputMappingContext* Config = AbilityMappingContext.LoadSynchronous();
		CombatMapping = Config ? DuplicateObject<UInputMappingContext>(Config, this) : NewObject<UInputMappingContext>(this);
		FireAction = NewObject<UInputAction>(this); FireAction->ValueType = EInputActionValueType::Boolean;
		EquipAction = NewObject<UInputAction>(this); EquipAction->ValueType = EInputActionValueType::Boolean;
		UnequipAction = NewObject<UInputAction>(this); UnequipAction->ValueType = EInputActionValueType::Boolean;
		CombatMapping->MapKey(FireAction, EKeys::LeftMouseButton);
		CombatMapping->MapKey(EquipAction, EKeys::F);
		CombatMapping->MapKey(UnequipAction, EKeys::U);
		PistolAction = NewObject<UInputAction>(this); RifleAction = NewObject<UInputAction>(this); ShotgunAction=NewObject<UInputAction>(this); CombatMapping->MapKey(ShotgunAction,EKeys::Three);
		AimWeaponAction = NewObject<UInputAction>(this); ReloadAction = NewObject<UInputAction>(this);
		InventoryAction = NewObject<UInputAction>(this); CombatMapping->MapKey(InventoryAction, EKeys::I);
		CombatMapping->MapKey(PistolAction, EKeys::One); CombatMapping->MapKey(RifleAction, EKeys::Two);
		CombatMapping->MapKey(AimWeaponAction, EKeys::RightMouseButton); CombatMapping->MapKey(ReloadAction, EKeys::R);
		DashAction = DashInput.LoadSynchronous(); MantleAction = MantleInput.LoadSynchronous(); PickupAction = PickupInput.LoadSynchronous();
		if (!DashAction) { DashAction = NewObject<UInputAction>(this); CombatMapping->MapKey(DashAction, EKeys::Q); }
		if (!MantleAction) { MantleAction = NewObject<UInputAction>(this); CombatMapping->MapKey(MantleAction, EKeys::V); }
		if (!PickupAction) { PickupAction = NewObject<UInputAction>(this); CombatMapping->MapKey(PickupAction, EKeys::E); }
	}
	Input->BindAction(FireAction, ETriggerEvent::Started, this, &ThisClass::PrimaryPressed);
	Input->BindAction(FireAction, ETriggerEvent::Completed, this, &ThisClass::PrimaryReleased);
	Input->BindAction(FireAction, ETriggerEvent::Canceled, this, &ThisClass::PrimaryReleased);
	Input->BindAction(PistolAction, ETriggerEvent::Started, this, &ThisClass::NGPistol);
	Input->BindAction(RifleAction, ETriggerEvent::Started, this, &ThisClass::NGRifle);
	Input->BindAction(ShotgunAction,ETriggerEvent::Started,this,&ThisClass::NGShotgun);
	Input->BindAction(ReloadAction, ETriggerEvent::Started, this, &ThisClass::NGReload);
	Input->BindAction(InventoryAction, ETriggerEvent::Started, this, &ThisClass::RequestInventoryUI);
	Input->BindAction(AimWeaponAction, ETriggerEvent::Started, this, &ThisClass::AimPressed);
	Input->BindAction(AimWeaponAction, ETriggerEvent::Completed, this, &ThisClass::AimReleased);
	Input->BindAction(AimWeaponAction, ETriggerEvent::Canceled, this, &ThisClass::AimReleased);
	Input->BindAction(EquipAction, ETriggerEvent::Started, this, &ThisClass::NGEquip);
	Input->BindAction(UnequipAction, ETriggerEvent::Started, this, &ThisClass::NGUnequip);
	Input->BindAction(DashAction, ETriggerEvent::Started, this, &ThisClass::DashPressed);
	Input->BindAction(DashAction, ETriggerEvent::Completed, this, &ThisClass::DashReleased);
	Input->BindAction(DashAction, ETriggerEvent::Canceled, this, &ThisClass::DashReleased);
	Input->BindAction(MantleAction, ETriggerEvent::Started, this, &ThisClass::MantlePressed);
	Input->BindAction(MantleAction, ETriggerEvent::Completed, this, &ThisClass::MantleReleased);
	Input->BindAction(MantleAction, ETriggerEvent::Canceled, this, &ThisClass::MantleReleased);
	Input->BindAction(PickupAction, ETriggerEvent::Started, this, &ThisClass::PickupPressed);
	Input->BindAction(PickupAction, ETriggerEvent::Completed, this, &ThisClass::PickupReleased);
	Input->BindAction(PickupAction, ETriggerEvent::Canceled, this, &ThisClass::PickupReleased);
	UpdateGameplayPawn();
}
void ANextgenonePlayerController::UpdateGameplayPawn()
{
	PrimaryReleased();
	if (auto* ControlledPawn = Cast<ANextgenoneCharacter>(GetPawn())) ControlledPawn->TryInitializeGameplay();
	if (CombatMapping && IsLocalController())
		if (auto* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer())) Sub->AddMappingContext(CombatMapping, 2);
}
void ANextgenonePlayerController::RemoveCombatMapping()
{
	if (auto* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		if (CombatMapping) Sub->RemoveMappingContext(CombatMapping);
}
void ANextgenonePlayerController::OnPossess(APawn* ControlledPawn) { Super::OnPossess(ControlledPawn); UpdateGameplayPawn(); }
void ANextgenonePlayerController::OnRep_Pawn() { Super::OnRep_Pawn(); if (GetPawn()) UpdateGameplayPawn(); else RemoveCombatMapping(); }
void ANextgenonePlayerController::EndPlay(const EEndPlayReason::Type Reason) { PrimaryReleased(); RemoveCombatMapping(); Super::EndPlay(Reason); }
void ANextgenonePlayerController::NGEquip() { if (auto* ControlledPawn = Cast<ANextgenoneCharacter>(GetPawn())) ControlledPawn->Combat->RequestEquip(true); }
void ANextgenonePlayerController::NGUnequip() { PrimaryReleased(); if (auto* ControlledPawn = Cast<ANextgenoneCharacter>(GetPawn())) ControlledPawn->Combat->RequestEquip(false); }
void ANextgenonePlayerController::NGPistol() { PrimaryReleased(); if (auto* P = Cast<ANextgenoneCharacter>(GetPawn())) P->Combat->RequestSelectSlot(0); }
void ANextgenonePlayerController::NGRifle() { PrimaryReleased(); if (auto* P = Cast<ANextgenoneCharacter>(GetPawn())) P->Combat->RequestSelectSlot(1); }
void ANextgenonePlayerController::NGShotgun() { PrimaryReleased(); if (auto* P=Cast<ANextgenoneCharacter>(GetPawn())) P->Combat->RequestSelectSlot(2); }
void ANextgenonePlayerController::NGReload() { if (bInventoryUIOpen || bSettingsUIOpen) return; if (auto* P = Cast<ANextgenoneCharacter>(GetPawn())) P->Combat->RequestReload(); }
void ANextgenonePlayerController::RequestInventoryUI()
{
	PrimaryReleased(); AimReleased();
	if (const auto* PS = GetPlayerState<ANextgenonePlayerState>()) { InventoryUIRequested.Broadcast(PS->Inventory); OnInventoryRequested(PS->Inventory); }
}
void ANextgenonePlayerController::AimPressed() { if (bInventoryUIOpen || bSettingsUIOpen) return; if (auto* P = Cast<ANextgenoneCharacter>(GetPawn())) P->Combat->RequestAim(true); }
void ANextgenonePlayerController::AimReleased() { if (auto* P = Cast<ANextgenoneCharacter>(GetPawn())) P->Combat->RequestAim(false); }
void ANextgenonePlayerController::PrimaryPressed()
{
	if (bInventoryUIOpen || bSettingsUIOpen) return;
	NGFire(); GetWorldTimerManager().SetTimer(AutoFireTimer, this, &ThisClass::ContinueAutomaticFire, .05f, true);
}
void ANextgenonePlayerController::PrimaryReleased() { GetWorldTimerManager().ClearTimer(AutoFireTimer); }
void ANextgenonePlayerController::ContinueAutomaticFire()
{
	const auto* P = Cast<ANextgenoneCharacter>(GetPawn());
	const auto State = P ? P->Combat->GetEquipment() : FNGEquipmentState();
	if (!P || P->IsDead()) { PrimaryReleased(); return; }
	if (State.bEquipped && State.Definition && State.Definition->bAutomatic) P->Combat->RequestFire();
}
void ANextgenonePlayerController::NGFire()
{
	if (auto* ControlledPawn = Cast<ANextgenoneCharacter>(GetPawn()))
	{
		if (ControlledPawn->Combat->GetEquipment().bEquipped) ControlledPawn->Combat->RequestFire();
		else if (auto* ASC = Cast<UNGAbilitySystemComponent>(ControlledPawn->GetAbilitySystemComponent())) ASC->TryActivateAbilityByInputTag(TAG_NGInputPunch);
	}
}
void ANextgenonePlayerController::RouteAbilityInput(FGameplayTag Tag, bool bPressed)
{
	if (bPressed && (bInventoryUIOpen || bSettingsUIOpen)) return;
	if (auto* Controlled = Cast<ANextgenoneCharacter>(GetPawn()))
		if (auto* ASC = Cast<UNGAbilitySystemComponent>(Controlled->GetAbilitySystemComponent()))
			if (bPressed) ASC->AbilityInputTagPressed(Tag); else ASC->AbilityInputTagReleased(Tag);
}
void ANextgenonePlayerController::DashPressed() { RouteAbilityInput(TAG_NGInputDash, true); }
void ANextgenonePlayerController::DashReleased() { RouteAbilityInput(TAG_NGInputDash, false); }
void ANextgenonePlayerController::MantlePressed() { RouteAbilityInput(TAG_NGInputMantle, true); }
void ANextgenonePlayerController::MantleReleased() { RouteAbilityInput(TAG_NGInputMantle, false); }
void ANextgenonePlayerController::PickupPressed() { RouteAbilityInput(TAG_NGInputInteract, true); }
void ANextgenonePlayerController::PickupReleased() {}
void ANextgenonePlayerController::RequestDropItem(UNGInventoryItemInstance* Item,int32 Count)
{
	if (!IsLocalController() && !HasAuthority()) return;
	auto* Expected=Cast<ANextgenoneCharacter>(GetPawn());
	if (HasAuthority()) ServerDropItem_Implementation(Expected,Item,Count); else ServerDropItem(Expected,Item,Count);
}
void ANextgenonePlayerController::ServerDropItem_Implementation(ANextgenoneCharacter* ExpectedPawn,UNGInventoryItemInstance* Item,int32 Count)
{
	if (!ExpectedPawn || GetPawn()!=ExpectedPawn) return;
	auto* System=GetWorld()->GetSubsystem<UNGInteractionSubsystem>();
	auto* PS=GetPlayerState<ANextgenonePlayerState>();
	if (!System || !System->IsCurrentInteractor(ExpectedPawn) || !PS) return;
	const double Now=GetWorld()->GetTimeSeconds(); if (Now<PS->NextAllowedDropTime) return;
	PS->NextAllowedDropTime=Now+.15; System->DropInventoryItem(ExpectedPawn,Item,Count);
}
void ANextgenonePlayerController::NGDash() { DashPressed(); DashReleased(); }
void ANextgenonePlayerController::NGMantle() { MantlePressed(); MantleReleased(); }
void ANextgenonePlayerController::NGPickupWeapon() { PickupPressed(); PickupReleased(); }
void ANextgenonePlayerController::ClientSmoke_Implementation(int32 Command)
{
#if !UE_BUILD_SHIPPING
	if (!FParse::Param(FCommandLine::Get(), TEXT("NGCombatSmoke"))) return;
	auto* ControlledPawn = Cast<ANextgenoneCharacter>(GetPawn());
	if ((Command == 0 || Command == 6) && (!ControlledPawn || !ControlledPawn->GetAbilitySystemComponent()))
	{
		// Controller RPC can precede Pawn/PlayerState replication on a new connection.
		FTimerHandle Retry;
		GetWorldTimerManager().SetTimer(Retry, FTimerDelegate::CreateWeakLambda(this, [this, Command]() { ClientSmoke_Implementation(Command); }), .2f, false);
		return;
	}
	if (!ControlledPawn) { ServerSmokeResult(Command, false); return; }
	if (Command == 0 || Command == 6)
	{
		auto* ASC = ControlledPawn->GetAbilitySystemComponent();
		ServerSmokeResult(Command, ASC && ASC->GetOwnerActor() == PlayerState && ASC->GetAvatarActor() == ControlledPawn && IsLocalController() && ControlledPawn->GetHealth()==100.f);
	}
	else if (Command == 1) { SetControlRotation(FRotator::ZeroRotator); NGEquip(); }
	else if (Command == 2)
	{
		// Actual client GAS activation requests, interleaved with equip RPCs.
		for (int32 i = 0; i < 8; ++i) { NGFire(); NGUnequip(); NGEquip(); }
		NGFire();
	}
	else if (Command == 3) NGFire();
	else
	{
		bool bFoundState = false;
		for (TActorIterator<ANextgenoneCharacter> It(GetWorld()); It; ++It)
		{
			if (It->IsDead()) { bFoundState |= Command == 5 && It->GetHealth() == 0.f; }
			else if (It->GetHealth() == 75.f) bFoundState |= Command == 4;
		}
		// All clients must also see an equipped shooter; only owner sees ammo.
		bool bEquippedSeen = false;
		for (TActorIterator<ANextgenoneCharacter> It(GetWorld()); It; ++It) bEquippedSeen |= It->Combat->GetEquipment().bEquipped;
		const bool bOwnerAmmoCorrect = !ControlledPawn->Combat->GetEquipment().bEquipped || ControlledPawn->Combat->GetMagazineAmmo() == (Command == 4 ? 11 : 8);
		ServerSmokeResult(Command, bFoundState && bEquippedSeen && bOwnerAmmoCorrect);
	}
#endif
}
void ANextgenonePlayerController::ServerSmokeResult_Implementation(int32 Command, bool bPassed)
{
#if !UE_BUILD_SHIPPING
	if (auto* Mode = GetWorld()->GetAuthGameMode<ANGCombatTestGameMode>()) Mode->RecordSmoke(this, Command, bPassed);
#endif
}
