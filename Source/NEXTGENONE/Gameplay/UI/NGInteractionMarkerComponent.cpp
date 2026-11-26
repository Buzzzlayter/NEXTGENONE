#include "Gameplay/UI/NGInteractionMarkerComponent.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/Interaction/NGInteraction.h"
#include "Gameplay/UI/NGInteractionMarkerModel.h"
#include "Gameplay/UI/NGInteractionWidget.h"
#include "Map/UIMarkerSubsystem.h"

UNGInteractionMarkerComponent::UNGInteractionMarkerComponent()
{
	ModelClass = UNGInteractionMarkerModel::StaticClass();
	SetWidgetClass(UNGInteractionWidget::StaticClass());
}

void UNGInteractionMarkerComponent::BeginPlay()
{
	Super::BeginPlay();
	if (auto* Interactable = Cast<ANGInteractableActor>(GetOwner()))
		Interactable->OnInteractionFocus.AddUObject(this, &ThisClass::OnInteractionFocusChanged);
}

void UNGInteractionMarkerComponent::EndPlay(EEndPlayReason::Type Reason)
{
	if (auto* Interactable = Cast<ANGInteractableActor>(GetOwner()))
		Interactable->OnInteractionFocus.RemoveAll(this);
	for (const auto& Pair : Presentations)
		DestroyPresentation(Pair.Value.Get());
	Presentations.Reset();
	Super::EndPlay(Reason);
}

void UNGInteractionMarkerComponent::OnInteractionFocusChanged(APawn* Interactor, bool bFocused, const FText& Prompt, bool bAvailable)
{
	if (!bFocused)
	{
		RemovePresentation(Interactor);
		return;
	}

	if (auto* Existing = Presentations.FindRef(Interactor).Get())
	{
		if (auto* Model = Cast<UNGInteractionMarkerModel>(Existing->GetMarkerModel()))
		{
			Model->SetFocus(Interactor, Prompt, bAvailable);
			return;
		}
		RemovePresentation(Interactor);
	}

	auto* Viewer = Interactor ? Cast<APlayerController>(Interactor->GetController()) : nullptr;
	auto* LocalPlayer = Viewer ? Viewer->GetLocalPlayer() : nullptr;
	auto* Subsystem = UWorld::GetSubsystem<UUIMarkerSubsystem>(GetWorld());
	const TSubclassOf<UUIMarkerWidget> PresentationClass(GetWidgetClass().Get());
	if (!LocalPlayer || !Subsystem || !ModelClass || !PresentationClass) return;

	auto* Model = Cast<UNGInteractionMarkerModel>(UUIMarkerModel::CreateRuntimeModel(
		Subsystem, ModelClass.GetDefaultObject(), GetComponentLocation()));
	if (!Model) return;
	Model->SetAnchor(this);
	Model->SetViewer(LocalPlayer);
	Model->SetFocus(Interactor, Prompt, bAvailable);

	auto* Presentation = NewObject<UUIWorldMarkerComponent>(Interactor, NAME_None, RF_Transient);
	Presentation->SetRelativeScale3D(GetComponentScale());
	Presentation->RegisterComponent();
	if (Presentation->ShowMarker(Viewer, Model, PresentationClass))
		Presentations.Add(Interactor, Presentation);
	else
	{
		Model->SetViewer(nullptr);
		Presentation->DestroyComponent();
	}
}

void UNGInteractionMarkerComponent::RemovePresentation(APawn* Interactor)
{
	TWeakObjectPtr<UUIWorldMarkerComponent> Presentation;
	if (Presentations.RemoveAndCopyValue(Interactor, Presentation))
		DestroyPresentation(Presentation.Get());
}

void UNGInteractionMarkerComponent::DestroyPresentation(UUIWorldMarkerComponent* Presentation)
{
	if (!Presentation) return;
	if (auto* Model = Cast<UNGInteractionMarkerModel>(Presentation->GetMarkerModel())) Model->SetViewer(nullptr);
	Presentation->DestroyComponent();
}
