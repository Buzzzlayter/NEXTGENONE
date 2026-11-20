#pragma once

#include "UObject/Object.h"
#include "UIReplicatedObject.generated.h"

UCLASS(Abstract)
class UIMODULE_API UUIReplicatedObject : public UObject
{
	GENERATED_BODY()

public:
	virtual bool IsSupportedForNetworking() const override { return true; }
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void RegisterReplicationFragments(UE::Net::FFragmentRegistrationContext& Context,
		UE::Net::EFragmentRegistrationFlags RegistrationFlags) override;
};
