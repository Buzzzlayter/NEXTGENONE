#include "Inventory/UIInventoryDragDropOperation.h"
#include "Engine/LocalPlayer.h"

void UUIInventoryDragDropOperation::Initialize(const FUIInventorySlotData& Source, ULocalPlayer* Player)
{
	SourceData = Source;
	SourcePlayer = Player;
}

bool UUIInventoryDragDropOperation::IsFromPlayer(const ULocalPlayer* Player) const
{
	return Player && SourcePlayer.Get() == Player;
}
