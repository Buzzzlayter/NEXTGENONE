// Copyright (c) 2018 Gamecentric, All rights reserved

#pragma once

#include "GCFSMEditorModule.h"
#include <set>
#include <map>
#include <memory>

class UEdGraphNode;
class UGCFSMStartNode;
class UGCFSMNodeBase;

class FGraphValidator : public std::enable_shared_from_this<FGraphValidator>
{
	struct PrivateTag {};

public:
	FGraphValidator(PrivateTag)
	{}

	const std::set<UGCFSMNodeBase*>& GetFSMNodes() const { return visitedFSMNodes; }

	static void ValidateFrom(FKismetCompilerContext& compilerContext, UEdGraphNode* testNode);

private:
	std::set<UEdGraphNode*> visitedNodes;
	std::set<UGCFSMNodeBase*> visitedFSMNodes;
	std::set<UGCFSMStartNode*> startNodes;

	enum BadReason
	{
		NoStopNode = 1,
		TickPinAttached = 2
	};
	std::map<UEdGraphNode*, int> badNodes;

	void CollectNodes(UEdGraphNode* node);
	void Validate(FKismetCompilerContext& compilerContext, UEdGraphNode* testNode);
};
