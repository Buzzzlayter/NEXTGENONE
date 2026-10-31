#include "NextgenoneCharacter.h"
#include "NextgenonePlayerState.h"
#include "NGAbilitySystem.h"
#include "NGCombat.h"
#include "Components/CapsuleComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

ANextgenoneCharacter::ANextgenoneCharacter(const FObjectInitializer& Init) : Super(Init)
{
	bReplicates = true;
	Combat = CreateDefaultSubobject<UNGCombatComponent>(TEXT("Combat"));
	Combat->TestWeapon = CreateDefaultSubobject<UNGWeaponDefinition>(TEXT("TestWeapon"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Combat->TestWeapon->Mesh = Cube.Object;
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}
UAbilitySystemComponent* ANextgenoneCharacter::GetAbilitySystemComponent() const
{
	UAbilitySystemComponent* ASC = BoundASC.Get();
	return !bDead && ASC && ASC->GetAvatarActor() == this ? ASC : nullptr;
}
float ANextgenoneCharacter::GetHealth() const
{
	const auto* PS = GetPlayerState<ANextgenonePlayerState>();
	return !bDead && PS && PS->AbilitySystem->GetAvatarActor() == this ? PS->HealthSet->Health.GetCurrentValue() : 0.f;
}
float ANextgenoneCharacter::GetMaxHealth() const
{
	const auto* PS = GetPlayerState<ANextgenonePlayerState>();
	return PS ? PS->HealthSet->MaxHealth.GetCurrentValue() : 100.f;
}
void ANextgenoneCharacter::TryInitializeGameplay()
{
	ANextgenonePlayerState* PS = GetPlayerState<ANextgenonePlayerState>();
	if (!PS || bDead || (PS->GetPawn() && PS->GetPawn() != this)) return;
	auto* ASC = PS->AbilitySystem.Get();
	// A delayed OnRep from an old Pawn must not steal a newer avatar.
	if (!HasAuthority() && ASC->GetAvatarActor() && ASC->GetAvatarActor() != this)
	{
		const auto* Old = Cast<ANextgenoneCharacter>(ASC->GetAvatarActor());
		if (PS->GetPawn() != this && Old && !Old->IsDead()) return;
	}
	if (BoundASC == ASC && ASC->GetAvatarActor() == this) { ASC->RefreshAbilityActorInfo(); return; }
	UnbindGameplay();
	if (HasAuthority()) PS->StartLife(this);
	else ASC->InitAbilityActorInfo(PS, this);
	BoundASC = ASC;
	HealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(UNGHealthSet::GetHealthAttribute()).AddUObject(this, &ThisClass::HealthChanged);
	MaxHealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(UNGHealthSet::GetMaxHealthAttribute()).AddUObject(this, &ThisClass::HealthChanged);
	OnHealthChanged.Broadcast(GetHealth(), GetMaxHealth());
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
			if (HasAuthority()) ASC->CancelAllAbilities();
			ASC->SetAvatarActor(nullptr);
		}
	}
	BoundASC.Reset();
}
void ANextgenoneCharacter::HealthChanged(const FOnAttributeChangeData& Data)
{
	auto* ASC = BoundASC.Get();
	if (!ASC || ASC->GetAvatarActor() != this || bDead) return;
	if (HasAuthority() && GetHealth() <= 0.f)
	{
		bDead = true;
		ASC->SetLooseGameplayTagCount(TAG_NGDead, 1, EGameplayTagReplicationState::TagOnly);
		ASC->CancelAllAbilities();
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
		OnRep_Dead(); ForceNetUpdate();
		UE_LOG(LogTemp, Display, TEXT("NG_DEATH pawn=%s"), *GetName());
	}
	OnHealthChanged.Broadcast(GetHealth(), GetMaxHealth());
}
void ANextgenoneCharacter::OnRep_Dead()
{
	if (bDead) GetCharacterMovement()->DisableMovement();
	OnDeathChanged.Broadcast(bDead);
	OnHealthChanged.Broadcast(GetHealth(), GetMaxHealth());
}
void ANextgenoneCharacter::PossessedBy(AController* C) { Super::PossessedBy(C); TryInitializeGameplay(); }
void ANextgenoneCharacter::UnPossessed()
{
	if (HasAuthority()) Combat->SetEquippedOnServer(false);
	UnbindGameplay(); Super::UnPossessed();
}
void ANextgenoneCharacter::OnRep_PlayerState() { Super::OnRep_PlayerState(); TryInitializeGameplay(); }
void ANextgenoneCharacter::OnRep_Controller() { Super::OnRep_Controller(); TryInitializeGameplay(); }
void ANextgenoneCharacter::BeginPlay()
{
	Super::BeginPlay();
	// Imported Blueprint defaults can override native constructor responses.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	TryInitializeGameplay();
}
void ANextgenoneCharacter::EndPlay(const EEndPlayReason::Type Reason) { UnbindGameplay(); Super::EndPlay(Reason); }
void ANextgenoneCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const { Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(ANextgenoneCharacter, bDead); }
