#include "Gameplay/GameModes/NextgenoneGameMode.h"
#include "Gameplay/Character/NextgenoneCharacter.h"
#include "Gameplay/Player/NextgenonePlayerController.h"
#include "Gameplay/Player/NextgenonePlayerState.h"
#include "GameFramework/HUD.h"

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
