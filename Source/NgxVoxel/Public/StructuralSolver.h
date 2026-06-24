// Copyright. NgxVoxel — structural connectivity solver (VS0 step 6).

#pragma once

#include "CoreMinimal.h"

// Бинарная связность: находит solid-воксели, не достижимые (6-связность) ни от одного якоря,
// и группирует их в связные компоненты («оторванные куски»).
// Stateless, без игровых зависимостей. Координаты — ГЛОБАЛЬНЫЕ воксельные в [0,Dims).
class NGXVOXEL_API FStructuralSolver
{
public:
	// IsSolid/IsAnchor — предикаты по глобальным воксельным координатам. IsAnchor проверяется
	// только для solid-вокселей. OutComponents — список компонент оторванных вокселей.
	static void FindDetachedComponents(
		const FIntVector& Dims,
		TFunctionRef<bool(int32, int32, int32)> IsSolid,
		TFunctionRef<bool(int32, int32, int32)> IsAnchor,
		TArray<TArray<FIntVector>>& OutComponents);
};
