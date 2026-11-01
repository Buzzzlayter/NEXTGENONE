#pragma once
#include "CoreMinimal.h"
class UNiagaraComponent;
class UNiagaraSystem;
class UNGWeaponDefinition;
class ANextgenoneCharacter;
namespace NGWeaponVFX
{
	void Shot(ANextgenoneCharacter* Pawn,const UNGWeaponDefinition* Weapon,FVector Start,FVector Direction);
	void Impact(UObject* Context,const UNGWeaponDefinition* Weapon,FVector Point,FVector Normal,bool bCharacter);
	UNiagaraComponent* CreateTracer(UObject* Context,UNiagaraSystem* System,FVector Position);
	void UpdateTracer(UNiagaraComponent* Component,FVector From,FVector To,bool bTrigger);
	void StopTracer(UNiagaraComponent* Component);
}
