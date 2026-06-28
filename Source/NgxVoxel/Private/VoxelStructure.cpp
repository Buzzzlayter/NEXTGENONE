// Copyright. NgxVoxel — AVoxelStructure implementation (VS0).

#include "VoxelStructure.h"
#include "VoxelMesher.h"
#include "VoxelTypes.h"
#include "VoxelChunk.h"
#include "StructuralSolver.h"
#include "VoxelDebris.h"
#include "NgxVoxel.h"
#include "VoxelProceduralMeshComponent.h"
#include "Async/Async.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

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

	Mesh = CreateDefaultSubobject<UVoxelProceduralMeshComponent>(TEXT("VoxelMesh"));
	SetRootComponent(Mesh);
	Mesh->bUseAsyncCooking = true;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> VoxelMaterial(
		TEXT("/Game/Voxel/Materials/M_VoxelVertexBiome.M_VoxelVertexBiome"));
	if (VoxelMaterial.Succeeded())
	{
		Mesh->SharedMaterial = VoxelMaterial.Object;
	}
	// Разрушаемый воксель-меш не должен гонять навмеш (дорого + транзиентные пустые баунды
	// при async-ребилде → спам LogNavigation). Динамическую навигацию решим отдельно, если понадобится.
	Mesh->SetCanEverAffectNavigation(false);
	// Явно блокируем query-каналы → трейс инструмента игрока (Visibility, complex) попадает по структуре.
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionObjectType(ECC_WorldStatic);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);
}

void AVoxelStructure::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Rebuild();
}

void AVoxelStructure::BeginPlay()
{
	Super::BeginPlay();
	// Chunks — не UPROPERTY и в рантайм-инстанс (PIE/дубликат) не переносятся, а OnConstruction
	// в игре не перевызывается. Пересобираем данные+меш, иначе разрушению нечего менять.
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
	const int32 GX = Dims.X * N, GY = Dims.Y * N, GZ = Dims.Z * N;

	// Кусок «поверхности»: заполняем всё ниже бугристой высоты (heightfield) → читается как глыба
	// грунта/скалы. Стоит на земле (нижние слои — якоря), сверху неровный рельеф с холмом по центру —
	// масса, которой эффектно обрушаться глыбами при подрыве/карве.
	const float BaseH  = GZ * 0.42f;                    // средняя высота поверхности
	const float Amp    = GZ * 0.16f;                    // амплитуда бугров
	const float MoundH = GZ * 0.40f;                    // высота центрального холма
	const FVector2D Center(GX * 0.5f, GY * 0.5f);
	const float MoundR = FMath::Min(GX, GY) * 0.32f;

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
			const int32 gx = CX * N + X;
			const int32 gy = CY * N + Y;
			const int32 gz = CZ * N + Z;

			// Бугристая высота поверхности в этой точке (сумма синусов) + центральный купол.
			float H = BaseH
				+ Amp * FMath::Sin(gx * 0.09f) * FMath::Cos(gy * 0.07f)
				+ Amp * 0.5f * FMath::Sin((gx + gy) * 0.05f);

			const float Dist2D = FVector2D((float)gx - Center.X, (float)gy - Center.Y).Size();
			if (Dist2D < MoundR)
			{
				const float T = 1.f - (Dist2D / MoundR);
				H += MoundH * T * T;   // гладкий купол к центру
			}

			const float TreeWave = FMath::Sin(gx * 0.13f + 1.7f) + FMath::Cos(gy * 0.11f - 0.4f);
			const bool bTreePocket =
				((gx % 31) >= 13 && (gx % 31) <= 17) &&
				((gy % 29) >= 12 && (gy % 29) <= 16) &&
				TreeWave > 0.35f;
			const bool bWoodColumn = bTreePocket && (float)gz < H + 12.f && (float)gz > H - 9.f;

			if ((float)gz < H || bWoodColumn)
			{
				uint8 Mat = TestMaterial; // 1 = earth
				if (bWoodColumn)                   Mat = 2;
				else if ((float)gz > H - 2.5f)     Mat = 3;
				else if ((float)gz < GZ * 0.18f)   Mat = 4;
				Ch.Set(X, Y, Z, Mat);
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

		// Этот чанк + соседи по границам (их граничные грани зависят от изменённого вокселя).
		MarkChunkDirtyAround(CC, LX, LY, LZ);
	}

	if (Changed > 0)
	{
		if (!Pipeline.IsValid())
		{
			Pipeline = MakeShared<FVoxelMeshPipeline, ESPMode::ThreadSafe>();
		}
		// Удаление вокселей могло что-то оторвать → запланировать проверку связности
		// (наращивание только добавляет связность, оторвать не может).
		if (NewMaterial == 0)
		{
			bIntegrityDirty = true;
			// Запоминаем центр удара → радиальный импульс кускам в отложенной проверке.
			bPendingShock = true;
			PendingShockOrigin = WorldCenter;
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
		// Меш устаканился — момент для проверки связности (дебаунс после серии карвов).
		if (bIntegrityDirty)
		{
			bIntegrityDirty = false;
			// Если был удар игрока — толкаем куски радиально от него; иначе просто падают.
			if (bPendingShock)
			{
				bPendingShock = false;
				RunIntegrityCheck(EDetachLaunch::Radial, PendingShockOrigin, DebrisLaunchSpeedCmS);
			}
			else
			{
				RunIntegrityCheck();   // может удалить оторванное → новые dirty-чанки
			}
			if (DirtyChunks.Num() > 0 || InFlight.Num() > 0)
			{
				return true;       // продолжаем тикать, доремешим
			}
		}

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
			/*bCreateCollision=*/ ShouldCookCollision(Result.Coord));

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
				/*bCreateCollision=*/ ShouldCookCollision(Pair.Key));

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

// ---- Коллизия: скоупинг на активные чанки (шаг 5b) --------------------------

void AVoxelStructure::SetCollisionFocus(const FVector& WorldCenter)
{
	const bool bFirst = !bHasFocus;
	CollisionFocus = WorldCenter;
	bHasFocus = true;
	UpdateActiveChunks(bFirst);
}

bool AVoxelStructure::ShouldCookCollision(const FIntVector& Coord) const
{
	if (!bScopeCollisionToFocus)
	{
		return true;                      // скоупинг выключен → кукаем у всех
	}
	if (!bHasFocus)
	{
		return true;                      // фокус ещё не задан → кукаем всё (коллизия сразу доступна)
	}
	return ActiveChunks.Contains(Coord);  // только чанки в радиусе фокуса
}

void AVoxelStructure::UpdateActiveChunks(bool bForceAllDirty)
{
	if (!bScopeCollisionToFocus || !bHasFocus)
	{
		return;
	}

	const float ChunkCm = (float)NgxVoxel::ChunkSize * NgxVoxel::VoxelSizeCm;
	const float RadiusSq = FMath::Square(CollisionActiveRadiusCm);
	const FTransform& Xf = GetActorTransform();

	bool bAnyDirty = false;
	for (const TPair<FIntVector, FVoxelChunk>& Pair : Chunks)
	{
		const FIntVector& CC = Pair.Key;
		const FVector LocalCenter(
			(CC.X + 0.5f) * ChunkCm,
			(CC.Y + 0.5f) * ChunkCm,
			(CC.Z + 0.5f) * ChunkCm);
		const FVector WorldCenter = Xf.TransformPosition(LocalCenter);

		const bool bNowActive = FVector::DistSquared(WorldCenter, CollisionFocus) <= RadiusSq;
		const bool bWasActive = ActiveChunks.Contains(CC);

		if (bNowActive)
		{
			ActiveChunks.Add(CC);
		}
		else
		{
			ActiveChunks.Remove(CC);
		}

		// Дёрти при смене активности; при первом пересчёте — все (флаг коллизии у всех поменялся).
		if (bForceAllDirty || (bNowActive != bWasActive))
		{
			DirtyChunks.Add(CC);
			bAnyDirty = true;
		}
	}

	if (bAnyDirty)
	{
		if (!Pipeline.IsValid())
		{
			Pipeline = MakeShared<FVoxelMeshPipeline, ESPMode::ThreadSafe>();
		}
		EnsureTicker();
	}
}

// ---- Структурная целостность (шаг 6) ----------------------------------------

void AVoxelStructure::MarkChunkDirtyAround(const FIntVector& ChunkCoord, int32 LX, int32 LY, int32 LZ)
{
	const int32 N = NgxVoxel::ChunkSize;
	DirtyChunks.Add(ChunkCoord);
	if (LX == 0     && Chunks.Contains(ChunkCoord + FIntVector(-1, 0, 0))) DirtyChunks.Add(ChunkCoord + FIntVector(-1, 0, 0));
	if (LX == N - 1 && Chunks.Contains(ChunkCoord + FIntVector( 1, 0, 0))) DirtyChunks.Add(ChunkCoord + FIntVector( 1, 0, 0));
	if (LY == 0     && Chunks.Contains(ChunkCoord + FIntVector(0, -1, 0))) DirtyChunks.Add(ChunkCoord + FIntVector(0, -1, 0));
	if (LY == N - 1 && Chunks.Contains(ChunkCoord + FIntVector(0,  1, 0))) DirtyChunks.Add(ChunkCoord + FIntVector(0,  1, 0));
	if (LZ == 0     && Chunks.Contains(ChunkCoord + FIntVector(0, 0, -1))) DirtyChunks.Add(ChunkCoord + FIntVector(0, 0, -1));
	if (LZ == N - 1 && Chunks.Contains(ChunkCoord + FIntVector(0, 0,  1))) DirtyChunks.Add(ChunkCoord + FIntVector(0, 0,  1));
}

void AVoxelStructure::RunIntegrityCheck(EDetachLaunch Launch, const FVector& LaunchOrigin, float LaunchSpeed)
{
	if (!bEnableStructuralIntegrity || Chunks.Num() == 0)
	{
		return;
	}

	const int32 N = NgxVoxel::ChunkSize;
	const FIntVector CDims(
		FMath::Max(ChunkDims.X, 1),
		FMath::Max(ChunkDims.Y, 1),
		FMath::Max(ChunkDims.Z, 1));
	const FIntVector VDims(CDims.X * N, CDims.Y * N, CDims.Z * N);

	auto IsSolid = [this, N](int32 X, int32 Y, int32 Z) -> bool
	{
		const FIntVector CC(X / N, Y / N, Z / N);
		const FVoxelChunk* Ch = Chunks.Find(CC);
		return Ch && Ch->Get(X - CC.X * N, Y - CC.Y * N, Z - CC.Z * N) != 0;
	};
	auto IsAnchor = [this](int32 /*X*/, int32 /*Y*/, int32 Z) -> bool
	{
		return Z <= AnchorMaxGlobalZ;   // солвер вызывает IsAnchor только для solid-вокселей
	};

	// Реальная опора (gravity-aware): нависания без опоры снизу рушатся, а не «висят».
	TArray<TArray<FIntVector>> Detached;
	FStructuralSolver::FindUnsupportedComponents(VDims, MaxCantileverVoxels, IsSolid, IsAnchor, Detached);

	if (Detached.Num() == 0)
	{
		return;
	}

	// Материал вокселя (для меша дебриса; читаем ДО удаления — компоненты не пересекаются).
	auto MaterialAt = [this, N](const FIntVector& G) -> uint8
	{
		const FIntVector CC(G.X / N, G.Y / N, G.Z / N);
		const FVoxelChunk* Ch = Chunks.Find(CC);
		return Ch ? Ch->Get(G.X - CC.X * N, G.Y - CC.Y * N, G.Z - CC.Z * N) : 0;
	};

	const float S = NgxVoxel::VoxelSizeCm;
	UWorld* World = GetWorld();
	int32 RemovedVox = 0;
	int32 SpawnedDebris = 0;

	const int32 BSize = FMath::Max(BoulderSizeVoxels, 1);

	for (const TArray<FIntVector>& Comp : Detached)
	{
		// Дробим оторванную массу на «глыбы» (~BoulderSizeVoxels): обвал идёт кусками, а не одним
		// монолитом и не пылью. Бьём по грубой сетке — каждая занятая ячейка = одна глыба.
		if (bSpawnDebris && World)
		{
			TMap<FIntVector, TArray<FIntVector>> Cells;
			for (const FIntVector& G : Comp)
			{
				const FIntVector Key(G.X / BSize, G.Y / BSize, G.Z / BSize);
				Cells.FindOrAdd(Key).Add(G);
			}

			for (const TPair<FIntVector, TArray<FIntVector>>& Cell : Cells)
			{
				const TArray<FIntVector>& Vox = Cell.Value;
				if (Vox.Num() < MinDebrisVoxels)
				{
					continue;   // совсем мелочь — без дебриса (удалится ниже)
				}

				TSet<FIntVector> Set;
				Set.Append(Vox);

				FIntVector Origin = Vox[0];
				FVector CentroidVx = FVector::ZeroVector;
				for (const FIntVector& G : Vox)
				{
					Origin.X = FMath::Min(Origin.X, G.X);
					Origin.Y = FMath::Min(Origin.Y, G.Y);
					Origin.Z = FMath::Min(Origin.Z, G.Z);
					CentroidVx += FVector(G.X + 0.5f, G.Y + 0.5f, G.Z + 0.5f);
				}
				CentroidVx /= (float)Vox.Num();

				FVoxelMeshData DebrisMesh;
				FVoxelMesher::GenerateFromVoxelSet(Set, MaterialAt, Origin, DebrisMesh);
				if (DebrisMesh.IsEmpty())
				{
					continue;
				}

				FTransform Xf = GetActorTransform();
				Xf.SetLocation(GetActorTransform().TransformPosition(
					FVector(Origin.X, Origin.Y, Origin.Z) * S));

				// Стартовый импульс глыбы («ось разрушения») по её центроиду.
				FVector LinVel = FVector::ZeroVector;
				FVector AngVel = FVector::ZeroVector;
				if (Launch != EDetachLaunch::Gravity && LaunchSpeed > 0.f)
				{
					const FVector CentroidWorld = GetActorTransform().TransformPosition(CentroidVx * S);
					if (Launch == EDetachLaunch::Radial)
					{
						const FVector Dir = (CentroidWorld - LaunchOrigin).GetSafeNormal();
						LinVel = Dir * LaunchSpeed + FVector(0.f, 0.f, LaunchSpeed * 0.25f); // чуть подбрасываем
					}
					else // Cascade: в основном вниз + лёгкий разлёт в стороны
					{
						LinVel = FVector(
							FMath::FRandRange(-0.3f, 0.3f),
							FMath::FRandRange(-0.3f, 0.3f),
							-0.1f) * LaunchSpeed;
					}
					AngVel = FMath::VRand() * DebrisSpinRadS;
				}

				if (AVoxelDebris* Debris = World->SpawnActor<AVoxelDebris>(
						AVoxelDebris::StaticClass(), Xf))
				{
					Debris->Init(DebrisMesh, DebrisLifeSeconds, LinVel, AngVel);
					++SpawnedDebris;
				}
			}
		}

		// Удаляем воксели всей оторванной массы из структуры.
		for (const FIntVector& G : Comp)
		{
			const FIntVector CC(G.X / N, G.Y / N, G.Z / N);
			FVoxelChunk* Ch = Chunks.Find(CC);
			if (!Ch)
			{
				continue;
			}
			const int32 LX = G.X - CC.X * N;
			const int32 LY = G.Y - CC.Y * N;
			const int32 LZ = G.Z - CC.Z * N;
			Ch->Set(LX, LY, LZ, 0);
			++RemovedVox;
			MarkChunkDirtyAround(CC, LX, LY, LZ);
		}
	}

	UE_LOG(LogNgxVoxel, Log, TEXT("VoxelStructure '%s': integrity → %d detached comp, %d debris, %d voxels removed"),
		*GetName(), Detached.Num(), SpawnedDebris, RemovedVox);

	if (DirtyChunks.Num() > 0)
	{
		if (!Pipeline.IsValid())
		{
			Pipeline = MakeShared<FVoxelMeshPipeline, ESPMode::ThreadSafe>();
		}
		EnsureTicker();
	}
}

// ---- Тест-обвал (шаг 7) -----------------------------------------------------
// Одношаговый триггер (клавиша C): подрываем опору — срезаем тонкую полосу прямо над якорным
// слоем по всей площади → всё, что выше, теряет опору и обрушается ПО-НАСТОЯЩЕМУ через модель
// опоры (RunIntegrityCheck) с дроблением на глыбы. Никаких скриптовых «слоёв».

void AVoxelStructure::StartCascade()
{
	const int32 N = NgxVoxel::ChunkSize;
	const FIntVector VDims(
		FMath::Max(ChunkDims.X, 1) * N,
		FMath::Max(ChunkDims.Y, 1) * N,
		FMath::Max(ChunkDims.Z, 1) * N);

	// Полоса подрыва: чуть выше якорного слоя, толщиной с «глыбу» — рвёт вертикальные опоры.
	const int32 Z0 = FMath::Clamp(AnchorMaxGlobalZ + 1, 0, VDims.Z - 1);
	const int32 Z1 = FMath::Clamp(Z0 + FMath::Max(BoulderSizeVoxels, 1) - 1, 0, VDims.Z - 1);

	for (int32 Z = Z0; Z <= Z1; ++Z)
	for (int32 Y = 0; Y < VDims.Y; ++Y)
	for (int32 X = 0; X < VDims.X; ++X)
	{
		const FIntVector CC(X / N, Y / N, Z / N);
		FVoxelChunk* Ch = Chunks.Find(CC);
		if (!Ch)
		{
			continue;
		}
		const int32 LX = X - CC.X * N;
		const int32 LY = Y - CC.Y * N;
		const int32 LZ = Z - CC.Z * N;
		if (Ch->Get(LX, LY, LZ) != 0)
		{
			Ch->Set(LX, LY, LZ, 0);
			MarkChunkDirtyAround(CC, LX, LY, LZ);
		}
	}

	if (!Pipeline.IsValid())
	{
		Pipeline = MakeShared<FVoxelMeshPipeline, ESPMode::ThreadSafe>();
	}

	// Подрыв опоры → масса выше обрушается глыбами (вниз + спин).
	RunIntegrityCheck(EDetachLaunch::Cascade, FVector::ZeroVector, CascadeLaunchSpeedCmS);
	EnsureTicker();

	UE_LOG(LogNgxVoxel, Log, TEXT("VoxelStructure '%s': collapse triggered (Z %d..%d)"), *GetName(), Z0, Z1);
}

void AVoxelStructure::StopCascade()
{
	bCascadeRunning = false;
}
