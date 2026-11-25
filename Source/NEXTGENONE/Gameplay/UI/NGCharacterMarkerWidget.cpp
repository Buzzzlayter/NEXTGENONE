#include "Gameplay/UI/NGCharacterMarkerWidget.h"

#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Gameplay/UI/NGAbilitySetVitalsWidget.h"
#include "Map/UIMarkerModel.h"

void UNGCharacterMarkerWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshCharacter();
}

void UNGCharacterMarkerWidget::NativeTick(const FGeometry& MyGeometry, float DeltaSeconds)
{
	Super::NativeTick(MyGeometry, DeltaSeconds);
	NextRefresh -= DeltaSeconds;
	if (NextRefresh > 0.f) return;
	NextRefresh = .25f;
	RefreshCharacter();
}

void UNGCharacterMarkerWidget::RefreshCharacter()
{
	const auto* Model = GetMarkerModel();
	auto* Pawn = Model ? Cast<APawn>(Model->GetSourceActor()) : nullptr;
	if (Vitals) Vitals->ObservePawn(Pawn);
	if (!CharacterName) return;

	const auto* PlayerState = Pawn ? Pawn->GetPlayerState() : nullptr;
	FString PlayerName = PlayerState ? PlayerState->GetPlayerName() : FString();
	if (PlayerName.IsEmpty() && Pawn)
		PlayerName = Pawn->GetClass()->GetDisplayNameText().ToString();
	if (!CachedPlayerName.IsSet() || CachedPlayerName.GetValue() != PlayerName)
	{
		CachedPlayerName = MoveTemp(PlayerName);
		CharacterName->SetText(FText::AsCultureInvariant(CachedPlayerName.GetValue()));
	}
}
