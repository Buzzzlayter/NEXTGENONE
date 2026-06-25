Незадокументированные / отложенные задачи. Сгруппировано по приоритету и категории.

Приоритеты:
- **P0** — критично, влияет на gameplay loop или содержит regression
- **P1** — sustainable improvement, нужно для долгосрочной устойчивости pipeline
- **P2** — nice to have, фичи / визуал
- **P3** — optimization / cleanup



### [P0] краш при закрытии редактора

Unhandled Exception: EXCEPTION_ACCESS_VIOLATION reading address 0xffffffffffffffff

UnrealEditor_Core
UnrealEditor_Core
UnrealEditor_NgxVoxel_patch_0!AVoxelStructure::~AVoxelStructure() [C:\Users\buzzz\OneDrive\Desktop\NEXTGENONE\Intermediate\Build\Win64\UnrealEditor\Inc\NgxVoxel\UHT\VoxelStructure.gen.cpp:430]
UnrealEditor_NgxVoxel_patch_0!AVoxelStructure::`vector deleting destructor'()



### [P3] перф: проверка опоры гоняет BFS по всему объёму структуры на каждый карв

`AVoxelStructure::RunIntegrityCheck` → `FStructuralSolver::FindUnsupportedComponents` делает 0-1 BFS
по ВСЕМУ объёму структуры (при `ChunkDims=6×6×3` это ~442k вокселей) на **game thread**, на каждое
изменение вокселей (карв ЛКМ, подрыв C). При частом карве по крупной структуре — возможны хитчи.

Когда дойдём до перфа (профайлером): варианты —
- **регионализация** — BFS только по затронутому региону + его «потолку», а не по всему объёму;
- **инкрементальность** — кэшировать карту опоры (Dist), пересчитывать только вокруг изменений;
- увести расчёт на **воркер** (как меширование) — результат применять на game thread;
- дебаунс уже есть (проверка на устаканивании меша), но сам проход всё равно O(объём).

Введено 2026-06-25 вместе с моделью опоры (замена бинарной связности). Перф решено смотреть позже.