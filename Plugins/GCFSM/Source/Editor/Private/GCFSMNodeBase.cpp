// Copyright (c) 2018 Gamecentric, All rights reserved

#include "GCFSMNodeBase.h"
#include "GCFSMStartNode.h"
#include "GCFSMGraphValidator.h"

UEdGraphPin* UGCFSMNodeBase::GetFSMPin(FKismetCompilerContext& compilerContext)
{
	if (!startNode.IsSet())
	{
		FGraphValidator::ValidateFrom(compilerContext, this);
	}
	return startNode.GetValue() ? startNode.GetValue()->GetFSMPin() : nullptr;
}
