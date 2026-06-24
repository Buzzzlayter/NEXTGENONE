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

	// --- Коллизия (шаг 5b): кукать collision только у чанков около «точки фокуса» ---
	// Точку фокуса задаёт ИГРА извне через SetCollisionFocus (модуль вокселей про игрока не знает).

	// Скоупить ли коллизию по фокусу. false → кукать у всех чанков.
	UPROPERTY(EditAnywhere, Category = "Voxel|Collision")
	bool bScopeCollisionToFocus = true;

	// Радиус активной коллизии вокруг фокуса (см). Должен покрывать дальность взаимодействия.
	UPROPERTY(EditAnywhere, Category = "Voxel|Collision", meta = (ClampMin = "0"))
	float CollisionActiveRadiusCm = 2000.f;

	// --- Структурная целостность (шаг 6) ---

	// Проверять связность после разрушения; оторванное (не достигает якорей) — удаляется/падает.
	UPROPERTY(EditAnywhere, Category = "Voxel|Structure")
	bool bEnableStructuralIntegrity = true;

	// Solid-воксели с глобальным Z <= этого — якоря («земля»). Дефолт — нижний слой чанков.
	UPROPERTY(EditAnywhere, Category = "Voxel|Structure")
	int32 AnchorMaxGlobalZ = NgxVoxel::ChunkSize - 1;

	// Спавнить оторванное телом Chaos (падает). false → просто удалять (поведение 6a).
	UPROPERTY(EditAnywhere, Category = "Voxel|Structure")
	bool bSpawnDebris = true;

	// Время жизни дебриса до авто-деспавна (сек).
	UPROPERTY(EditAnywhere, Category = "Voxel|Structure")
	float DebrisLifeSeconds = 8.f;

	// Куски мельче этого (в вокселях) — без дебриса, просто удаляются.
	UPROPERTY(EditAnywhere, Category = "Voxel|Structure", meta = (ClampMin = "1"))
	int32 MinDebrisVoxels = 2;

	// --- Каскад (шаг 7): фронт обрушения, идущий сверху вниз ---

	// Скорость опускания фронта каскада (см/с).
	UPROPERTY(EditAnywhere, Category = "Voxel|Cascade", meta = (ClampMin = "1"))
	float CascadeSpeedCmS = 400.f;

	// Полный ребилд: пересобрать тестовые чанки и весь меш.
	UFUNCTION(CallInEditor, Category = "Voxel")
	void Rebuild();

	// Точечный ремеш одного (центрального) чанка — демонстрация dirty-пути без полного ребилда.
	UFUNCTION(CallInEditor, Category = "Voxel")
	void RemeshCenterChunk();

	// --- Разрушение (шаг 4) ---

	// Радиус тестового воздействия (см).
	UPROPERTY(EditAnywhere, Category = "Voxel|Damage", meta = (ClampMin = "0"))
	float TestDamageRadiusCm = 25.f;

	// Материал тестового воздействия: 0 = убрать воксели (бур/заряд), >0 = добавить (опора/балка).
	UPROPERTY(EditAnywhere, Category = "Voxel|Damage")
	uint8 TestDamageMaterial = 0;

	// Точка удара в локальных координатах актёра (см). Дефолт — у верхней грани тестового шара.
	UPROPERTY(EditAnywhere, Category = "Voxel|Damage")
	FVector TestDamageLocalCm = FVector(80.f, 80.f, 150.f);

	// Применить тестовое сферическое воздействие в TestDamageLocalCm (кнопка в редакторе).
	UFUNCTION(CallInEditor, Category = "Voxel|Damage")
	void ApplyTestDamage();

	// Сферическое воздействие в МИРОВЫХ координатах. NewMaterial==0 убирает воксели (бур/заряд),
	// >0 добавляет (опора). Возвращает число изменённых вокселей; затронутые чанки (+ соседи на
	// границах) ставятся в async-ремеш. Точка входа для будущей привязки к прицелу игрока.
	int32 ApplyDamageSphere(const FVector& WorldCenter, float RadiusCm, uint8 NewMaterial);

	// Задать «точку фокуса» (мир) для скоупинга коллизии — вызывает ИГРА (паун/камера).
	// Модуль вокселей про игрока не знает: получает только мировую точку и сам решает,
	// какие чанки в радиусе → им и кукает collision.
	void SetCollisionFocus(const FVector& WorldCenter);

	// Запустить/остановить каскад обрушения (фронт сверху вниз).
	UFUNCTION(CallInEditor, Category = "Voxel|Cascade")
	void StartCascade();

	UFUNCTION(CallInEditor, Category = "Voxel|Cascade")
	void StopCascade();

	// Метрики для перф-оверлея (шаг 8). Только чтение.
	int32 GetNumChunks() const { return Chunks.Num(); }
	int32 GetNumSections() const { return SectionOf.Num(); }
	bool IsCascadeRunning() const { return bCascadeRunning; }

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
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

	// Шаг 6 — заглушка: здесь будет запуск FStructuralSolver на затронутом регионе.
	void NotifyStructuralShock(const FVector& WorldCenter, float RadiusCm) const;

	// --- Коллизия (шаг 5b): скоупинг на активные чанки около точки фокуса ---
	FVector CollisionFocus = FVector::ZeroVector;
	bool bHasFocus = false;          // задавалась ли точка фокуса игрой (до этого кукаем всё)
	TSet<FIntVector> ActiveChunks;   // чанки, для которых сейчас кукается коллизия
	bool ShouldCookCollision(const FIntVector& Coord) const;
	void UpdateActiveChunks(bool bForceAllDirty); // пересчёт активных чанков по CollisionFocus

	// --- Структурная целостность (шаг 6) ---
	bool bIntegrityDirty = false;   // после удаления вокселей нужна проверка связности
	void RunIntegrityCheck();
	void MarkChunkDirtyAround(const FIntVector& ChunkCoord, int32 LX, int32 LY, int32 LZ);

	// --- Каскад (шаг 7) ---
	bool bCascadeRunning = false;
	float CascadeFrontVoxelZ = 0.f;     // текущий фронт в воксельных Z (идёт сверху вниз)
	void AdvanceCascade(float Dt);      // двигает фронт и выпиливает пройденную полосу
};
