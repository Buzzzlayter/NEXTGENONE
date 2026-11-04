#include "Gameplay/Character/NextgenoneCharacter.h"
#include "Gameplay/Input/NGCharacterInputComponent.h"
#include "Gameplay/StateMachine/NGCharacterStateMachineComponent.h"
#include "Gameplay/Player/NextgenonePlayerState.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "Gameplay/Combat/NGCombat.h"
#include "Gameplay/Interaction/NGInteraction.h"
#include "Gameplay/Combat/NGAimComponent.h"
#include "Gameplay/Character/NGHitReactionComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"


ANextgenoneCharacter::ANextgenoneCharacter(const FObjectInitializer& Init) : Super(Init)
{
	bReplicates = true;
	Combat = CreateDefaultSubobject<UNGCombatComponent>(TEXT("Combat"));
	Interaction = CreateDefaultSubobject<UNGInteractionComponent>(TEXT("Interaction"));
	Aim = CreateDefaultSubobject<UNGAimComponent>(TEXT("Aim"));
	HitReaction = CreateDefaultSubobject<UNGHitReactionComponent>(TEXT("HitReaction"));
	GameplayInput = CreateDefaultSubobject<UNGCharacterInputComponent>(TEXT("GameplayInput"));
	CharacterStateMachine = CreateDefaultSubobject<UNGCharacterStateMachineComponent>(TEXT("CharacterStateMachine"));
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}
UAbilitySystemComponent* ANextgenoneCharacter::GetAbilitySystemComponent() const
{
	UAbilitySystemComponent* ASC = BoundASC.Get();
	return ASC && ASC->GetAvatarActor() == this ? ASC : nullptr;
}
bool ANextgenoneCharacter::IsDead() const
{
    return CharacterStateMachine && CharacterStateMachine->HasStateTag(TAG_NGDead);
}
float ANextgenoneCharacter::GetHealth() const
{
	const auto* ASC = GetAbilitySystemComponent();
	return !IsDead() && ASC ? ASC->GetNumericAttribute(UNGHealthSet::GetHealthAttribute()) : 0.f;
}
float ANextgenoneCharacter::GetMaxHealth() const
{
	const auto* ASC = GetAbilitySystemComponent();
	return ASC ? ASC->GetNumericAttribute(UNGHealthSet::GetMaxHealthAttribute()) : 0.f;
}
void ANextgenoneCharacter::TryInitializeGameplay()
{
	ANextgenonePlayerState* PS = GetPlayerState<ANextgenonePlayerState>();
	if (!PS || IsDead() || (PS->GetPawn() && PS->GetPawn() != this)) return;
	auto* ASC = PS->GetAbilitySystemComponent();
	if (!ASC) return;
	// A delayed OnRep from an old Pawn must not steal a newer avatar.
	if (!HasAuthority() && ASC->GetAvatarActor() && ASC->GetAvatarActor() != this)
	{
		const auto* Old = Cast<ANextgenoneCharacter>(ASC->GetAvatarActor());
		if (PS->GetPawn() != this && Old && !Old->IsDead()) return;
	}
	if (BoundASC == ASC && ASC->GetAvatarActor() == this) { ASC->RefreshAbilityActorInfo(); CharacterStateMachine->NotifyGameplayReady(ASC); return; }
	UnbindGameplay();
	if (HasAuthority()) { if (!PS->StartLife(this)) return; }
	else ASC->InitAbilityActorInfo(PS, this);
	BoundASC = ASC;
	Combat->BindAbilitySystem(ASC);
	Interaction->BindAbilitySystem(ASC);
	Combat->BindInventory();
	HealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(UNGHealthSet::GetHealthAttribute()).AddUObject(this, &ThisClass::HealthChanged);
	MaxHealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(UNGHealthSet::GetMaxHealthAttribute()).AddUObject(this, &ThisClass::HealthChanged);
	// Startup effects run before delegate binding; observe their final health, including a lethal result.
	FOnAttributeChangeData InitialHealth;
	HealthChanged(InitialHealth);
	CharacterStateMachine->NotifyGameplayReady(ASC);

	UE_LOG(LogTemp, Display, TEXT("NG_ASC_READY authority=%d pawn=%s owner=%s avatar=%s"), HasAuthority(), *GetName(), *GetNameSafe(ASC->GetOwnerActor()), *GetNameSafe(ASC->GetAvatarActor()));
}
void ANextgenoneCharacter::UnbindGameplay()
{
	if (auto* ASC = BoundASC.Get())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UNGHealthSet::GetHealthAttribute()).Remove(HealthHandle);
		ASC->GetGameplayAttributeValueChangeDelegate(UNGHealthSet::GetMaxHealthAttribute()).Remove(MaxHealthHandle);
		if (ASC->GetAvatarActor() == this)
		{
			CharacterStateMachine->SuspendResources();
			if (auto* GameplayASC = Cast<UNGAbilitySystemComponent>(ASC)) GameplayASC->ClearAbilityInput();
			ASC->CancelAllAbilities();
			ASC->SetAvatarActor(nullptr);
		}
	}
	BoundASC.Reset();
	Combat->BindAbilitySystem(nullptr);
	Interaction->BindAbilitySystem(nullptr);
}
void ANextgenoneCharacter::ApplyCombatPresentation()
{
	if (!Combat) return;
	const auto Equipment = Combat->GetEquipment();
	const auto* Def = Equipment.Definition.Get();
	const bool bArmed = !IsDead() && Equipment.bEquipped && Def;
	const auto DesiredOverlay = bArmed ? Def->Overlay : EALSOverlayState::Default;
	if (GetOverlayState() != DesiredOverlay)
	{
		// The overlay callback restores our item after ALS updates its demo held object.
		SetOverlayState(DesiredOverlay);
		if (bArmed) return;
	}
	Super::AimAction_Implementation(bArmed && Combat->IsAiming());
	if (!bArmed || GetMovementState() == EALSMovementState::Mantling || GetMovementState() == EALSMovementState::Ragdoll)
	{ ClearHeldObject(); return; }
	AttachToHand(Def->Mesh, Def->SkeletalMesh, nullptr, false, Def->VisualTransform.GetLocation());
	HeldObjectRoot->SetRelativeRotation(Def->VisualTransform.Rotator());
	HeldObjectRoot->SetRelativeScale3D(Def->VisualTransform.GetScale3D());
	StaticMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void ANextgenoneCharacter::OnOverlayStateChanged(EALSOverlayState PreviousState)
{
	Super::OnOverlayStateChanged(PreviousState);
	if (Combat && Combat->GetEquipment().bEquipped) ApplyCombatPresentation();
}
void ANextgenoneCharacter::Tick(float DeltaSeconds)
{
	if (IsDead())
	{
		// ALS's non-virtual RagdollUpdate trusts IsLocallyControlled/its client RPC.
		// Death ragdolls are cosmetic local physics. Retain the engine/Blueprint actor tick
		// without ALS's pelvis RPCs or remote-body correction.
		ACharacter::Tick(DeltaSeconds);
		return;
	}
	Super::Tick(DeltaSeconds);
}
void ANextgenoneCharacter::RagdollStart()
{
	if (IsDead() && GetMovementState() == EALSMovementState::Ragdoll) return;
	Super::RagdollStart();
	if (IsDead())
	{
		GetMesh()->bOnlyAllowAutonomousTickPose = false;
		// Cosmetic ragdolls differ on each machine and cannot be authoritative movement obstacles.
		GetMesh()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		GetMesh()->CanCharacterStepUpOn = ECB_No;
		// The permanent cosmetic corpse is independent of capsule/possession movement.
		GetMesh()->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		if (HitReaction) HitReaction->ReleaseToRagdoll();
	}
}
void ANextgenoneCharacter::RagdollEnd()
{
	// Includes late ALS multicast/get-up requests arriving after death.
	if (!IsDead())
	{
		Super::RagdollEnd();
		ApplyCombatPresentation();
	}
}
void ANextgenoneCharacter::HealthChanged(const FOnAttributeChangeData& Data)
{
    auto* ASC = BoundASC.Get();
    if (!ASC || ASC->GetAvatarActor() != this || IsDead()) return;
    if (HasAuthority() && GetHealth() <= 0.f)
    {
        ASC->CancelAllAbilities();
        CharacterStateMachine->NotifyDeath();
    }
    OnHealthChanged.Broadcast(GetHealth(), GetMaxHealth());
}
void ANextgenoneCharacter::StateChanged(FGameplayTag Previous, FGameplayTag Current)
{
    const bool bNowDead = IsDead();
    if (bNowDead)
    {
        GameplayInput->ShutdownInput();
        GetCharacterMovement()->DisableMovement();
    }
    if (Previous.MatchesTag(TAG_NGDead) != bNowDead) OnDeathChanged.Broadcast(bNowDead);
    OnHealthChanged.Broadcast(GetHealth(), GetMaxHealth());
}
void ANextgenoneCharacter::PossessedBy(AController* C) { Super::PossessedBy(C); TryInitializeGameplay(); }
void ANextgenoneCharacter::UnPossessed()
{
	GameplayInput->ShutdownInput();
	if (HasAuthority()) Combat->SetEquippedOnServer(false);
	if (!HasAuthority() || CharacterStateMachine->CanReleaseAvatar()) UnbindGameplay();
	Super::UnPossessed();
	CharacterStateMachine->RefreshResources();
}
void ANextgenoneCharacter::OnRep_PlayerState() { Super::OnRep_PlayerState(); TryInitializeGameplay(); }
void ANextgenoneCharacter::OnRep_Controller() { Super::OnRep_Controller(); if (!IsLocallyControlled()) GameplayInput->ShutdownInput(); TryInitializeGameplay(); CharacterStateMachine->RefreshResources(); }
void ANextgenoneCharacter::BeginPlay()
{
    CharacterStateMachine->OnStateChanged.AddUObject(this, &ThisClass::StateChanged);
    Super::BeginPlay();
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    TryInitializeGameplay();
    if (auto* ASC = BoundASC.Get(); ASC && ASC->GetAvatarActor() == this) CharacterStateMachine->NotifyGameplayReady(ASC);
    CharacterStateMachine->RestorePresentation();
	ApplyCombatPresentation();
}
void ANextgenoneCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
	auto* ASC = BoundASC.Get();
	auto* PS = ASC ? Cast<ANextgenonePlayerState>(ASC->GetOwnerActor()) : nullptr;
	GameplayInput->ShutdownInput();
	CharacterStateMachine->SuspendResources();
	UnbindGameplay();
	Super::EndPlay(Reason);
	if (HasAuthority() && IsValid(PS) && Reason == EEndPlayReason::Destroyed) PS->ResumePendingLife(this);
}

void ANextgenoneCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (!IsDead()) GameplayInput->InitializeInput(PlayerInputComponent);
	else GameplayInput->ShutdownInput();
	CharacterStateMachine->RefreshResources();
}
