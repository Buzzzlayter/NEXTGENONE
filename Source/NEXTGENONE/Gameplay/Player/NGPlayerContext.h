#pragma once

#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerState.h"
#include "Gameplay/StateMachine/NGCharacterStateMachineComponent.h"

namespace NGPlayerContext
{
    /** Current possessed player Avatar only. Proxy presentation must not use this authority/input gate. */
    inline UAbilitySystemComponent* GetCurrentASC(const APawn* Pawn, bool bRequireAuthority)
    {
        if (!IsValid(Pawn) || (bRequireAuthority && !Pawn->HasAuthority())) return nullptr;
        if (const auto* Machine = Pawn->FindComponentByClass<UNGCharacterStateMachineComponent>(); Machine && Machine->HasResourceFailure()) return nullptr;
        const auto* Controller = Pawn->GetController();
        const auto* Player = Pawn->GetPlayerState();
        if (!Controller || Controller->GetPawn() != Pawn || !Player || Player->GetPawn() != Pawn) return nullptr;
        auto* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(const_cast<APawn*>(Pawn));
        return ASC && ASC->GetAvatarActor() == Pawn && ASC->GetOwnerActor() == Player ? ASC : nullptr;
    }
}
