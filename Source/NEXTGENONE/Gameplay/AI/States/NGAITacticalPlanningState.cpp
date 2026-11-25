#include "Gameplay/AI/States/NGAITacticalPlanningState.h"

#include "AbilitySystem/NGAbilitySystem.h"
#include "AbilitySystemGlobals.h"
#include "Gameplay/AI/Memory/NGAIMemoryComponent.h"
#include "Gameplay/AI/NGAIController.h"
#include "Gameplay/AI/NGAIEvents.h"
#include "Gameplay/AI/States/NGAIFireBurstState.h"
#include "Gameplay/AI/States/NGAITacticalDecisionState.h"
#include "Gameplay/Combat/NGCombat.h"
#include "Gameplay/Combat/NGWeaponActor.h"
#include "Gameplay/Combat/NGWeaponDefinition.h"
#include "GameplayTags/NGGameplayTags.h"
#include "Inventory/NGInventoryManagerComponent.h"
#include "Inventory/NGInventoryItemDefinition.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
	constexpr double OptionalFlankCooldown = 4.;
	constexpr int32 MaxPressureChoices = 2;
	constexpr double RecentDamageWindow = 2.5;

	bool IsFlightAction(FName Action)
	{
		return Action == NGAIEvents::EquipJetpack || Action == NGAIEvents::Fly;
	}

	const FName RegainContact(TEXT("RegainContact"));
	const FName Pressure(TEXT("Pressure"));
	const FName ImproveAngle(TEXT("ImproveAngle"));

	FName GetGoalId(ENGAITacticalGoal SelectedGoal)
	{
		switch (SelectedGoal)
		{
		case ENGAITacticalGoal::RegainContact: return RegainContact;
		case ENGAITacticalGoal::Pressure: return Pressure;
		case ENGAITacticalGoal::ImproveAngle: return ImproveAngle;
		case ENGAITacticalGoal::Reload: return NGAIEvents::Reload;
		}
		return NAME_None;
	}

	FName GetFallbackAction(ENGAITacticalGoal SelectedGoal)
	{
		switch (SelectedGoal)
		{
		case ENGAITacticalGoal::RegainContact: return NGAIEvents::Search;
		case ENGAITacticalGoal::Pressure: return NGAIEvents::HoldAngle;
		case ENGAITacticalGoal::ImproveAngle: return NGAIEvents::GroundFlank;
		case ENGAITacticalGoal::Reload: return NGAIEvents::Reload;
		}
		return NAME_None;
	}
}

void UNGAITacticalPlanningState::Enter()
{
	Super::Enter();
	StopPlanning();
	auto* Controller = Cast<ANGAIController>(GetContextOwner());
	if (!Controller || !Controller->HasAuthority())
		return;

	ExpectedPawn = Controller->GetPawn();
	Memory = Controller->GetMemory();
	if (auto* ASC = Controller->GetReadyAbilitySystem(ExpectedPawn.Get()))
	{
		ObservedASC = ASC;
		HealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UNGHealthSet::GetHealthAttribute())
			.AddUObject(this, &ThisClass::OnVitalChanged);
		ShieldChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UNGHealthSet::GetShieldAttribute())
			.AddUObject(this, &ThisClass::OnVitalChanged);
	}
	if (auto* TargetMemory = Memory.Get())
	{
		MemoryUpdatedHandle = TargetMemory->OnMemoryUpdated.AddUObject(this, &ThisClass::EvaluateGoal);
		EvaluateGoal();
	}
}

void UNGAITacticalPlanningState::Exit(FName Event)
{
	StopPlanning();
	Super::Exit(Event);
}

FName UNGAITacticalPlanningState::CommitAction(const APawn& Pawn, ENGAITacticalLane Lane)
{
	const UWorld* World = GetWorld();
	if (ExpectedPawn.Get() != &Pawn || !World)
		return NAME_None;

	EvaluateGoal();
	const double Now = World->GetTimeSeconds();
	if (Lane == ENGAITacticalLane::Mobility)
	{
		ActiveMobilityAction = NAME_None;
		bFlightWaitingForWeapon = IsFlightAction(MobilityChoice) && !ActiveWeaponAction.IsNone();
		if (bFlightWaitingForWeapon)
			return NGAIEvents::Wait;

		if (IsFlightAction(MobilityChoice))
		{
			ActiveMobilityAction = MobilityChoice;
			if (MobilityChoice == NGAIEvents::EquipJetpack)
				NextJetpackEquipTime = Now + 3.;
			else
				NextFlightTime = Now + 6.;
		}
		else if (MobilityChoice == NGAIEvents::GroundFlank)
		{
			ConsecutivePressureChoices = 0;
			NextGroundTryTime = Now + 1.;
			NextOptionalFlankTime = Now + OptionalFlankCooldown;
		}
		return MobilityChoice;
	}
	if (Lane == ENGAITacticalLane::Weapon)
	{
		ActiveWeaponAction = NAME_None;
		if (bFlightActive || IsFlightAction(ActiveMobilityAction)
			|| (bFlightWaitingForWeapon && IsFlightAction(MobilityChoice)))
			return NGAIEvents::Wait;

		if (WeaponChoice != NGAIEvents::Wait)
			ActiveWeaponAction = WeaponChoice;
		if (WeaponChoice == NGAIEvents::HoldAngle)
		{
			ConsecutivePressureChoices = FMath::Min(ConsecutivePressureChoices + 1, MaxPressureChoices);
			NextFireTryTime = Now + .25;
		}
		else if (WeaponChoice == NGAIEvents::EquipWeapon)
			NextWeaponEquipTime = Now + 1.5;
		else if (WeaponChoice == NGAIEvents::Reload)
			NextReloadTryTime = Now + 1.25;
		return WeaponChoice;
	}

	const bool bReloadAvailable = PlannedFacts.Matches({{ENGAIFact::ReserveAvailable, true}});
	const FName Fallback = bReloadAvailable ? NGAIEvents::Reload : GetFallbackAction(Goal);
	const FName Action = Plan.Status == ENGAIPlanStatus::Succeeded && !Plan.Actions.IsEmpty() ? Plan.Actions[0] : Fallback;
	if (Action == NGAIEvents::HoldAngle)
		ConsecutivePressureChoices = FMath::Min(ConsecutivePressureChoices + 1, MaxPressureChoices);
	else if (Action == NGAIEvents::GroundFlank)
	{
		ConsecutivePressureChoices = 0;
		NextOptionalFlankTime = Now + OptionalFlankCooldown;
	}
	return Action;
}

void UNGAITacticalPlanningState::EvaluateGoal()
{
	const auto* Pawn = ExpectedPawn.Get();
	const auto* Controller = Cast<ANGAIController>(GetContextOwner());
	const auto* TargetMemory = Memory.Get();
	const auto* Target = TargetMemory ? TargetMemory->GetTarget() : nullptr;
	const auto* TargetActor = Target ? Target->Actor.Get() : nullptr;
	const UWorld* World = GetWorld();
	if (!Pawn || !Controller || Controller->GetPawn() != Pawn || !World)
	{
		Goal = ENGAITacticalGoal::RegainContact;
		PlannedGoal = NAME_None;
		PlannedFacts = FNGAIFacts();
		Plan = FNGAIPlan();
		MobilityChoice = NAME_None;
		WeaponChoice = NAME_None;
		return;
	}

	const auto* Combat = Pawn->FindComponentByClass<UNGCombatComponent>();
	const auto* WeaponActor = Combat ? Combat->GetWeaponActor() : nullptr;
	const auto* WeaponDefinition = WeaponActor ? WeaponActor->GetDefinition() : nullptr;
	const int32 MagazineAmmo = WeaponActor ? WeaponActor->GetMagazineAmmo() : 0;
	const bool bVisible = TargetActor && Target->bVisible;
	const bool bWantsReload = WeaponDefinition && MagazineAmmo < WeaponDefinition->MagazineCapacity
		&& (MagazineAmmo == 0 || (!bVisible && MagazineAmmo <= WeaponDefinition->MagazineCapacity / 4));
	const bool bNeedsReload = bWantsReload
		&& Combat->HasAvailableReserveAmmo(*WeaponDefinition);

	if (TargetActor && EvaluatedTarget != Target->Actor)
	{
		EvaluatedTarget = Target->Actor;
		ConsecutivePressureChoices = 0;
		Goal = ENGAITacticalGoal::RegainContact;
		PlannedGoal = NAME_None;
	}

	FNGAIDecisionSnapshot Snapshot;
	Snapshot.Facts.Set(ENGAIFact::TargetVisible, bVisible);
	Snapshot.Facts.Set(ENGAIFact::WeaponEquipped, WeaponActor != nullptr);
	Snapshot.Facts.Set(ENGAIFact::MagazineLoaded, MagazineAmmo > 0);
	if (bWantsReload)
		Snapshot.Facts.Set(ENGAIFact::ReserveAvailable, bNeedsReload);
	Snapshot.Facts.Set(ENGAIFact::MagazineToppedUp, !bWantsReload);
	Snapshot.Facts.Set(ENGAIFact::SearchPerformed, false);
	Snapshot.Facts.Set(ENGAIFact::AttackPerformed, false);
	Snapshot.Facts.Set(ENGAIFact::AtPosition, false);
	Snapshot.Facts.Set(ENGAIFact::LineOfFire, false);
	if (!bVisible)
	{
		EvaluateBranches(*Pawn, Target, false, 0.);
		RefreshPlan(bNeedsReload ? ENGAITacticalGoal::Reload : ENGAITacticalGoal::RegainContact, Snapshot);
		return;
	}

	const double Distance = FVector::Dist(Pawn->GetPawnViewLocation(), Target->LastSeenLocation);
	const float MinDistance = FMath::IsFinite(MinHoldDistance) ? FMath::Max(0.f, MinHoldDistance) : 600.f;
	const float MaxDistance = FMath::IsFinite(MaxHoldDistance) ? FMath::Max(MinDistance, MaxHoldDistance) : FMath::Max(MinDistance, 2400.f);
	const double DistanceMargin = Goal == ENGAITacticalGoal::Pressure ? 125. : 0.;
	const bool bInRange = Distance >= MinDistance - DistanceMargin && Distance <= MaxDistance + DistanceMargin;
	const bool bHasLineOfFire = WeaponActor
		&& UNGAIFireBurstState::HasClearShot(*Pawn, *TargetActor, Target->LastSeenLocation, WeaponActor->GetShotOrigin());
	const bool bClearShot = bInRange && bHasLineOfFire;
	Snapshot.Facts.Set(ENGAIFact::LineOfFire, bClearShot);
	const bool bCanPressure = bClearShot && (MagazineAmmo > 0 || bNeedsReload);

	const double HalfRange = FMath::Max(1., (MaxDistance - MinDistance) * .5);
	const double RangeCenter = MinDistance + (MaxDistance - MinDistance) * .5;
	const double RangeEdge = FMath::Clamp(FMath::Abs(Distance - RangeCenter) / HalfRange, 0., 1.);
	EvaluateBranches(*Pawn, Target, bHasLineOfFire, RangeEdge);
	const double PressureScore = bCanPressure ? 1. - .2 * RangeEdge : 0.;
	const bool bCanFlankByChoice = World->GetTimeSeconds() >= NextOptionalFlankTime;
	const double FlankScore = bCanPressure
		? (bCanFlankByChoice ? .25 + .4 * ConsecutivePressureChoices + .15 * RangeEdge : 0.)
		: 1.;
	RefreshPlan(FlankScore > PressureScore ? ENGAITacticalGoal::ImproveAngle : ENGAITacticalGoal::Pressure, Snapshot);
}

void UNGAITacticalPlanningState::OnVitalChanged(const FOnAttributeChangeData& Data)
{
	if (Data.NewValue < Data.OldValue && GetWorld())
		LastDamageTime = GetWorld()->GetTimeSeconds();
}

void UNGAITacticalPlanningState::EvaluateBranches(const APawn& Pawn, const FNGTargetMemory* Target,
	bool bClearShot, double RangeEdge)
{
	using namespace NGAIEvents;
	const double Now = GetWorld()->GetTimeSeconds();
	const auto* ASC = ObservedASC.Get();
	const auto* Enemy = Target && Target->bVisible ? Cast<APawn>(Target->Actor.Get()) : nullptr;
	const auto* EnemyASC = Enemy ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Enemy) : nullptr;
	const bool bShotTowardBot = EnemyASC && EnemyASC->HasMatchingGameplayTag(TAG_NGFireCooldown)
		&& FVector::DotProduct(Enemy->GetBaseAimRotation().Vector(),
			(Pawn.GetPawnViewLocation() - Target->LastSeenLocation).GetSafeNormal()) > .97;
	const double DamagePressure = LastDamageTime >= 0.
		? FMath::Clamp(1. - (Now - LastDamageTime) / RecentDamageWindow, 0., 1.) : 0.;
	const double IncomingPressure = FMath::Max(DamagePressure, bShotTowardBot ? .65 : 0.);
	const float MaxHealth = ASC ? ASC->GetNumericAttribute(UNGHealthSet::GetMaxHealthAttribute()) : 0.f;
	const double HealthFraction = MaxHealth > 0.f
		? FMath::Clamp(ASC->GetNumericAttribute(UNGHealthSet::GetHealthAttribute()) / MaxHealth, 0.f, 1.f) : 1.;
	const double Danger = FMath::Clamp(IncomingPressure * (1. + .45 * (1. - HealthFraction)), 0., 1.);

	const auto* Combat = Pawn.FindComponentByClass<UNGCombatComponent>();
	const auto* WeaponActor = Combat ? Combat->GetWeaponActor() : nullptr;
	const auto* WeaponDefinition = WeaponActor ? WeaponActor->GetDefinition() : nullptr;
	const int32 Ammo = WeaponActor ? WeaponActor->GetMagazineAmmo() : 0;
	WeaponChoice = Wait;
	double WeaponScore = .15;
	if (!WeaponActor && Now >= NextWeaponEquipTime)
	{
		WeaponChoice = EquipWeapon;
		WeaponScore = 1.;
	}
	if (Enemy && bClearShot && Ammo > 0 && Now >= NextFireTryTime)
	{
		WeaponChoice = HoldAngle;
		WeaponScore = 1. - .20 * RangeEdge;
	}
	if (WeaponDefinition && Now >= NextReloadTryTime && Ammo < WeaponDefinition->MagazineCapacity
		&& (Ammo == 0 || (!bClearShot && Ammo <= WeaponDefinition->MagazineCapacity / 4)))
	{
		const double ReloadScore = Ammo == 0 ? 1. : .70 - .35 * Danger;
		if (ReloadScore > WeaponScore && Combat->HasAvailableReserveAmmo(*WeaponDefinition))
			WeaponChoice = Reload;
	}

	const auto* Character = Cast<ACharacter>(&Pawn);
	const auto* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	bFlightActive = ASC && ASC->HasMatchingGameplayTag(TAG_NGJetpackActive);
	MobilityChoice = Wait;
	if (!Movement || !Movement->IsMovingOnGround() || bFlightActive)
		return;
	if (!Enemy)
	{
		MobilityChoice = Search;
		return;
	}

	double MobilityScore = .45;
	const bool bCanChooseFlank = !bClearShot || Danger >= .55 || Now >= NextOptionalFlankTime;
	const double GroundScore = .23 + .28 * RangeEdge + .40 * Danger
		+ .16 * ConsecutivePressureChoices + (bClearShot ? 0. : .52);
	if (Now >= NextGroundTryTime && bCanChooseFlank && GroundScore > MobilityScore)
	{
		MobilityChoice = GroundFlank;
		MobilityScore = GroundScore;
	}

	const double Elevation = FMath::Clamp((Target->LastSeenLocation.Z - Pawn.GetPawnViewLocation().Z - 150.) / 450., 0., 1.);
	const double FlightScore = .10 + .52 * Danger + .70 * Elevation + .15 * RangeEdge;
	if (FlightScore <= MobilityScore || Now < NextFlightTime || !ASC)
		return;
	if (ASC->HasMatchingGameplayTag(TAG_NGJetpackEquipped))
	{
		MobilityChoice = Fly;
		return;
	}

	const auto* Owner = ASC->GetOwnerActor();
	const auto* Inventory = Owner && JetpackItemClass && Now >= NextJetpackEquipTime
		? Owner->FindComponentByClass<UNGInventoryManagerComponent>() : nullptr;
	if (Inventory && Inventory->GetTotalItemCountByDefinition(JetpackItemClass) > 0)
		MobilityChoice = EquipJetpack;
}

void UNGAITacticalPlanningState::RefreshPlan(ENGAITacticalGoal NewGoal, const FNGAIDecisionSnapshot& Snapshot)
{
	Goal = NewGoal;
	const FName GoalId = GetGoalId(Goal);
	if (PlannedGoal == GoalId && PlannedFacts.Known == Snapshot.Facts.Known && PlannedFacts.Values == Snapshot.Facts.Values)
		return;

	PlannedGoal = GoalId;
	PlannedFacts = Snapshot.Facts;
	Plan = PlanningProfile ? NGAIGOAP::BuildPlan(*PlanningProfile, Snapshot, GoalId) : FNGAIPlan();
}

void UNGAITacticalPlanningState::StopPlanning()
{
	if (auto* TargetMemory = Memory.Get())
		TargetMemory->OnMemoryUpdated.Remove(MemoryUpdatedHandle);
	if (auto* ASC = ObservedASC.Get())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UNGHealthSet::GetHealthAttribute()).Remove(HealthChangedHandle);
		ASC->GetGameplayAttributeValueChangeDelegate(UNGHealthSet::GetShieldAttribute()).Remove(ShieldChangedHandle);
	}
	MemoryUpdatedHandle.Reset();
	HealthChangedHandle.Reset();
	ShieldChangedHandle.Reset();
	ObservedASC.Reset();
	Memory.Reset();
	ExpectedPawn.Reset();
	EvaluatedTarget.Reset();
	Goal = ENGAITacticalGoal::RegainContact;
	PlannedGoal = NAME_None;
	PlannedFacts = FNGAIFacts();
	Plan = FNGAIPlan();
	ConsecutivePressureChoices = 0;
	NextOptionalFlankTime = 0.;
	NextGroundTryTime = 0.;
	NextJetpackEquipTime = 0.;
	NextFlightTime = 0.;
	NextWeaponEquipTime = 0.;
	NextReloadTryTime = 0.;
	NextFireTryTime = 0.;
	LastDamageTime = -1.;
	MobilityChoice = NAME_None;
	WeaponChoice = NAME_None;
	ActiveMobilityAction = NAME_None;
	ActiveWeaponAction = NAME_None;
	bFlightWaitingForWeapon = false;
	bFlightActive = false;
}
