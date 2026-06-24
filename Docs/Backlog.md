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