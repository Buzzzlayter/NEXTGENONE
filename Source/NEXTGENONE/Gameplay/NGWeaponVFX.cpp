#include "NGWeaponVFX.h"
#include "NGCombat.h"
#include "NextgenoneCharacter.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
void StopSpawningAfter(UNiagaraComponent* FX,float Delay)
{
	if (!FX || !FX->GetWorld()) return;
	TWeakObjectPtr<UNiagaraComponent> Weak=FX; FTimerHandle Stop,Destroy;
	FX->GetWorld()->GetTimerManager().SetTimer(Stop,[Weak](){ if (Weak.IsValid()) Weak->Deactivate(); },Delay,false);
	FX->GetWorld()->GetTimerManager().SetTimer(Destroy,[Weak](){ if (Weak.IsValid()) Weak->DestroyComponent(); },3.f,false);
}
UNiagaraComponent* Attached(UNiagaraSystem* System,USkeletalMeshComponent* Mesh,FName Socket,FVector Fallback,FRotator Rotation)
{
	if (!System || !Mesh) return nullptr;
	if (Mesh->DoesSocketExist(Socket)) return UNiagaraFunctionLibrary::SpawnSystemAttached(System,Mesh,Socket,FVector::ZeroVector,FRotator::ZeroRotator,EAttachLocation::SnapToTarget,true,false);
	return UNiagaraFunctionLibrary::SpawnSystemAtLocation(Mesh,System,Fallback,Rotation,FVector::OneVector,true,false);
}
}
void NGWeaponVFX::Shot(ANextgenoneCharacter* Pawn,const UNGWeaponDefinition* Weapon,FVector Start,FVector Direction)
{
	if (!Pawn || !Weapon || Pawn->GetNetMode()==NM_DedicatedServer) return;
	if (auto* FX=Attached(Weapon->MuzzleFX,Pawn->SkeletalMesh,Weapon->MuzzleFXSocket,Start,Direction.Rotation()))
	{
		FX->SetVariableVec3(TEXT("User.Direction"),Direction); FX->SetVariableBool(TEXT("User.Trigger"),true); FX->Activate(true); StopSpawningAfter(FX,.08f);
	}
	if (auto* FX=Attached(Weapon->ShellEjectFX,Pawn->SkeletalMesh,Weapon->ShellEjectFXSocket,Start,Direction.Rotation()))
	{
		if (Weapon->ShellMesh) FX->SetVariableStaticMesh(TEXT("User.ShellEjectStaticMesh"),Weapon->ShellMesh);
		FX->SetVariableBool(TEXT("User.Trigger"),true); FX->Activate(true); StopSpawningAfter(FX,.08f);
	}
}
void NGWeaponVFX::Impact(UObject* Context,const UNGWeaponDefinition* Weapon,FVector Point,FVector Normal,bool bCharacter)
{
	if (!Context || !Context->GetWorld() || Context->GetWorld()->GetNetMode()==NM_DedicatedServer || !Weapon) return;
	auto* System=bCharacter && Weapon->CharacterImpactFX ? Weapon->CharacterImpactFX.Get() : Weapon->ImpactFX.Get();
	if (auto* FX=UNiagaraFunctionLibrary::SpawnSystemAtLocation(Context,System,Point,Normal.Rotation(),FVector::OneVector,true,false))
	{
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayPosition(FX,TEXT("User.ImpactPositions"),{Point});
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(FX,TEXT("User.ImpactNormals"),{Normal});
		if (System!=Weapon->CharacterImpactFX) { FX->SetVariablePosition(TEXT("User.MuzzlePosition"),Point-Normal*10.f); FX->SetVariableInt(TEXT("User.StartOffset"),0); }
		FX->SetVariableInt(TEXT("User.NumberOfHits"),1);
		FX->Activate(true); StopSpawningAfter(FX,.08f);
	}
}
UNiagaraComponent* NGWeaponVFX::CreateTracer(UObject* Context,UNiagaraSystem* System,FVector Position)
{
	if (!Context || !Context->GetWorld() || Context->GetWorld()->GetNetMode()==NM_DedicatedServer || !System) return nullptr;
	auto* FX=UNiagaraFunctionLibrary::SpawnSystemAtLocation(Context,System,Position,FRotator::ZeroRotator,FVector::OneVector,false,false);
	if (FX) { UpdateTracer(FX,Position,Position,false); FX->Activate(true); } return FX;
}
void NGWeaponVFX::UpdateTracer(UNiagaraComponent* FX,FVector From,FVector To,bool bTrigger)
{
	if (!FX) return;
	FX->SetVariablePosition(TEXT("User.MuzzlePosition"),From);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(FX,TEXT("User.ImpactPositions"),{To});
	FX->SetVariableBool(TEXT("User.Trigger"),bTrigger); FX->SetWorldLocation(From);
}
void NGWeaponVFX::StopTracer(UNiagaraComponent* Component)
{
	if (!IsValid(Component)) return;
	Component->SetAutoDestroy(true); Component->Deactivate(); StopSpawningAfter(Component,.01f);
}
