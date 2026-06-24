// Copyright. NgxVoxel — destructible voxel structure actor (VS0 stub).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelChunk.h"
#include "VoxelMesher.h"
#include "Containers/Queue.h"
#include "Containers/Ticker.h"
#include "Templates/SharedPointer.h"
#include "VoxelStructure.generated.h"

class UProceduralMeshComponent;

// Результат меширования одного чанка: собран на воркере, применяется на game thread.
struct FChunkMeshResult
{
	FIntVector Coord = FIntVector(0, 0, 0);
	int32 SectionIndex = 0;
	uint64 Epoch = 0;
	FVoxelMeshData Data;
};

// Разделяемое thread-safe состояние пайплайна. Держится через TSharedPtr и переживает актёра,
// пока хоть один воркер ссылается на него → пуш результата никогда не попадёт в освобождённую память.
struct FVoxelMeshPipeline
{
	// Воркеры (производители) → game-thread тикер (единственный потребитель).
	TQueue<FChunkMeshResult, EQueueMode::Mpsc> Completed;
};

UCLASS()
class NGXVOXEL_API AVoxelStructure : public AActor
{
	GENERATED_BODY()

public:
	AVoxelStructure();

	UPROPERTY(VisibleAnywhere, Category = "Voxel")
	TObjectPtr<UProceduralMeshComponent> Mesh;

	// Материал тестового заполнения (1=камень, 2=укреплённый).
	UPROPERTY(EditAnywhere, Category = "Voxel")
	uint8 TestMaterial = 1;

	// true → greedy meshing (прод-путь); false → наивный (для A/B-сравнения тришек).
	UPROPERTY(EditAnywhere, Category = "Voxel")
	bool bUseGreedy = true;

	// Размер структуры в чанках (X×Y×Z). Тестовый шар вписывается в этот объём,
	// что показывает куллинг граней на границах чанков (внутренних «стен» быть не должно).
	// Значения <1 поджимаются до 1 в FillTestPattern.
	UPROPERTY(EditAnywhere, Category = "Voxel")
	FIntVector ChunkDims = FIntVector(2, 2, 2);

	// Меширование на воркере (game thread не блокируется). false → синхронно (A/B/отладка).
	UPROPERTY(EditAnywhere, Category = "Voxel|Async")
	bool bAsyncMeshing = true;

	// Бюджет: сколько чанков ставить в работу за кадр.
	UPROPERTY(EditAnywhere, Category = "Voxel|Async", meta = (ClampMin = "1"))
	int32 DispatchBudgetPerFrame = 4;

	// Бюджет: сколько готовых результатов применять (CreateMeshSection) за кадр.
	UPROPERTY(EditAnywhere, Category = "Voxel|Async", meta = (ClampMin = "1"))
	int32 ApplyBudgetPerFrame = 4;

	// Полный ребилд: пересобрать тестовые чанки и весь меш.
	UFUNCTION(CallInEditor, Category = "Voxel")
	void Rebuild();

	// Точечный ремеш одного (центрального) чанка — демонстрация dirty-пути без полного ребилда.
	UFUNCTION(CallInEditor, Category = "Voxel")
	void RemeshCenterChunk();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void BeginDestroy() override;

private:
	// Чанки структуры. Ключ = координата чанка в сетке (0..ChunkDims−1 по каждой оси).
	TMap<FIntVector, FVoxelChunk> Chunks;

	// Стабильный индекс ProcMesh-секции на чанк (по-чанковая инвалидация).
	TMap<FIntVector, int32> SectionOf;
	int32 NextSection = 0;

	// Очереди ремеша (только game thread).
	TSet<FIntVector> DirtyChunks;   // ждут меширования
	TSet<FIntVector> InFlight;      // задача уже в полёте
	uint64 BuildEpoch = 0;          // ++ на полный Rebuild → инвалидирует старые задачи

	// Async-инфраструктура.
	TSharedPtr<FVoxelMeshPipeline, ESPMode::ThreadSafe> Pipeline;
	FTSTicker::FDelegateHandle TickerHandle;

	// Сводка по текущей «сессии» ремеша (для одной лог-строки при простое).
	int32 SessionVerts = 0;
	int32 SessionTris = 0;
	int32 SessionSections = 0;

	void FillTestPattern();
	void RebuildSync();                                  // путь bAsyncMeshing=false

	void MarkAllDirty();
	void EnsureTicker();                                 // завести тикер, если ещё не активен
	void RemoveTicker();
	bool TickPipeline(float Dt);                         // применение + диспетчеризация (бюджет)
	void DispatchChunk(const FIntVector& Coord);         // снапшот + запуск воркера
	void ApplyResult(FChunkMeshResult& Result);          // CreateMeshSection на game thread

	FChunkNeighbors GatherNeighbors(const FIntVector& Coord) const; // для sync-пути
	FVector ChunkOrigin(const FIntVector& Coord) const;
};
