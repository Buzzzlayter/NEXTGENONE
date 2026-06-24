// Copyright. NgxVoxel — AVoxelStructure implementation (VS0).

#include "VoxelStructure.h"
#include "VoxelMesher.h"
#include "VoxelTypes.h"
#include "NgxVoxel.h"
#include "ProceduralMeshComponent.h"
#include "Async/Async.h"

namespace
{
	// Снимок данных для async-меширования одного чанка: центр + соседи копируются по значению,
	// поэтому воркер работает с неизменяемыми данными (нет гонок с правками на game thread).
	// Порядок соседей: 0=-X, 1=+X, 2=-Y, 3=+Y, 4=-Z, 5=+Z.
	struct FChunkSnapshot
	{
		FVoxelChunk Center;
		FVoxelChunk Neighbor[6];
		bool bHas[6] = { false, false, false, false, false, false };

		FChunkNeighbors MakeNeighbors() const
		{
			FChunkNeighbors N;
			N.Neg[0] = bHas[0] ? &Neighbor[0] : nullptr;
			N.Pos[0] = bHas[1] ? &Neighbor[1] : nullptr;
			N.Neg[1] = bHas[2] ? &Neighbor[2] : nullptr;
			N.Pos[1] = bHas[3] ? &Neighbor[3] : nullptr;
			N.Neg[2] = bHas[4] ? &Neighbor[4] : nullptr;
			N.Pos[2] = bHas[5] ? &Neighbor[5] : nullptr;
			return N;
		}
	};

	const FIntVector NeighborOffsets[6] = {
		{ -1, 0, 0 }, { 1, 0, 0 },
		{ 0, -1, 0 }, { 0, 1, 0 },
		{ 0, 0, -1 }, { 0, 0, 1 }
	};
}

AVoxelStructure::AVoxelStructure()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("VoxelMesh"));
	SetRootComponent(Mesh);
	Mesh->bUseAsyncCooking = true;
	// Разрушаемый воксель-меш не должен гонять навмеш (дорого + транзиентные пустые баунды
	// при async-ребилде → спам LogNavigation). Динамическую навигацию решим отдельно, если понадобится.
	Mesh->SetCanEverAffectNavigation(false);
}

void AVoxelStructure::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Rebuild();
}

void AVoxelStructure::EndPlay(const EEndPlayReason::Type Reason)
{
	RemoveTicker();
	Super::EndPlay(Reason);
}

void AVoxelStructure::BeginDestroy()
{
	RemoveTicker();
	Pipeline.Reset();   // воркеры в полёте держат свою копию TSharedPtr — пайплайн жив до их конца
	Super::BeginDestroy();
}

// ---- Заполнение данных ------------------------------------------------------

void AVoxelStructure::FillTestPattern()
{
	Chunks.Empty();

	const int32 N = NgxVoxel::ChunkSize;
	const FIntVector Dims(
		FMath::Max(ChunkDims.X, 1),
		FMath::Max(ChunkDims.Y, 1),
		FMath::Max(ChunkDims.Z, 1));

	// Глобальный воксельный объём всей структуры и вписанный в него сплошной шар.
	// Шар намеренно пересекает границы чанков → проверяем, что внутренних «стен» нет.
	const FVector Center(
		Dims.X * N * 0.5f - 0.5f,
		Dims.Y * N * 0.5f - 0.5f,
		Dims.Z * N * 0.5f - 0.5f);
	const float Radius = FMath::Min3(Dims.X, Dims.Y, Dims.Z) * N * 0.45f;

	for (int32 CZ = 0; CZ < Dims.Z; ++CZ)
	for (int32 CY = 0; CY < Dims.Y; ++CY)
	for (int32 CX = 0; CX < Dims.X; ++CX)
	{
		// Ключ-чанк создаётся уже занулённым и нужного размера (ctor FVoxelChunk).
		FVoxelChunk& Ch = Chunks.Add(FIntVector(CX, CY, CZ));

		for (int32 Z = 0; Z < N; ++Z)
		for (int32 Y = 0; Y < N; ++Y)
		for (int32 X = 0; X < N; ++X)
		{
			const FVector G(
				(float)(CX * N + X),
				(float)(CY * N + Y),
				(float)(CZ * N + Z));
			if ((G - Center).Size() <= Radius)
			{
				Ch.Set(X, Y, Z, TestMaterial);
			}
		}
	}
}

// ---- Публичные действия -----------------------------------------------------

void AVoxelStructure::Rebuild()
{
	FillTestPattern();

	if (!bAsyncMeshing)
	{
		RebuildSync();
		return;
	}

	// Полный async-ребилд: новый epoch инвалидирует задачи в полёте, чистим секции и очереди.
	++BuildEpoch;
	Mesh->ClearAllMeshSections();
	SectionOf.Reset();
	NextSection = 0;
	DirtyChunks.Reset();

	if (!Pipeline.IsValid())
	{
		Pipeline = MakeShared<FVoxelMeshPipeline, ESPMode::ThreadSafe>();
	}
	else
	{
		FChunkMeshResult Stale;
		while (Pipeline->Completed.Dequeue(Stale)) {}   // выбросить результаты прошлого epoch
	}

	MarkAllDirty();
	EnsureTicker();
}

void AVoxelStructure::RemeshCenterChunk()
{
	if (Chunks.Num() == 0)
	{
		return;
	}

	const FIntVector Dims(
		FMath::Max(ChunkDims.X, 1),
		FMath::Max(ChunkDims.Y, 1),
		FMath::Max(ChunkDims.Z, 1));
	const FIntVector Center(Dims.X / 2, Dims.Y / 2, Dims.Z / 2);

	if (!Chunks.Contains(Center))
	{
		return;
	}

	// Точечный ремеш: не трогаем epoch (поверх текущего меша), помечаем один чанк.
	DirtyChunks.Add(Center);
	if (!Pipeline.IsValid())
	{
		Pipeline = MakeShared<FVoxelMeshPipeline, ESPMode::ThreadSafe>();
	}
	EnsureTicker();
}

// ---- Разрушение (шаг 4) -----------------------------------------------------

void AVoxelStructure::ApplyTestDamage()
{
	const FVector World = GetActorTransform().TransformPosition(TestDamageLocalCm);
	ApplyDamageSphere(World, TestDamageRadiusCm, TestDamageMaterial);
}

int32 AVoxelStructure::ApplyDamageSphere(const FVector& WorldCenter, float RadiusCm, uint8 NewMaterial)
{
	if (Chunks.Num() == 0 || RadiusCm <= 0.f)
	{
		return 0;
	}

	const int32 N = NgxVoxel::ChunkSize;
	const float S = NgxVoxel::VoxelSizeCm;

	// Мир → локаль актёра → воксельные единицы.
	const FVector LocalCm = GetActorTransform().InverseTransformPosition(WorldCenter);
	const FVector CenterVx = LocalCm / S;
	const float RadiusVx = RadiusCm / S;
	const float RadiusVxSq = RadiusVx * RadiusVx;

	const FIntVector Dims(
		FMath::Max(ChunkDims.X, 1),
		FMath::Max(ChunkDims.Y, 1),
		FMath::Max(ChunkDims.Z, 1));
	const FIntVector MaxVox(Dims.X * N, Dims.Y * N, Dims.Z * N);

	// Bbox сферы в глобальных воксельных координатах, клампим к объёму структуры.
	const int32 MinX = FMath::Max(0, FMath::FloorToInt((float)CenterVx.X - RadiusVx));
	const int32 MinY = FMath::Max(0, FMath::FloorToInt((float)CenterVx.Y - RadiusVx));
	const int32 MinZ = FMath::Max(0, FMath::FloorToInt((float)CenterVx.Z - RadiusVx));
	const int32 MaxX = FMath::Min(MaxVox.X - 1, FMath::CeilToInt((float)CenterVx.X + RadiusVx));
	const int32 MaxY = FMath::Min(MaxVox.Y - 1, FMath::CeilToInt((float)CenterVx.Y + RadiusVx));
	const int32 MaxZ = FMath::Min(MaxVox.Z - 1, FMath::CeilToInt((float)CenterVx.Z + RadiusVx));

	int32 Changed = 0;

	for (int32 GZ = MinZ; GZ <= MaxZ; ++GZ)
	for (int32 GY = MinY; GY <= MaxY; ++GY)
	for (int32 GX = MinX; GX <= MaxX; ++GX)
	{
		// Сравниваем центр вокселя со сферой.
		const FVector VoxCenter(GX + 0.5f, GY + 0.5f, GZ + 0.5f);
		if (FVector::DistSquared(VoxCenter, CenterVx) > RadiusVxSq)
		{
			continue;
		}

		const FIntVector CC(GX / N, GY / N, GZ / N);
		FVoxelChunk* Ch = Chunks.Find(CC);
		if (!Ch)
		{
			continue;
		}

		const int32 LX = GX - CC.X * N;
		const int32 LY = GY - CC.Y * N;
		const int32 LZ = GZ - CC.Z * N;

		if (Ch->Get(LX, LY, LZ) == NewMaterial)
		{
			continue;   // без изменений
		}

		Ch->Set(LX, LY, LZ, NewMaterial);
		++Changed;

		// Этот чанк + соседи по осям, где воксель лежит на границе чанка (их граничные
		// грани зависят от изменённого вокселя). Несуществующих соседей не трогаем.
		DirtyChunks.Add(CC);
		if (LX == 0     && Chunks.Contains(CC + FIntVector(-1, 0, 0))) DirtyChunks.Add(CC + FIntVector(-1, 0, 0));
		if (LX == N - 1 && Chunks.Contains(CC + FIntVector( 1, 0, 0))) DirtyChunks.Add(CC + FIntVector( 1, 0, 0));
		if (LY == 0     && Chunks.Contains(CC + FIntVector(0, -1, 0))) DirtyChunks.Add(CC + FIntVector(0, -1, 0));
		if (LY == N - 1 && Chunks.Contains(CC + FIntVector(0,  1, 0))) DirtyChunks.Add(CC + FIntVector(0,  1, 0));
		if (LZ == 0     && Chunks.Contains(CC + FIntVector(0, 0, -1))) DirtyChunks.Add(CC + FIntVector(0, 0, -1));
		if (LZ == N - 1 && Chunks.Contains(CC + FIntVector(0, 0,  1))) DirtyChunks.Add(CC + FIntVector(0, 0,  1));
	}

	if (Changed > 0)
	{
		if (!Pipeline.IsValid())
		{
			Pipeline = MakeShared<FVoxelMeshPipeline, ESPMode::ThreadSafe>();
		}
		NotifyStructuralShock(WorldCenter, RadiusCm);
		EnsureTicker();

		UE_LOG(LogNgxVoxel, Log,
			TEXT("VoxelStructure '%s': damage r=%.0fcm mat=%u → %d voxels changed, %d chunks dirty"),
			*GetName(), RadiusCm, NewMaterial, Changed, DirtyChunks.Num());
	}

	return Changed;
}

void AVoxelStructure::NotifyStructuralShock(const FVector& WorldCenter, float RadiusCm) const
{
	// Шаг 6: здесь запустим FStructuralSolver (flood-fill от якорей) на затронутом регионе,
	// чтобы оторванные компоненты падали (→ AVoxelDebris). Пока — только трасса.
	UE_LOG(LogNgxVoxel, Verbose, TEXT("Structural shock @ %s r=%.0f (solver — шаг 6)"),
		*WorldCenter.ToString(), RadiusCm);
}

// ---- Async-пайплайн ---------------------------------------------------------

void AVoxelStructure::MarkAllDirty()
{
	for (const TPair<FIntVector, FVoxelChunk>& Pair : Chunks)
	{
		DirtyChunks.Add(Pair.Key);
	}
}

void AVoxelStructure::EnsureTicker()
{
	if (TickerHandle.IsValid())
	{
		return;
	}

	// Старт новой активной сессии ремеша — обнуляем сводку.
	SessionVerts = 0;
	SessionTris = 0;
	SessionSections = 0;

	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &AVoxelStructure::TickPipeline));
}

void AVoxelStructure::RemoveTicker()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
}

bool AVoxelStructure::TickPipeline(float /*Dt*/)
{
	// 1) Применить готовое (бюджет на кадр).
	if (Pipeline.IsValid())
	{
		int32 Applied = 0;
		FChunkMeshResult Result;
		while (Applied < ApplyBudgetPerFrame && Pipeline->Completed.Dequeue(Result))
		{
			ApplyResult(Result);
			++Applied;
		}
	}

	// 2) Диспетчеризовать грязные чанки (бюджет на кадр).
	int32 Dispatched = 0;
	for (auto It = DirtyChunks.CreateIterator(); It && Dispatched < DispatchBudgetPerFrame; ++It)
	{
		const FIntVector Coord = *It;
		if (InFlight.Contains(Coord))
		{
			continue;   // уже в работе — оставляем грязным, переотправим после завершения
		}
		It.RemoveCurrent();
		DispatchChunk(Coord);
		++Dispatched;
	}

	// 3) Делать больше нечего → снять тикер и подвести итог.
	const bool bBusy = DirtyChunks.Num() > 0
		|| InFlight.Num() > 0
		|| (Pipeline.IsValid() && !Pipeline->Completed.IsEmpty());

	if (!bBusy)
	{
		UE_LOG(LogNgxVoxel, Log,
			TEXT("VoxelStructure '%s': async %s build → %d sections, %d verts, %d tris (epoch %llu)"),
			*GetName(),
			bUseGreedy ? TEXT("greedy") : TEXT("naive"),
			SessionSections, SessionVerts, SessionTris, BuildEpoch);

		TickerHandle.Reset();
		return false;   // авто-снятие тикера
	}

	return true;
}

void AVoxelStructure::DispatchChunk(const FIntVector& Coord)
{
	const FVoxelChunk* Center = Chunks.Find(Coord);
	if (!Center)
	{
		return;
	}

	// Стабильный индекс секции на чанк.
	int32 SectionIndex;
	if (const int32* Found = SectionOf.Find(Coord))
	{
		SectionIndex = *Found;
	}
	else
	{
		SectionIndex = NextSection++;
		SectionOf.Add(Coord, SectionIndex);
	}

	// Снапшот центра + существующих соседей (копии по значению).
	FChunkSnapshot Snap;
	Snap.Center = *Center;
	for (int32 i = 0; i < 6; ++i)
	{
		if (const FVoxelChunk* Nb = Chunks.Find(Coord + NeighborOffsets[i]))
		{
			Snap.Neighbor[i] = *Nb;
			Snap.bHas[i] = true;
		}
	}

	const FVector Origin = ChunkOrigin(Coord);
	const bool bGreedy = bUseGreedy;
	const uint64 Epoch = BuildEpoch;
	TSharedPtr<FVoxelMeshPipeline, ESPMode::ThreadSafe> Pipe = Pipeline;

	InFlight.Add(Coord);

	Async(EAsyncExecution::ThreadPool,
		[Snap = MoveTemp(Snap), Origin, bGreedy, Epoch, Coord, SectionIndex, Pipe]()
		{
			FChunkMeshResult Result;
			Result.Coord = Coord;
			Result.SectionIndex = SectionIndex;
			Result.Epoch = Epoch;

			const FChunkNeighbors Nbr = Snap.MakeNeighbors();
			if (bGreedy)
			{
				FVoxelMesher::GenerateGreedy(Snap.Center, Nbr, Origin, Result.Data);
			}
			else
			{
				FVoxelMesher::GenerateNaive(Snap.Center, Nbr, Origin, Result.Data);
			}

			if (Pipe.IsValid())
			{
				Pipe->Completed.Enqueue(MoveTemp(Result));
			}
		});
}

void AVoxelStructure::ApplyResult(FChunkMeshResult& Result)
{
	InFlight.Remove(Result.Coord);

	if (Result.Epoch != BuildEpoch)
	{
		return;   // устарел (был полный Rebuild) — выбросить
	}
	if (DirtyChunks.Contains(Result.Coord))
	{
		return;   // чанк перегрязнён — применит более свежая задача
	}

	if (Result.Data.IsEmpty())
	{
		Mesh->ClearMeshSection(Result.SectionIndex);
	}
	else
	{
		Mesh->CreateMeshSection(
			Result.SectionIndex,
			Result.Data.Vertices,
			Result.Data.Triangles,
			Result.Data.Normals,
			Result.Data.UV0,
			Result.Data.VertexColors,
			TArray<FProcMeshTangent>(),
			/*bCreateCollision=*/ true);

		SessionVerts += Result.Data.Vertices.Num();
		SessionTris += Result.Data.Triangles.Num() / 3;
		++SessionSections;
	}
}

// ---- Синхронный путь (bAsyncMeshing=false) ----------------------------------

void AVoxelStructure::RebuildSync()
{
	Mesh->ClearAllMeshSections();
	SectionOf.Reset();
	NextSection = 0;

	int32 TotalVerts = 0;
	int32 TotalTris = 0;

	for (const TPair<FIntVector, FVoxelChunk>& Pair : Chunks)
	{
		const FChunkNeighbors Nbr = GatherNeighbors(Pair.Key);
		const FVector Origin = ChunkOrigin(Pair.Key);

		FVoxelMeshData Data;
		if (bUseGreedy)
		{
			FVoxelMesher::GenerateGreedy(Pair.Value, Nbr, Origin, Data);
		}
		else
		{
			FVoxelMesher::GenerateNaive(Pair.Value, Nbr, Origin, Data);
		}

		if (!Data.IsEmpty())
		{
			// Стабильный индекс секции на чанк — чтобы последующий dirty-ремеш (разрушение)
			// обновлял ту же секцию, а не плодил новые.
			const int32 SectionIdx = NextSection++;
			SectionOf.Add(Pair.Key, SectionIdx);

			Mesh->CreateMeshSection(
				SectionIdx,
				Data.Vertices,
				Data.Triangles,
				Data.Normals,
				Data.UV0,
				Data.VertexColors,
				TArray<FProcMeshTangent>(),
				/*bCreateCollision=*/ true);

			TotalVerts += Data.Vertices.Num();
			TotalTris += Data.Triangles.Num() / 3;
		}
	}

	UE_LOG(LogNgxVoxel, Log, TEXT("VoxelStructure '%s': SYNC %s mesher → %d chunks, %d sections, %d verts, %d tris"),
		*GetName(),
		bUseGreedy ? TEXT("greedy") : TEXT("naive"),
		Chunks.Num(),
		NextSection,
		TotalVerts,
		TotalTris);
}

// ---- Хелперы ----------------------------------------------------------------

FChunkNeighbors AVoxelStructure::GatherNeighbors(const FIntVector& Coord) const
{
	FChunkNeighbors N;
	N.Neg[0] = Chunks.Find(Coord + FIntVector(-1, 0, 0));
	N.Pos[0] = Chunks.Find(Coord + FIntVector( 1, 0, 0));
	N.Neg[1] = Chunks.Find(Coord + FIntVector(0, -1, 0));
	N.Pos[1] = Chunks.Find(Coord + FIntVector(0,  1, 0));
	N.Neg[2] = Chunks.Find(Coord + FIntVector(0, 0, -1));
	N.Pos[2] = Chunks.Find(Coord + FIntVector(0, 0,  1));
	return N;
}

FVector AVoxelStructure::ChunkOrigin(const FIntVector& Coord) const
{
	const float ChunkCm = (float)NgxVoxel::ChunkSize * NgxVoxel::VoxelSizeCm;
	return FVector(Coord.X * ChunkCm, Coord.Y * ChunkCm, Coord.Z * ChunkCm);
}
