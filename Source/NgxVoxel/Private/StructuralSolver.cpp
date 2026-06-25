// Copyright. NgxVoxel — structural connectivity solver (VS0 step 6).

#include "StructuralSolver.h"
#include "Containers/Deque.h"

namespace
{
	FORCEINLINE int32 FlatIdx(int32 X, int32 Y, int32 Z, const FIntVector& D)
	{
		return X + Y * D.X + Z * D.X * D.Y;
	}

	const FIntVector Dirs6[6] = {
		{ 1, 0, 0 }, { -1, 0, 0 },
		{ 0, 1, 0 }, { 0, -1, 0 },
		{ 0, 0, 1 }, { 0, 0, -1 }
	};
}

void FStructuralSolver::FindDetachedComponents(
	const FIntVector& Dims,
	TFunctionRef<bool(int32, int32, int32)> IsSolid,
	TFunctionRef<bool(int32, int32, int32)> IsAnchor,
	TArray<TArray<FIntVector>>& OutComponents)
{
	OutComponents.Reset();

	const int32 NX = Dims.X, NY = Dims.Y, NZ = Dims.Z;
	if (NX <= 0 || NY <= 0 || NZ <= 0)
	{
		return;
	}

	// State: 0 = пусто/не интересно, 1 = solid непосещён, 2 = посещён.
	TArray<uint8> State;
	State.SetNumZeroed(NX * NY * NZ);

	for (int32 Z = 0; Z < NZ; ++Z)
	for (int32 Y = 0; Y < NY; ++Y)
	for (int32 X = 0; X < NX; ++X)
	{
		if (IsSolid(X, Y, Z))
		{
			State[FlatIdx(X, Y, Z, Dims)] = 1;
		}
	}

	// BFS от всех якорных solid-вокселей → достижимое помечаем как 2 (связано с «землёй»).
	TArray<FIntVector> Queue;
	for (int32 Z = 0; Z < NZ; ++Z)
	for (int32 Y = 0; Y < NY; ++Y)
	for (int32 X = 0; X < NX; ++X)
	{
		const int32 I = FlatIdx(X, Y, Z, Dims);
		if (State[I] == 1 && IsAnchor(X, Y, Z))
		{
			State[I] = 2;
			Queue.Add(FIntVector(X, Y, Z));
		}
	}

	int32 Head = 0;
	while (Head < Queue.Num())
	{
		const FIntVector P = Queue[Head++];
		for (const FIntVector& D : Dirs6)
		{
			const int32 X = P.X + D.X, Y = P.Y + D.Y, Z = P.Z + D.Z;
			if (X < 0 || X >= NX || Y < 0 || Y >= NY || Z < 0 || Z >= NZ)
			{
				continue;
			}
			const int32 I = FlatIdx(X, Y, Z, Dims);
			if (State[I] == 1)
			{
				State[I] = 2;
				Queue.Add(FIntVector(X, Y, Z));
			}
		}
	}

	// Оставшиеся State==1 — оторваны. Группируем в связные компоненты (flood-fill).
	TArray<FIntVector> CompQueue;
	for (int32 Z = 0; Z < NZ; ++Z)
	for (int32 Y = 0; Y < NY; ++Y)
	for (int32 X = 0; X < NX; ++X)
	{
		if (State[FlatIdx(X, Y, Z, Dims)] != 1)
		{
			continue;
		}

		TArray<FIntVector>& Comp = OutComponents.AddDefaulted_GetRef();
		State[FlatIdx(X, Y, Z, Dims)] = 2;
		CompQueue.Reset();
		CompQueue.Add(FIntVector(X, Y, Z));

		int32 H = 0;
		while (H < CompQueue.Num())
		{
			const FIntVector P = CompQueue[H++];
			Comp.Add(P);
			for (const FIntVector& D : Dirs6)
			{
				const int32 CX = P.X + D.X, CY = P.Y + D.Y, CZ = P.Z + D.Z;
				if (CX < 0 || CX >= NX || CY < 0 || CY >= NY || CZ < 0 || CZ >= NZ)
				{
					continue;
				}
				const int32 I = FlatIdx(CX, CY, CZ, Dims);
				if (State[I] == 1)
				{
					State[I] = 2;
					CompQueue.Add(FIntVector(CX, CY, CZ));
				}
			}
		}
	}
}

void FStructuralSolver::FindUnsupportedComponents(
	const FIntVector& Dims,
	int32 MaxCantilever,
	TFunctionRef<bool(int32, int32, int32)> IsSolid,
	TFunctionRef<bool(int32, int32, int32)> IsAnchor,
	TArray<TArray<FIntVector>>& OutComponents)
{
	OutComponents.Reset();

	const int32 NX = Dims.X, NY = Dims.Y, NZ = Dims.Z;
	if (NX <= 0 || NY <= 0 || NZ <= 0)
	{
		return;
	}
	const int32 Count = NX * NY * NZ;
	const int32 MaxC = FMath::Max(MaxCantilever, 0);

	// Маска solid.
	TArray<uint8> Solid;
	Solid.SetNumZeroed(Count);
	for (int32 Z = 0; Z < NZ; ++Z)
	for (int32 Y = 0; Y < NY; ++Y)
	for (int32 X = 0; X < NX; ++X)
	{
		if (IsSolid(X, Y, Z))
		{
			Solid[FlatIdx(X, Y, Z, Dims)] = 1;
		}
	}

	// Dist = минимальный «вынос консоли» до якоря. MAX_int32 = недостижим (тоже неопёрт).
	TArray<int32> Dist;
	Dist.Init(MAX_int32, Count);

	// 0-1 BFS: вертикальные рёбра — вес 0 (в начало деки), горизонтальные — вес 1 (в конец).
	TDeque<FIntVector> Dq;
	for (int32 Z = 0; Z < NZ; ++Z)
	for (int32 Y = 0; Y < NY; ++Y)
	for (int32 X = 0; X < NX; ++X)
	{
		const int32 I = FlatIdx(X, Y, Z, Dims);
		if (Solid[I] && IsAnchor(X, Y, Z))
		{
			Dist[I] = 0;
			Dq.PushLast(FIntVector(X, Y, Z));
		}
	}

	auto Relax = [&](int32 X, int32 Y, int32 Z, int32 NewDist, bool bZeroWeight)
	{
		if (X < 0 || X >= NX || Y < 0 || Y >= NY || Z < 0 || Z >= NZ)
		{
			return;
		}
		const int32 I = FlatIdx(X, Y, Z, Dims);
		if (!Solid[I] || NewDist >= Dist[I])
		{
			return;
		}
		Dist[I] = NewDist;
		if (bZeroWeight) { Dq.PushFirst(FIntVector(X, Y, Z)); }
		else             { Dq.PushLast(FIntVector(X, Y, Z)); }
	};

	while (!Dq.IsEmpty())
	{
		const FIntVector P = Dq.First();
		Dq.PopFirst();
		const int32 D = Dist[FlatIdx(P.X, P.Y, P.Z, Dims)];

		// Вертикаль — бесплатно (колонна опирается сама); горизонталь дальше MaxC не пускаем.
		Relax(P.X, P.Y, P.Z + 1, D, true);
		Relax(P.X, P.Y, P.Z - 1, D, true);
		if (D < MaxC)
		{
			Relax(P.X + 1, P.Y, P.Z, D + 1, false);
			Relax(P.X - 1, P.Y, P.Z, D + 1, false);
			Relax(P.X, P.Y + 1, P.Z, D + 1, false);
			Relax(P.X, P.Y - 1, P.Z, D + 1, false);
		}
	}

	// Неопёрты = solid с Dist > MaxC (включая недостижимые). Группируем в связные компоненты.
	// (направления 6-связности — общий Dirs6 из анонимного namespace вверху файла)
	TArray<uint8> Visited;
	Visited.SetNumZeroed(Count);
	TArray<FIntVector> Stack;

	for (int32 Z = 0; Z < NZ; ++Z)
	for (int32 Y = 0; Y < NY; ++Y)
	for (int32 X = 0; X < NX; ++X)
	{
		const int32 I = FlatIdx(X, Y, Z, Dims);
		if (!Solid[I] || Dist[I] <= MaxC || Visited[I])
		{
			continue;
		}

		TArray<FIntVector>& Comp = OutComponents.AddDefaulted_GetRef();
		Visited[I] = 1;
		Stack.Reset();
		Stack.Add(FIntVector(X, Y, Z));

		while (Stack.Num() > 0)
		{
			const FIntVector P = Stack.Pop(EAllowShrinking::No);
			Comp.Add(P);
			for (const FIntVector& Dir : Dirs6)
			{
				const int32 CX = P.X + Dir.X, CY = P.Y + Dir.Y, CZ = P.Z + Dir.Z;
				if (CX < 0 || CX >= NX || CY < 0 || CY >= NY || CZ < 0 || CZ >= NZ)
				{
					continue;
				}
				const int32 CI = FlatIdx(CX, CY, CZ, Dims);
				if (Solid[CI] && Dist[CI] > MaxC && !Visited[CI])
				{
					Visited[CI] = 1;
					Stack.Add(FIntVector(CX, CY, CZ));
				}
			}
		}
	}
}
