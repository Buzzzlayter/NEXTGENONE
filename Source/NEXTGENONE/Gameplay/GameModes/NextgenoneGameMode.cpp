#include "Gameplay/GameModes/NextgenoneGameMode.h"
#include "Gameplay/Character/NextgenoneCharacter.h"
#include "Gameplay/Player/NextgenonePlayerController.h"
#include "Gameplay/Player/NextgenonePlayerState.h"
#include "GameFramework/HUD.h"
#include "Gameplay/StateMachine/NGCharacterStateMachineComponent.h"
#include "GameplayTags/NGGameplayTags.h"
#include "TimerManager.h"

uint8 ANextgenoneGameMode::GetDefaultTeamId(const AController& Controller) const
{
	return Controller.IsPlayerController() ? PlayerTeamId : BotTeamId;
}

ANextgenoneGameMode::ANextgenoneGameMode()
{
	DefaultPawnClass = ANextgenoneCharacter::StaticClass();
	PlayerControllerClass = ANextgenonePlayerController::StaticClass();
	PlayerStateClass = ANextgenonePlayerState::StaticClass();
	GameplayCharacterClass = TSoftClassPtr<APawn>(FSoftObjectPath(TEXT("/Game/Blueprints/BP_NextgenoneCharacter.BP_NextgenoneCharacter_C")));
	GameplayControllerClass = TSoftClassPtr<APlayerController>(FSoftObjectPath(TEXT("/Game/Blueprints/BP_NextgenonePlayerController.BP_NextgenonePlayerController_C")));
	GameplayHUDClass = TSoftClassPtr<AHUD>(FSoftObjectPath(TEXT("/Game/Blueprints/BP_NextgenoneHUD.BP_NextgenoneHUD_C")));
}

void ANextgenoneGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	// Resolve assets after native CDO registration, before any player is spawned.
	if (UClass* CharacterClass = GameplayCharacterClass.LoadSynchronous()) DefaultPawnClass = CharacterClass;
	if (UClass* ControllerClass = GameplayControllerClass.LoadSynchronous()) PlayerControllerClass = ControllerClass;
	if (UClass* UIClass = GameplayHUDClass.LoadSynchronous()) HUDClass = UIClass;
}

void ANextgenoneGameMode::RestartPlayer(AController* NewPlayer)
{
	ClearRespawn(NewPlayer);
	Super::RestartPlayer(NewPlayer);
	auto* Player = Cast<APlayerController>(NewPlayer);
	APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	if (auto* Machine = Pawn ? Pawn->FindComponentByClass<UNGCharacterStateMachineComponent>() : nullptr)
	{
		Machine->OnStateChanged.RemoveAll(this);
		const TWeakObjectPtr<APawn> WeakPawn(Pawn);
		Machine->OnStateChanged.AddWeakLambda(this, [this, WeakPawn](FGameplayTag, FGameplayTag Current)
		{
			if (Current.MatchesTag(TAG_NGDead)) ScheduleRespawn(WeakPawn.Get());
		});
		if (Machine->HasStateTag(TAG_NGDead)) ScheduleRespawn(Pawn);
	}
}

void ANextgenoneGameMode::ScheduleRespawn(APawn* DeadPawn)
{
	auto* Player = IsValid(DeadPawn) ? Cast<APlayerController>(DeadPawn->GetController()) : nullptr;
	if (!HasAuthority() || !IsValid(Player) || Player->IsActorBeingDestroyed()
		|| Player->GetPawn() != DeadPawn || RespawnTimers.Contains(Player)) return;
	const auto* Machine = DeadPawn->FindComponentByClass<UNGCharacterStateMachineComponent>();
	if (!Machine || !Machine->HasStateTag(TAG_NGDead)) return;
	const TWeakObjectPtr<APlayerController> WeakPlayer(Player);
	const TWeakObjectPtr<APawn> WeakPawn(DeadPawn);
	FTimerHandle& Timer = RespawnTimers.Add(WeakPlayer);
	GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this, WeakPlayer, WeakPawn]()
	{
		RespawnTimers.Remove(WeakPlayer);
		auto* PC = WeakPlayer.Get();
		auto* Corpse = WeakPawn.Get();
		if (!IsValid(PC) || PC->IsActorBeingDestroyed() || !IsValid(Corpse) || PC->GetPawn() != Corpse
			|| Corpse->GetController() != PC || !PC->PlayerState) return;
		auto* DeadMachine = Corpse->FindComponentByClass<UNGCharacterStateMachineComponent>();
		if (!DeadMachine || !DeadMachine->HasStateTag(TAG_NGDead) || !DeadMachine->CanReleaseAvatar()) return;
		DeadMachine->OnStateChanged.RemoveAll(this);
		// UnPossessed releases only this body's GAS/state resources before the new Avatar is initialized.
		PC->UnPossess();
		Corpse->SetLifeSpan(FMath::IsFinite(CorpseLifetime) ? FMath::Max(1.f, CorpseLifetime) : 15.f);
		RestartPlayer(PC);
		if (!PC->GetPawn()) UE_LOG(LogTemp, Warning, TEXT("Respawn failed for %s: check PlayerStart and DefaultPawnClass."), *GetNameSafe(PC));
	}), FMath::IsFinite(RespawnDelay) ? FMath::Max(.1f, RespawnDelay) : 3.f, false);
}

void ANextgenoneGameMode::ClearRespawn(AController* Controller)
{
	auto* Player = Cast<APlayerController>(Controller);
	if (!Player) return;
	if (auto* Timer = RespawnTimers.Find(Player)) GetWorldTimerManager().ClearTimer(*Timer);
	RespawnTimers.Remove(Player);
	if (APawn* Pawn = Player->GetPawn())
		if (auto* Machine = Pawn->FindComponentByClass<UNGCharacterStateMachineComponent>())
			Machine->OnStateChanged.RemoveAll(this);
}

void ANextgenoneGameMode::Logout(AController* Exiting)
{
	ClearRespawn(Exiting);
	Super::Logout(Exiting);
}

void ANextgenoneGameMode::EndPlay(EEndPlayReason::Type Reason)
{
	for (auto& Entry : RespawnTimers) GetWorldTimerManager().ClearTimer(Entry.Value);
	RespawnTimers.Reset();
	Super::EndPlay(Reason);
}
