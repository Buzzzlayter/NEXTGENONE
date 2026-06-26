// Copyright (c) 2018 Gamecentric, All rights reserved

#include "GCFSMGraphValidator.h"
#include "GCFSMEditorModule.h"
#include "GCFSMNodeBase.h"
#include "GCFSMStartNode.h"
#include "GCFSMStateNode.h"
#include "GCFSMEditorUtilities.h"
#include "KismetCompiler.h"

void FGraphValidator::ValidateFrom(FKismetCompilerContext& compilerContext, UEdGraphNode* testNode)
{
	auto validator = std::make_shared<FGraphValidator>(PrivateTag {});
	validator->Validate(compilerContext, testNode);
}

void FGraphValidator::Validate(FKismetCompilerContext& compilerContext, UEdGraphNode* testNode)
{
	// Collect all nodes in this connected subgraph
	visitedNodes = { testNode };
	CollectNodes(testNode);

	// detect start node
	int multiplicity;
	UGCFSMStartNode* startNode = nullptr;
	if (startNodes.size() == 0 || startNodes.count(nullptr))
	{
		multiplicity = 0;
	}
	else if (startNodes.size() > 1)
	{
		multiplicity = 2;
	}
	else
	{
		multiplicity = 1;
		startNode = *startNodes.begin();
	}

	// notify regular FSM nodes
	for (auto baseNode : visitedFSMNodes)
	{
		switch (multiplicity)
		{
		case 0:
			compilerContext.MessageLog.Error(TEXT("An execution path leading to @@ does not start with a FSM Start node"), baseNode);
			break;

		case 2:
			compilerContext.MessageLog.Error(TEXT("@@ is connected to more than one FSM Start nodes"), baseNode);
			break;
		}

		baseNode->startNode = startNode;
	}

	// notify start nodes (beware of nullptr)
	for (auto node : startNodes)
	{
		if (node)
		{
			node->graphValidator = shared_from_this();
		}
	}
	
	// warn about bad nodes
	for (auto badNode : badNodes)
	{
		if (badNode.second & BadReason::NoStopNode)
		{
			// TODO: we are temporariliy disabling this warning, since it gives false positives in legitimate cases, e.g. if a sequence node is used
			// compilerContext.MessageLog.Warning(TEXT("@@ has an unconnected outgoing exec pin, the FSM will hang if the pin is executed, please add a FSM Stop node"), badNode.first);
		}
		
		if (badNode.second & BadReason::TickPinAttached)
		{
			compilerContext.MessageLog.Error(TEXT("The execution path flowing off the Tick pin of @@ shall not potentially execute other FSM nodes"), badNode.first);
		}
	}
}

void FGraphValidator::CollectNodes(UEdGraphNode* node)
{
	bool isStateNode = false;
	if (auto baseNode = Cast<UGCFSMNodeBase>(node))
	{
		visitedFSMNodes.insert(baseNode);
		isStateNode = baseNode->IsA<UGCFSMStateNode>();
	}

	int execPins[2] = { 0, 0 };
	bool emptyExecPins[2] = { false, false };
	for (auto pin : node->Pins)
	{
		if (pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{
			// ignore the output Tick pin of a UGCFSMStateNode
			if (isStateNode && UGCFSMStateNode::IsTickPin(pin))
			{
				continue;
			}

			++execPins[pin->Direction];

			if (pin->LinkedTo.Num() == 0)
			{
				emptyExecPins[pin->Direction] = true;
			}
			else
			{
				for (auto linkedPin : pin->LinkedTo)
				{
					if (auto nextNode = linkedPin->GetOwningNode())
					{
						// attaching to an output Tick pin of a UGCFSMStateNode is bad
						if (nextNode->IsA<UGCFSMStateNode>() && UGCFSMStateNode::IsTickPin(linkedPin))
						{
							badNodes[nextNode] |= BadReason::TickPinAttached;
						}
						else if (visitedNodes.insert(nextNode).second)
						{
							CollectNodes(nextNode);
						}
					}
				}
			}
		}
	}

	if (execPins[EGPD_Input] == 0)
	{
		// it's an entry node
		startNodes.insert(Cast<UGCFSMStartNode>(node));
	}

	if (emptyExecPins[EGPD_Output])
	{
		// it has unattached output exec pins
		badNodes[node] |= BadReason::NoStopNode;
	}
}
