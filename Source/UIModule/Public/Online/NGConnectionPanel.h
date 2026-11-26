#pragma once
#include "CoreMinimal.h"
#include "Components/UIStateWidget.h"
#include "NGConnectionPanel.generated.h"
class UEditableTextBox;
class UTextBlock;
class UButton;
class UVerticalBox;
class UNGConnectionSubsystem;
UCLASS(Blueprintable)
class UIMODULE_API UNGConnectionPanel : public UUIStateWidget
{
	GENERATED_BODY()
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
private:
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> Address;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Message;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PartyHeading;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PlayLabel;
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> PartyCodeInput;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> PartyRoster;
	UPROPERTY(Transient) TObjectPtr<UButton> SteamButton;
	UPROPERTY(Transient) TObjectPtr<UButton> EpicButton;
	UPROPERTY(Transient) TObjectPtr<UButton> CreateButton;
	UPROPERTY(Transient) TObjectPtr<UButton> JoinButton;
	UPROPERTY(Transient) TObjectPtr<UButton> PlayButton;
	UPROPERTY(Transient) TObjectPtr<UButton> LeaveButton;
	TWeakObjectPtr<UNGConnectionSubsystem> Connection;
	FDelegateHandle PartyChangedHandle;
	UFUNCTION() void Connect();
	UFUNCTION() void Disconnect();
	UFUNCTION() void StatusChanged(FText Text);
	UFUNCTION() void SignInSteam();
	UFUNCTION() void SignInEpic();
	UFUNCTION() void CreateParty();
	UFUNCTION() void JoinParty();
	UFUNCTION() void PlayParty();
	UFUNCTION() void LeaveParty();
	void RefreshParty();
};
