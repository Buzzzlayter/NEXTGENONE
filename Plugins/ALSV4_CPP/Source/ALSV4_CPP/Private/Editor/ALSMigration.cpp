// Editor-only migration support for the original UE 5.1 ALS animation Blueprint.
#if WITH_EDITOR

#include "AnimGraphNode_StateMachine.h"
#include "AnimStateTransitionNode.h"
#include "Animation/AnimBlueprint.h"
#include "EdGraph/EdGraph.h"
#include "HAL/IConsoleManager.h"
#include "K2Node_AnimGetter.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
void RepairALSAnimationBlueprint()
{
	UAnimBlueprint* Blueprint = LoadObject<UAnimBlueprint>(nullptr,
		TEXT("/ALSV4_CPP/AdvancedLocomotionV4/CharacterAssets/MannequinSkeleton/ALS_AnimBP.ALS_AnimBP"));
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("ALS migration: animation Blueprint not found"));
		return;
	}

	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	int32 Repaired = 0;
	for (UEdGraph* Graph : Graphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UK2Node_AnimGetter* Getter = Cast<UK2Node_AnimGetter>(Node);
			if (!Getter || Getter->SourceNode ||
				Getter->FunctionReference.GetMemberName() != TEXT("GetInstanceCurrentStateElapsedTime"))
			{
				continue;
			}
			UAnimStateTransitionNode* Transition = Cast<UAnimStateTransitionNode>(Graph->GetOuter());
			UAnimStateNodeBase* State = Transition ? Transition->GetPreviousState() : nullptr;
			UAnimGraphNode_StateMachine* Machine = State
				? Cast<UAnimGraphNode_StateMachine>(State->GetOuter()->GetOuter()) : nullptr;
			if (!Machine)
			{
				continue;
			}
			Blueprint->Modify();
			Getter->Modify();
			Getter->SourceStateNode = State;
			Getter->SourceNode = Machine;
			++Repaired;
		}
	}

	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	if (Blueprint->Status == BS_Error)
	{
		UE_LOG(LogTemp, Error, TEXT("ALS migration: Blueprint compilation failed; asset was not saved"));
		return;
	}
	UPackage* Package = Blueprint->GetOutermost();
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	const FString Filename = FPackageName::LongPackageNameToFilename(
		Package->GetName(), FPackageName::GetAssetPackageExtension());
	if (UPackage::SavePackage(Package, Blueprint, *Filename, SaveArgs))
	{
		UE_LOG(LogTemp, Display, TEXT("ALS_MIGRATION_SUCCESS: repaired %d animation getter(s), Blueprint compiled and saved"), Repaired);
	}
}

FAutoConsoleCommand RepairCommand(
	TEXT("ALS.RepairAnimationBlueprint"),
	TEXT("Restore missing state-machine references in the imported ALS animation Blueprint and save it."),
	FConsoleCommandDelegate::CreateStatic(&RepairALSAnimationBlueprint));
}

#endif
