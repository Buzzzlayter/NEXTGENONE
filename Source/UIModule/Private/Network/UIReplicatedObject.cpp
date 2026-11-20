#include "Network/UIReplicatedObject.h"

#include "Engine/BlueprintGeneratedClass.h"
#include "Iris/ReplicationSystem/ReplicationFragmentUtil.h"
#include "Net/UnrealNetwork.h"

void UUIReplicatedObject::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	if (const auto* BlueprintClass = Cast<UBlueprintGeneratedClass>(GetClass()))
		BlueprintClass->GetLifetimeBlueprintReplicationList(OutLifetimeProps);
}

void UUIReplicatedObject::RegisterReplicationFragments(UE::Net::FFragmentRegistrationContext& Context,
	UE::Net::EFragmentRegistrationFlags RegistrationFlags)
{
	UE::Net::FReplicationFragmentUtil::CreateAndRegisterFragmentsForObject(this, Context, RegistrationFlags);
}
