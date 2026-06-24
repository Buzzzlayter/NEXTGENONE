// Copyright. NgxVoxel — chunk meshing.

#pragma once

#include "CoreMinimal.h"

struct FVoxelChunk;

// Буферы под ProceduralMeshComponent::CreateMeshSection.
struct NGXVOXEL_API FVoxelMeshData
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FColor> VertexColors;

	void Reset();
	bool IsEmpty() const { return Triangles.Num() == 0; }
};

// Соседние чанки по каждой оси — нужны для face-culling на границах чанка.
// Индекс = ось (0=X, 1=Y, 2=Z). nullptr = соседа нет (край структуры → граничная грань
// генерируется как внешняя поверхность). Только const-указатели на данные владельца —
// структура plain, безопасна для async-вызова мешера на воркере.
struct FChunkNeighbors
{
	const FVoxelChunk* Neg[3] = { nullptr, nullptr, nullptr }; // соседи в −X / −Y / −Z
	const FVoxelChunk* Pos[3] = { nullptr, nullptr, nullptr }; // соседи в +X / +Y / +Z
};

class NGXVOXEL_API FVoxelMesher
{
public:
	// Наивный face-culled меш: 2 треугольника на каждую открытую грань. Оставлен для A/B-сравнения.
	// Neighbors — соседи для куллинга межчанковых граней; OriginCm — смещение чанка (см) в простр. актёра.
	static void GenerateNaive(const FVoxelChunk& Chunk, const FChunkNeighbors& Neighbors,
		const FVector& OriginCm, FVoxelMeshData& Out);

	// Greedy meshing (алгоритм 0fps по 3 осям): объединяет компланарные смежные грани
	// одного материала в крупные квады → кратно меньше треугольников. Та же ориентация/нормали,
	// что и у наивного. Stateless — пригоден для async-вызова на воркере.
	static void GenerateGreedy(const FVoxelChunk& Chunk, const FChunkNeighbors& Neighbors,
		const FVector& OriginCm, FVoxelMeshData& Out);

	// Меш произвольного набора вокселей (оторванный кусок под дебрис; может быть больше чанка).
	// Membership в Voxels = solid; грань генерится, если соседа в наборе нет. Вершины — в локали
	// относительно OriginVoxel (в воксельных координатах), умноженные на размер вокселя.
	static void GenerateFromVoxelSet(
		const TSet<FIntVector>& Voxels,
		TFunctionRef<uint8(const FIntVector&)> MaterialOf,
		const FIntVector& OriginVoxel,
		FVoxelMeshData& Out);
};
