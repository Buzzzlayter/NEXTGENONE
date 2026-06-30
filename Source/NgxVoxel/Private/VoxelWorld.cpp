// Copyright. NgxVoxel - streaming voxel world actor.

#include "VoxelWorld.h"
#include "NgxVoxel.h"
#include "VoxelProceduralMeshComponent.h"
#include "VoxelRuntimeRegistry.h"
#include "Async/Async.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	struct FWorldChunkSnapshot
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

AVoxelWorld::AVoxelWorld()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FoundMaterial(
		TEXT("/Game/Voxel/Materials/M_VoxelVertexBiome.M_VoxelVertexBiome"));
	if (FoundMaterial.Succeeded())
	{
		VoxelMaterial = FoundMaterial.Object;
	}
}

void AVoxelWorld::BeginPlay()
{
	Super::BeginPlay();
	FVoxelRuntimeRegistry::Register(this);
	RebuildWorld();
}

void AVoxelWorld::EndPlay(const EEndPlayReason::Type Reason)
{
	FVoxelRuntimeRegistry::Unregister(this);
	for (TPair<FIntVector, FVoxelWorldChunk>& Pair : Chunks)
	{
		if (Pair.Value.Mesh)
		{
			Pair.Value.Mesh->ClearAllMeshSections();
			Pair.Value.Mesh->DestroyComponent();
			Pair.Value.Mesh = nullptr;
		}
	}

	Chunks.Reset();
	DirtyChunks.Reset();
	InFlight.Reset();
	Pipeline.Reset();

	Super::EndPlay(Reason);
}

void AVoxelWorld::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	TimeUntilStreamingUpdate -= DeltaSeconds;
	if (TimeUntilStreamingUpdate <= 0.f)
	{
		TimeUntilStreamingUpdate = StreamingUpdateInterval;
		UpdateStreamingWindow();
	}

	if (Pipeline.IsValid())
	{
		int32 Applied = 0;
		FVoxelWorldMeshResult Result;
		while (Applied < ApplyBudgetPerFrame && Pipeline->Completed.Dequeue(Result))
		{
			ApplyResult(Result);
			++Applied;
		}
	}

	int32 Dispatched = 0;
	for (auto It = DirtyChunks.CreateIterator(); It && Dispatched < DispatchBudgetPerFrame; ++It)
	{
		const FIntVector Coord = *It;
		if (InFlight.Contains(Coord))
		{
			continue;
		}

		It.RemoveCurrent();
		DispatchChunk(Coord);
		++Dispatched;
	}
}

void AVoxelWorld::RebuildWorld()
{
	for (TPair<FIntVector, FVoxelWorldChunk>& Pair : Chunks)
	{
		if (Pair.Value.Mesh)
		{
			Pair.Value.Mesh->ClearAllMeshSections();
			Pair.Value.Mesh->DestroyComponent();
			Pair.Value.Mesh = nullptr;
		}
	}

	Chunks.Reset();
	DirtyChunks.Reset();
	InFlight.Reset();
	Pipeline = MakeShared<FVoxelWorldMeshPipeline, ESPMode::ThreadSafe>();
	TimeUntilStreamingUpdate = 0.f;
	UpdateStreamingWindow();
}

void AVoxelWorld::UpdateStreamingWindow()
{
	const AActor* Focus = ResolveFocusActor();
	if (!Focus)
	{
		return;
	}

	const FIntVector Center = WorldToChunkCoord(Focus->GetActorLocation());
	TSet<FIntVector> Desired;
	TArray<FIntVector> LoadOrder;

	const float ChunkCm = (float)NgxVoxel::ChunkSize * NgxVoxel::VoxelSizeCm;
	const int32 RadiusByCm = FMath::CeilToInt(FMath::Max(ViewDistanceCm, 0.f) / ChunkCm);
	const int32 Radius = FMath::Max(FMath::Max(ViewDistanceChunks, 0), RadiusByCm);
	const int32 Height = FMath::Max(ChunkHeightChunks, 1);
	for (int32 Y = Center.Y - Radius; Y <= Center.Y + Radius; ++Y)
	for (int32 X = Center.X - Radius; X <= Center.X + Radius; ++X)
	for (int32 Z = 0; Z < Height; ++Z)
	{
		const FIntVector Coord(X, Y, Z);
		Desired.Add(Coord);
		LoadOrder.Add(Coord);
	}
	LoadOrder.Sort([Center](const FIntVector& A, const FIntVector& B)
	{
		const int32 DistA = FMath::Abs(A.X - Center.X) + FMath::Abs(A.Y - Center.Y) + A.Z;
		const int32 DistB = FMath::Abs(B.X - Center.X) + FMath::Abs(B.Y - Center.Y) + B.Z;
		return DistA < DistB;
	});

	TArray<FIntVector> ToUnload;
	for (const TPair<FIntVector, FVoxelWorldChunk>& Pair : Chunks)
	{
		if (!Desired.Contains(Pair.Key))
		{
			ToUnload.Add(Pair.Key);
		}
	}

	for (const FIntVector& Coord : ToUnload)
	{
		UnloadChunk(Coord);
	}

	int32 Loaded = 0;
	for (const FIntVector& Coord : LoadOrder)
	{
		if (Chunks.Contains(Coord))
		{
			continue;
		}

		GenerateChunk(Coord);
		++Loaded;
		if (Loaded >= MaxChunkLoadsPerUpdate)
		{
			break;
		}
	}
}

void AVoxelWorld::GenerateChunk(const FIntVector& Coord)
{
	FVoxelWorldChunk& Chunk = Chunks.Add(Coord);

	const int32 N = NgxVoxel::ChunkSize;
	for (int32 Z = 0; Z < N; ++Z)
	for (int32 Y = 0; Y < N; ++Y)
	for (int32 X = 0; X < N; ++X)
	{
		const int32 GX = Coord.X * N + X;
		const int32 GY = Coord.Y * N + Y;
		const int32 GZ = Coord.Z * N + Z;
		Chunk.Data.Set(X, Y, Z, GenerateMaterialAt(GX, GY, GZ));
	}

	UVoxelProceduralMeshComponent* Mesh = NewObject<UVoxelProceduralMeshComponent>(this);
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetRelativeLocation(ChunkOrigin(Coord));
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->bUseAsyncCooking = true;
	Mesh->SharedMaterial = VoxelMaterial;
	Mesh->SetCollisionEnabled(bCreateCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	Mesh->SetCollisionObjectType(ECC_WorldStatic);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);
	AddInstanceComponent(Mesh);
	Mesh->RegisterComponent();

	Chunk.Mesh = Mesh;
	++Chunk.Version;
	MarkChunkAndNeighborsDirty(Coord);
}

void AVoxelWorld::UnloadChunk(const FIntVector& Coord)
{
	FVoxelWorldChunk Removed;
	if (!Chunks.RemoveAndCopyValue(Coord, Removed))
	{
		return;
	}

	DirtyChunks.Remove(Coord);
	InFlight.Remove(Coord);

	if (Removed.Mesh)
	{
		Removed.Mesh->ClearAllMeshSections();
		Removed.Mesh->DestroyComponent();
	}

	for (const FIntVector& Offset : NeighborOffsets)
	{
		if (Chunks.Contains(Coord + Offset))
		{
			DirtyChunks.Add(Coord + Offset);
		}
	}
}

int32 AVoxelWorld::ApplyDamageSphere(const FVector& WorldCenter, float RadiusCm, uint8 NewMaterial)
{
	if (RadiusCm <= 0.f)
	{
		return 0;
	}

	const FVector LocalCm = GetActorTransform().InverseTransformPosition(WorldCenter);
	const FVector CenterV = LocalCm / NgxVoxel::VoxelSizeCm;
	const float RadiusV = RadiusCm / NgxVoxel::VoxelSizeCm;
	const float RadiusVSq = RadiusV * RadiusV;

	const FIntVector MinV(
		FMath::FloorToInt(CenterV.X - RadiusV),
		FMath::FloorToInt(CenterV.Y - RadiusV),
		FMath::FloorToInt(CenterV.Z - RadiusV));
	const FIntVector MaxV(
		FMath::CeilToInt(CenterV.X + RadiusV),
		FMath::CeilToInt(CenterV.Y + RadiusV),
		FMath::CeilToInt(CenterV.Z + RadiusV));

	TSet<FIntVector> ChangedChunks;
	int32 Changed = 0;
	for (int32 Z = MinV.Z; Z <= MaxV.Z; ++Z)
	for (int32 Y = MinV.Y; Y <= MaxV.Y; ++Y)
	for (int32 X = MinV.X; X <= MaxV.X; ++X)
	{
		const FVector VoxelCenter((float)X + 0.5f, (float)Y + 0.5f, (float)Z + 0.5f);
		if (FVector::DistSquared(VoxelCenter, CenterV) > RadiusVSq)
		{
			continue;
		}

		FIntVector ChunkCoord;
		FIntVector LocalVoxel;
		FVoxelWorldChunk* Chunk = FindChunkForGlobalVoxel(FIntVector(X, Y, Z), ChunkCoord, LocalVoxel);
		if (!Chunk)
		{
			continue;
		}

		if (Chunk->Data.Get(LocalVoxel.X, LocalVoxel.Y, LocalVoxel.Z) == NewMaterial)
		{
			continue;
		}

		Chunk->Data.Set(LocalVoxel.X, LocalVoxel.Y, LocalVoxel.Z, NewMaterial);
		++Chunk->Version;
		++Changed;
		ChangedChunks.Add(ChunkCoord);

		const int32 N = NgxVoxel::ChunkSize;
		if (LocalVoxel.X == 0)     ChangedChunks.Add(ChunkCoord + FIntVector(-1, 0, 0));
		if (LocalVoxel.X == N - 1) ChangedChunks.Add(ChunkCoord + FIntVector( 1, 0, 0));
		if (LocalVoxel.Y == 0)     ChangedChunks.Add(ChunkCoord + FIntVector(0, -1, 0));
		if (LocalVoxel.Y == N - 1) ChangedChunks.Add(ChunkCoord + FIntVector(0,  1, 0));
		if (LocalVoxel.Z == 0)     ChangedChunks.Add(ChunkCoord + FIntVector(0, 0, -1));
		if (LocalVoxel.Z == N - 1) ChangedChunks.Add(ChunkCoord + FIntVector(0, 0,  1));
	}

	for (const FIntVector& Coord : ChangedChunks)
	{
		if (Chunks.Contains(Coord))
		{
			DirtyChunks.Add(Coord);
		}
	}
	return Changed;
}

bool AVoxelWorld::RaycastSolidVoxel(const FVector& WorldStart, const FVector& WorldEnd,
	FVector& OutWorldImpact, FVector& OutWorldNormal) const
{
	const FTransform& Xf = GetActorTransform();
	const FVector LocalStart = Xf.InverseTransformPosition(WorldStart);
	const FVector LocalEnd = Xf.InverseTransformPosition(WorldEnd);
	const FVector StartV = LocalStart / NgxVoxel::VoxelSizeCm;
	const FVector EndV = LocalEnd / NgxVoxel::VoxelSizeCm;
	const FVector DeltaV = EndV - StartV;

	if (DeltaV.IsNearlyZero())
	{
		return false;
	}

	FIntVector Voxel(
		FMath::FloorToInt(StartV.X),
		FMath::FloorToInt(StartV.Y),
		FMath::FloorToInt(StartV.Z));

	const FIntVector Step(
		DeltaV.X > 0.f ? 1 : (DeltaV.X < 0.f ? -1 : 0),
		DeltaV.Y > 0.f ? 1 : (DeltaV.Y < 0.f ? -1 : 0),
		DeltaV.Z > 0.f ? 1 : (DeltaV.Z < 0.f ? -1 : 0));

	FVector TMax(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector TDelta(FLT_MAX, FLT_MAX, FLT_MAX);
	if (Step.X != 0)
	{
		const float Boundary = (float)(Voxel.X + (Step.X > 0 ? 1 : 0));
		TMax.X = (Boundary - StartV.X) / DeltaV.X;
		TDelta.X = 1.f / FMath::Abs(DeltaV.X);
	}
	if (Step.Y != 0)
	{
		const float Boundary = (float)(Voxel.Y + (Step.Y > 0 ? 1 : 0));
		TMax.Y = (Boundary - StartV.Y) / DeltaV.Y;
		TDelta.Y = 1.f / FMath::Abs(DeltaV.Y);
	}
	if (Step.Z != 0)
	{
		const float Boundary = (float)(Voxel.Z + (Step.Z > 0 ? 1 : 0));
		TMax.Z = (Boundary - StartV.Z) / DeltaV.Z;
		TDelta.Z = 1.f / FMath::Abs(DeltaV.Z);
	}

	const int32 MaxSteps = FMath::CeilToInt(DeltaV.Size()) + 4;
	float T = 0.f;
	FVector LocalNormal = -DeltaV.GetSafeNormal();
	for (int32 StepIndex = 0; StepIndex < MaxSteps && T <= 1.f; ++StepIndex)
	{
		if (GetMaterialAtGlobalVoxel(Voxel) != 0)
		{
			const FVector LocalImpact = FMath::Lerp(LocalStart, LocalEnd, T);
			OutWorldImpact = Xf.TransformPosition(LocalImpact);
			OutWorldNormal = Xf.TransformVectorNoScale(LocalNormal).GetSafeNormal();
			return true;
		}

		if (TMax.X <= TMax.Y && TMax.X <= TMax.Z)
		{
			Voxel.X += Step.X;
			T = TMax.X;
			TMax.X += TDelta.X;
			LocalNormal = FVector((float)-Step.X, 0.f, 0.f);
		}
		else if (TMax.Y <= TMax.Z)
		{
			Voxel.Y += Step.Y;
			T = TMax.Y;
			TMax.Y += TDelta.Y;
			LocalNormal = FVector(0.f, (float)-Step.Y, 0.f);
		}
		else
		{
			Voxel.Z += Step.Z;
			T = TMax.Z;
			TMax.Z += TDelta.Z;
			LocalNormal = FVector(0.f, 0.f, (float)-Step.Z);
		}
	}

	return false;
}

void AVoxelWorld::MarkChunkAndNeighborsDirty(const FIntVector& Coord)
{
	DirtyChunks.Add(Coord);
	for (const FIntVector& Offset : NeighborOffsets)
	{
		const FIntVector NeighborCoord = Coord + Offset;
		if (Chunks.Contains(NeighborCoord))
		{
			DirtyChunks.Add(NeighborCoord);
		}
	}
}

void AVoxelWorld::DispatchChunk(const FIntVector& Coord)
{
	FVoxelWorldChunk* Chunk = Chunks.Find(Coord);
	if (!Chunk || InFlight.Contains(Coord))
	{
		return;
	}

	if (!Pipeline.IsValid())
	{
		Pipeline = MakeShared<FVoxelWorldMeshPipeline, ESPMode::ThreadSafe>();
	}

	FWorldChunkSnapshot Snapshot;
	Snapshot.Center = Chunk->Data;
	for (int32 Index = 0; Index < 6; ++Index)
	{
		if (const FVoxelWorldChunk* Neighbor = Chunks.Find(Coord + NeighborOffsets[Index]))
		{
			Snapshot.Neighbor[Index] = Neighbor->Data;
			Snapshot.bHas[Index] = true;
		}
	}

	const bool bGreedy = bUseGreedy;
	const uint64 Version = Chunk->Version;
	TSharedPtr<FVoxelWorldMeshPipeline, ESPMode::ThreadSafe> Pipe = Pipeline;
	InFlight.Add(Coord);

	Async(EAsyncExecution::ThreadPool,
		[Snapshot = MoveTemp(Snapshot), bGreedy, Version, Coord, Pipe]()
		{
			FVoxelWorldMeshResult Result;
			Result.Coord = Coord;
			Result.Version = Version;

			const FChunkNeighbors Neighbors = Snapshot.MakeNeighbors();
			if (bGreedy)
			{
				FVoxelMesher::GenerateGreedy(Snapshot.Center, Neighbors, FVector::ZeroVector, Result.Data);
			}
			else
			{
				FVoxelMesher::GenerateNaive(Snapshot.Center, Neighbors, FVector::ZeroVector, Result.Data);
			}

			if (Pipe.IsValid())
			{
				Pipe->Completed.Enqueue(MoveTemp(Result));
			}
		});
}

void AVoxelWorld::ApplyResult(FVoxelWorldMeshResult& Result)
{
	InFlight.Remove(Result.Coord);

	FVoxelWorldChunk* Chunk = Chunks.Find(Result.Coord);
	if (!Chunk || Chunk->Version != Result.Version || DirtyChunks.Contains(Result.Coord))
	{
		return;
	}

	if (!Chunk->Mesh)
	{
		return;
	}

	if (Result.Data.IsEmpty())
	{
		Chunk->Mesh->ClearMeshSection(0);
		return;
	}

	Chunk->Mesh->CreateMeshSection(
		0,
		Result.Data.Vertices,
		Result.Data.Triangles,
		Result.Data.Normals,
		Result.Data.UV0,
		Result.Data.VertexColors,
		TArray<FProcMeshTangent>(),
		bCreateCollision);
}

FVector AVoxelWorld::ChunkOrigin(const FIntVector& Coord) const
{
	const float ChunkCm = (float)NgxVoxel::ChunkSize * NgxVoxel::VoxelSizeCm;
	return FVector(Coord.X * ChunkCm, Coord.Y * ChunkCm, Coord.Z * ChunkCm);
}

FIntVector AVoxelWorld::WorldToChunkCoord(const FVector& WorldLocation) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldLocation);
	const float ChunkCm = (float)NgxVoxel::ChunkSize * NgxVoxel::VoxelSizeCm;
	return FIntVector(
		FMath::FloorToInt(Local.X / ChunkCm),
		FMath::FloorToInt(Local.Y / ChunkCm),
		FMath::Max(0, FMath::FloorToInt(Local.Z / ChunkCm)));
}

uint8 AVoxelWorld::GenerateMaterialAt(int32 GlobalX, int32 GlobalY, int32 GlobalZ) const
{
	const FVector2D P((float)GlobalX, (float)GlobalY);
	const float SeedOffset = (float)Seed * 0.013f;
	const float Hills = FMath::PerlinNoise2D(P * 0.018f + FVector2D(SeedOffset, SeedOffset * 1.7f));
	const float Detail = FMath::PerlinNoise2D(P * 0.071f + FVector2D(SeedOffset * 3.1f, -SeedOffset));

	const int32 MaxZ = FMath::Max(ChunkHeightChunks, 1) * NgxVoxel::ChunkSize - 1;
	const int32 Height = FMath::Clamp(
		24 + FMath::RoundToInt(Hills * 18.f + Detail * 5.f),
		1,
		MaxZ);

	if (GlobalZ > Height)
	{
		return 0;
	}
	if (GlobalZ == Height)
	{
		return 3; // grass
	}
	if (GlobalZ > Height - 4)
	{
		return 1; // earth
	}
	return 4; // rock/brick placeholder
}

uint8 AVoxelWorld::GetMaterialAtGlobalVoxel(const FIntVector& GlobalVoxel) const
{
	FIntVector ChunkCoord;
	FIntVector LocalVoxel;
	const FVoxelWorldChunk* Chunk = FindChunkForGlobalVoxel(GlobalVoxel, ChunkCoord, LocalVoxel);
	return Chunk ? Chunk->Data.Get(LocalVoxel.X, LocalVoxel.Y, LocalVoxel.Z) : 0;
}

FVoxelWorldChunk* AVoxelWorld::FindChunkForGlobalVoxel(const FIntVector& GlobalVoxel, FIntVector& OutChunkCoord,
	FIntVector& OutLocalVoxel)
{
	const int32 N = NgxVoxel::ChunkSize;
	OutChunkCoord = FIntVector(
		FloorDiv(GlobalVoxel.X, N),
		FloorDiv(GlobalVoxel.Y, N),
		FloorDiv(GlobalVoxel.Z, N));
	OutLocalVoxel = FIntVector(
		PositiveMod(GlobalVoxel.X, N),
		PositiveMod(GlobalVoxel.Y, N),
		PositiveMod(GlobalVoxel.Z, N));
	return Chunks.Find(OutChunkCoord);
}

const FVoxelWorldChunk* AVoxelWorld::FindChunkForGlobalVoxel(const FIntVector& GlobalVoxel,
	FIntVector& OutChunkCoord, FIntVector& OutLocalVoxel) const
{
	const int32 N = NgxVoxel::ChunkSize;
	OutChunkCoord = FIntVector(
		FloorDiv(GlobalVoxel.X, N),
		FloorDiv(GlobalVoxel.Y, N),
		FloorDiv(GlobalVoxel.Z, N));
	OutLocalVoxel = FIntVector(
		PositiveMod(GlobalVoxel.X, N),
		PositiveMod(GlobalVoxel.Y, N),
		PositiveMod(GlobalVoxel.Z, N));
	return Chunks.Find(OutChunkCoord);
}

int32 AVoxelWorld::FloorDiv(int32 Value, int32 Divisor)
{
	check(Divisor > 0);
	if (Value >= 0)
	{
		return Value / Divisor;
	}
	return -(((-Value) + Divisor - 1) / Divisor);
}

int32 AVoxelWorld::PositiveMod(int32 Value, int32 Divisor)
{
	check(Divisor > 0);
	const int32 M = Value % Divisor;
	return M < 0 ? M + Divisor : M;
}

AActor* AVoxelWorld::ResolveFocusActor() const
{
	if (FocusActor)
	{
		return FocusActor;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			return Pawn;
		}
	}
	return const_cast<AVoxelWorld*>(this);
}
