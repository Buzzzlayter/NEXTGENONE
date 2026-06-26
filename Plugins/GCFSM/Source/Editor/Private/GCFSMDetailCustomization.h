// Copyright (c) 2018 Gamecentric, All rights reserved

#pragma once

#include "UObject/Interface.h"
#include "GCFSMCompatibility.h"
#include "GCFSMDetailCustomization.generated.h"

class UK2Node;
class UEdGraphPin;
class IDetailCustomization;

UINTERFACE()
class UGCFSMDetailCustomization : public UInterface
{
	GENERATED_BODY()
};

class IGCFSMDetailCustomization
{
	GENERATED_BODY()

public:
	static TSharedRef<IDetailCustomization> MakeInstance();

protected:
	struct PinProperty
	{
		FProperty* property;
		FString pinName;
		FString toolTip;
	};

	void SyncPropertyWithPin(UEdGraphPin* pin);
	void SyncPinWithProperty(UEdGraphPin* pin);
	void SyncPinWithProperty(FProperty* property);
	void SyncAllPins();
	void SyncAllProperties();

	bool IsPinVisible(UEdGraphPin* pin);

	virtual bool IsPinPropertyAvailable(const PinProperty& pinProperty) const { return true; }

private:
	UK2Node* node;
	TProperty<uint32, FNumericProperty>* visiblePins;
	TArray<PinProperty> pinProperties;

	void Initialize();

	const PinProperty* GetPinProperty(const FString& pinName);
	const PinProperty* GetPinProperty(FProperty* property);

	void SyncPin(const PinProperty& pinProperty);
	void SyncProperty(const PinProperty& pinProperty);

	void SetPinVisible(const PinProperty& pinProperty, bool visible);
	bool IsPinVisible(const PinProperty& pinProperty);

	uint32 MaskForProperty(const PinProperty& pinProperty);

	friend class FGCFSMDetailCustomization;
};
