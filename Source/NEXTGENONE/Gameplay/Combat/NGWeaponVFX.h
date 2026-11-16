#pragma once
#include "CoreMinimal.h"
class UNiagaraComponent;
class UNiagaraSystem;
class UNGWeaponDefinition;
class USkeletalMeshComponent;
namespace NGWeaponVFX
{
	void Shot(USkeletalMeshComponent* Mesh,const UNGWeaponDefinition* Weapon,FVector Start,FVector Direction);
	void Impact(UObject* Context,UNiagaraSystem* System,FVector Point,FVector Normal,bool bCharacterEffect);
	UNiagaraComponent* CreateTracer(UObject* Context,UNiagaraSystem* System,FVector Position);
	void UpdateTracer(UNiagaraComponent* Component,FVector From,FVector To,bool bTrigger);
	void StopTracer(UNiagaraComponent* Component);
}
