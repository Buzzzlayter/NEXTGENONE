#include "NGInventoryPanel.h"
#include "NGInventoryItemInstance.h"
#include "NGInventoryItemDefinition.h"
#include "Gameplay/Character/NextgenoneCharacter.h"
#include "Gameplay/Player/NextgenonePlayerState.h"
#include "Gameplay/Combat/NGCombat.h"
#include "Gameplay/Player/NextgenonePlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/HorizontalBox.h"

void UNGInventoryItemButton::InitializeItem(UNGInventoryItemInstance* InItem, APlayerController* InController,bool bInDrop)
{
	Item = InItem; Controller = InController; bDrop=bInDrop;
	OnClicked.AddDynamic(this, &ThisClass::Equip);
}
void UNGInventoryItemButton::Equip()
{
	if (bDrop) { if (auto* PC=Cast<ANextgenonePlayerController>(Controller.Get())) PC->RequestDropItem(Item.Get(),0); return; }
	if (auto* PC = Controller.Get()) if (auto* Pawn = Cast<ANextgenoneCharacter>(PC->GetPawn())) Pawn->Combat->RequestEquipItem(Item.Get());
}
TSharedRef<SWidget> UNGInventoryPanel::RebuildWidget()
{
	if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	if (!ItemsList) ItemsList = Cast<UVerticalBox>(WidgetTree->FindWidget(TEXT("ItemsList")));
	if (!WidgetTree->RootWidget)
	{
		auto* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
		ItemsList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ItemsList"));
		Scroll->AddChild(ItemsList); WidgetTree->RootWidget = Scroll;
	}
	return Super::RebuildWidget();
}
void UNGInventoryPanel::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	const auto* PC = GetOwningPlayer();
	const auto* PS = PC ? PC->GetPlayerState<ANextgenonePlayerState>() : nullptr;
	if (PS)
	{
		Inventory = PS->Inventory;
		Inventory->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::Changed);
	}
	Refresh();
}
void UNGInventoryPanel::UnbindItems()
{
	for (auto Item : ObservedItems) if (Item.IsValid()) Item->OnStatsChanged.RemoveDynamic(this, &ThisClass::Refresh);
	ObservedItems.Reset();
}
void UNGInventoryPanel::NativeDestruct()
{
	UnbindItems();
	if (Inventory.IsValid()) Inventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::Changed);
	Inventory.Reset(); Super::NativeDestruct();
}
void UNGInventoryPanel::Changed(FNGInventoryChangeMessage Change) { Refresh(); }
void UNGInventoryPanel::Refresh()
{
	if (!ItemsList) return;
	UnbindItems(); ItemsList->ClearChildren();
	if (!Inventory.IsValid()) return;
	for (auto* Item : Inventory->GetAllItems())
	{
		if (!Item || !Item->GetItemDef()) continue;
		const auto* Def = GetDefault<UNGInventoryItemDefinition>(Item->GetItemDef());
		auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>();
		auto* Label = WidgetTree->ConstructWidget<UTextBlock>();
		FString Text=FString::Printf(TEXT("%s x%d"),*Def->DisplayName.ToString(),Inventory->GetCount(Item));
		if (Def->Weapon) Text+=FString::Printf(TEXT("    [%d / %d]"),Item->GetStatTagStackCount(TAG_NGMagazineAmmo),Inventory->GetTotalItemCountByDefinition(Def->Weapon->AmmoItemClass.LoadSynchronous()));
		Label->SetText(FText::FromString(Text));
		if (Def->Weapon)
		{
			auto* Button=WidgetTree->ConstructWidget<UNGInventoryItemButton>(); Button->InitializeItem(Item,GetOwningPlayer()); Button->AddChild(Label); Row->AddChild(Button);
		}
		else Row->AddChild(Label);
		auto* Drop=WidgetTree->ConstructWidget<UNGInventoryItemButton>(); Drop->InitializeItem(Item,GetOwningPlayer(),true);
		auto* DropLabel=WidgetTree->ConstructWidget<UTextBlock>(); DropLabel->SetText(NSLOCTEXT("NEXTGENONE","DropItem","Выбросить"));
		Drop->AddChild(DropLabel); Row->AddChild(Drop); ItemsList->AddChild(Row);
		Item->OnStatsChanged.AddUniqueDynamic(this, &ThisClass::Refresh); ObservedItems.Add(Item);
	}
}
