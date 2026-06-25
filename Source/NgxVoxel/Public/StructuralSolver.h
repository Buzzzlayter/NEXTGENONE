// Copyright. NgxVoxel — structural connectivity solver (VS0 step 6).

#pragma once

#include "CoreMinimal.h"

// Структурная опора. Stateless, без игровых зависимостей. Координаты — ГЛОБАЛЬНЫЕ воксельные в [0,Dims).
class NGXVOXEL_API FStructuralSolver
{
public:
	// Бинарная связность (6-связность от якорей): оторванное от якоря → падает. Держит нависания
	// (всё, что хоть как-то связано с якорной массой). Оставлено для A/B; реальный обвал — ниже.
	static void FindDetachedComponents(
		const FIntVector& Dims,
		TFunctionRef<bool(int32, int32, int32)> IsSolid,
		TFunctionRef<bool(int32, int32, int32)> IsAnchor,
		TArray<TArray<FIntVector>>& OutComponents);

	// Опора с учётом гравитации: воксель держится, если до якоря есть путь, где ВЕРТИКАЛЬНЫЕ шаги
	// бесплатны (стопка стоит сама), а ГОРИЗОНТАЛЬНЫЕ копят «вынос консоли». Если минимальный вынос
	// > MaxCantilever (или якорь недостижим вовсе) — воксель НЕ опёрт → обрушается. Неопёртые
	// группируются в связные компоненты (обваливающиеся массы → дробятся на глыбы вызывающим).
	static void FindUnsupportedComponents(
		const FIntVector& Dims,
		int32 MaxCantilever,
		TFunctionRef<bool(int32, int32, int32)> IsSolid,
		TFunctionRef<bool(int32, int32, int32)> IsAnchor,
		TArray<TArray<FIntVector>>& OutComponents);
};
