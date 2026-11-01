#include "NGTestGameMode.h"
#include "NextgenonePlayerState.h"
#include "NextgenoneCharacter.h"
#include "NextgenonePlayerController.h"
#include "NGCombat.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/PackageName.h"

void ANGCombatTestGameMode::BeginPlay()
{
	Super::BeginPlay();
#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("NGCombatSmoke")))
	{
		Deadline = GetWorld()->GetTimeSeconds() + 90;
		GetWorldTimerManager().SetTimer(SmokeTimer, this, &ThisClass::SmokeTick, .1f, true);
	}
#endif
}
void ANGCombatTestGameMode::Fail(const FString& Reason)
{
	UE_LOG(LogTemp, Error, TEXT("NG_SMOKE_FAIL phase=%d %s"), Phase, *Reason);
	GetWorldTimerManager().ClearTimer(SmokeTimer);
}
void ANGCombatTestGameMode::RecordSmoke(ANextgenonePlayerController* PC, int32 Command, bool bPassed)
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("NGCombatSmoke")) || Command != ExpectedReport || !Players.Contains(PC)) return;
	if (!bPassed) { Fail(FString::Printf(TEXT("client %s report %d failed"), *GetNameSafe(PC), Command)); return; }
	Reports.Add(PC);
	UE_LOG(LogTemp, Display, TEXT("NG_CLIENT_REPORT command=%d player=%s passed=1"), Command, *GetNameSafe(PC));
}
void ANGCombatTestGameMode::AskClients(int32 Command)
{
	Reports.Reset(); ExpectedReport = Command;
	for (auto PC : Players) PC->ClientSmoke(Command);
}
void ANGCombatTestGameMode::SmokeTick()
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now > Deadline) { Fail(TEXT("timeout")); return; }
	if (Phase == 0)
	{
		Players.Reset();
		for (TActorIterator<ANextgenonePlayerController> It(GetWorld()); It; ++It) if (It->GetPawn()) Players.Add(*It);
		if (Players.Num() != 2) return;
		for (int32 i=0; i<2; ++i)
		{
			auto* P = Cast<ANextgenoneCharacter>(Players[i]->GetPawn()); if (!P || !P->GetAbilitySystemComponent()) return;
			P->SetActorLocation(FVector(i*600.f, 0, 100)); P->SetActorRotation(FRotator::ZeroRotator);
			P->GetCharacterMovement()->DisableMovement();
			Players[i]->SetControlRotation(FRotator::ZeroRotator);
		}
		AskClients(0); Phase=1;
		return;
	}
	if (Phase==1 && Reports.Num()==2) { Players[0]->ClientSmoke(1); Phase=2; PhaseTime=Now; return; }
	auto* A = Cast<ANextgenoneCharacter>(Players[0]->GetPawn()); auto* B = Cast<ANextgenoneCharacter>(Players[1]->GetPawn());
	if (!A || !B) { Fail(TEXT("missing Pawn")); return; }
	// Player abilities now coexist with the equipment grant. Do not assume
	// the whole ASC contains only GA_Fire as the ability library grows.
	auto HasUniqueFoundationGrants = [A]()
	{
		int32 FireGrants = 0, JumpGrants = 0;
		for (const FGameplayAbilitySpec& Spec : A->GetAbilitySystemComponent()->GetActivatableAbilities())
		{
			if (Spec.Ability && Spec.Ability->IsA<UNGFireAbility>()) ++FireGrants;
			if (Spec.GetDynamicSpecSourceTags().HasTagExact(TAG_NGInputJump)) ++JumpGrants;
		}
		return FireGrants == 1 && JumpGrants == 1;
	};
	if (Phase==2 && Now-PhaseTime>2 && A->Combat->GetEquipment().bEquipped)
	{
		A->TryInitializeGameplay(); A->TryInitializeGameplay();
		if (!HasUniqueFoundationGrants()) { Fail(TEXT("duplicate/missing foundation grants")); return; }
		Players[0]->ClientSmoke(2); Phase=3; PhaseTime=Now; return;
	}
	if (Phase==3 && Now-PhaseTime>2)
	{
		if (B->GetHealth()!=75 || A->Combat->GetMagazineAmmo()!=11) { Fail(FString::Printf(TEXT("burst/equip hp=%.0f ammo=%d"),B->GetHealth(),A->Combat->GetMagazineAmmo())); return; }
		if (!HasUniqueFoundationGrants()) { Fail(TEXT("equip duplicated foundation grants")); return; }
		AskClients(4); Phase=4; return;
	}
	if (Phase==4 && Reports.Num()==2) { Shots=0; Phase=5; PhaseTime=Now; return; }
	if (Phase==5 && Now-PhaseTime>1.2)
	{
		if (Shots<3) { Players[0]->ClientSmoke(3); ++Shots; PhaseTime=Now; return; }
		if (!B->IsDead() || B->GetHealth()!=0) { Fail(TEXT("death not reached")); return; }
		AskClients(5); Phase=6; return;
	}
	if (Phase==6 && Reports.Num()==2)
	{
		// Keep the corpse alive while the same PlayerState possesses a new Pawn.
		Players[1]->UnPossess(); RestartPlayer(Players[1].Get());
		auto* NewPawn=Cast<ANextgenoneCharacter>(Players[1]->GetPawn());
		if (!NewPawn || NewPawn==B || NewPawn->GetHealth()!=100 || B->GetAbilitySystemComponent()!=nullptr) { Fail(TEXT("respawn binding")); return; }
		B->TryInitializeGameplay();
		B->UnbindGameplay(); // delayed old teardown must not detach the new avatar
		bool Rejected=!UNGCombatComponent::ApplyDamageToPawn(B,A->GetAbilitySystemComponent(),50);
		// A GE constructed before death must also reject a changed target avatar.
		auto Context=A->GetAbilitySystemComponent()->MakeEffectContext(); Context.AddSourceObject(B);
		auto DelayedSpec=A->GetAbilitySystemComponent()->MakeOutgoingSpec(UNGDamageEffect::StaticClass(),1.f,Context);
		DelayedSpec.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(TEXT("Data.Damage")),50.f);
		A->GetAbilitySystemComponent()->ApplyGameplayEffectSpecToTarget(*DelayedSpec.Data,NewPawn->GetAbilitySystemComponent());
		if (!Rejected || NewPawn->GetHealth()!=100 || NewPawn->GetAbilitySystemComponent()==nullptr) { Fail(TEXT("corpse affected new life")); return; }
		NewPawn->SetActorLocation(FVector(600,600,100));
		// Exercise the authoritative limiter with valid current grants, not only stale client handles.
		const int32 BeforeAmmo=A->Combat->GetMagazineAmmo();
		if (!A->Combat->FireOnServer() || A->Combat->FireOnServer()) { Fail(TEXT("reactivation bypassed cooldown")); return; }
		A->Combat->SetEquippedOnServer(false); A->Combat->SetEquippedOnServer(true);
		if (A->Combat->FireOnServer() || A->Combat->GetMagazineAmmo()!=BeforeAmmo-1 || NewPawn->GetHealth()!=100)
		{ Fail(TEXT("reequip bypassed cooldown or corpse hitscan hurt new life")); return; }
		UE_LOG(LogTemp,Display,TEXT("NG_LIMITER_PASS immediate_reactivation=blocked reequip=blocked corpse_hitscan=rejected"));
		Phase=7; PhaseTime=Now; return;
	}
	if (Phase==7 && Now-PhaseTime>2) { AskClients(6); Phase=8; return; }
	if (Phase==8 && Reports.Num()==2)
	{
		UE_LOG(LogTemp, Display, TEXT("NG_SMOKE_PASS server+2clients GAS equip burst_rate ammo damage health death respawn corpse_guard"));
		GetWorldTimerManager().ClearTimer(SmokeTimer); Phase=9;
	}
}
