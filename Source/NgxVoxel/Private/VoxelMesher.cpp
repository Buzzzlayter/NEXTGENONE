// Copyright. NgxVoxel — naive face-culled mesher.

#include "VoxelMesher.h"
#include "VoxelChunk.h"
#include "VoxelTypes.h"

void FVoxelMeshData::Reset()
{
	Vertices.Reset();
	Triangles.Reset();
	Normals.Reset();
	UV0.Reset();
	VertexColors.Reset();
}

namespace
{
	// Смещения к соседнему вокселю по 6 граням. Порядок: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z.
	const FIntVector FaceDirs[6] = {
		{ 1, 0, 0 }, { -1, 0, 0 },
		{ 0, 1, 0 }, { 0, -1, 0 },
		{ 0, 0, 1 }, { 0, 0, -1 }
	};

	const FVector FaceNormals[6] = {
		{ 1, 0, 0 }, { -1, 0, 0 },
		{ 0, 1, 0 }, { 0, -1, 0 },
		{ 0, 0, 1 }, { 0, 0, -1 }
	};

	// 4 угла на грань (в воксельных единицах), CCW при взгляде снаружи.
	// Если меш отрисуется наизнанку — поменять порядок треугольников (winding) в цикле ниже.
	const FVector FaceCorners[6][4] = {
		// +X
		{ { 1, 0, 0 }, { 1, 1, 0 }, { 1, 1, 1 }, { 1, 0, 1 } },
		// -X
		{ { 0, 1, 0 }, { 0, 0, 0 }, { 0, 0, 1 }, { 0, 1, 1 } },
		// +Y
		{ { 1, 1, 0 }, { 0, 1, 0 }, { 0, 1, 1 }, { 1, 1, 1 } },
		// -Y
		{ { 0, 0, 0 }, { 1, 0, 0 }, { 1, 0, 1 }, { 0, 0, 1 } },
		// +Z
		{ { 0, 0, 1 }, { 1, 0, 1 }, { 1, 1, 1 }, { 0, 1, 1 } },
		// -Z
		{ { 0, 1, 0 }, { 1, 1, 0 }, { 1, 0, 0 }, { 0, 0, 0 } }
	};

	FColor MaterialColor(uint8 Material)
	{
		switch (Material)
		{
		case 1:  return FColor(170, 160, 145); // камень
		case 2:  return FColor(120, 110, 90);  // укреплённый
		default: return FColor(200, 70, 60);   // слабый/прочее
		}
	}

	// Материал вокселя в локальных координатах чанка. Если координата выходит за грань ровно
	// по одной оси — берём воксель из соседнего чанка по этой оси (null-сосед → пусто). Для
	// face-culling этого достаточно: смещение всегда ±1 по одной оси.
	FORCEINLINE uint8 SampleMat(int32 X, int32 Y, int32 Z,
		const FVoxelChunk& C, const FChunkNeighbors& Nbr)
	{
		const int32 N = NgxVoxel::ChunkSize;
		int32 P[3] = { X, Y, Z };
		for (int32 a = 0; a < 3; ++a)
		{
			if (P[a] < 0)
			{
				const FVoxelChunk* Nb = Nbr.Neg[a];
				if (!Nb) return 0;
				P[a] += N;                          // −1 → N−1 в соседе
				return Nb->Get(P[0], P[1], P[2]);
			}
			if (P[a] >= N)
			{
				const FVoxelChunk* Nb = Nbr.Pos[a];
				if (!Nb) return 0;
				P[a] -= N;                          // N → 0 в соседе
				return Nb->Get(P[0], P[1], P[2]);
			}
		}
		return C.Get(X, Y, Z);
	}

	// Собрать вершину по компонентам осей (d — постоянная плоскость, u/v — стороны квада).
	FORCEINLINE FVector AxisVert(int32 d, int32 u, int32 v, int32 pd, int32 cu, int32 cv, float S)
	{
		float P[3] = { 0.f, 0.f, 0.f };
		P[d] = (float)pd * S;
		P[u] = (float)cu * S;
		P[v] = (float)cv * S;
		return FVector(P[0], P[1], P[2]);
	}

	// Добавить один greedy-квад (w×h в воксельных единицах) в буфер.
	// MaskVal > 0 → грань смотрит в +d, < 0 → в -d; |MaskVal| = material id.
	void AddGreedyQuad(FVoxelMeshData& Out, const FVector& Origin, int32 d, int32 u, int32 v,
		int32 Pd, int32 Iu, int32 Jv, int32 W, int32 H, int32 MaskVal, float S)
	{
		const bool bPositive = MaskVal > 0;
		const uint8 Material = (uint8)FMath::Abs(MaskVal);
		const FColor Col = MaterialColor(Material);

		const FVector V00 = Origin + AxisVert(d, u, v, Pd, Iu,     Jv,     S);
		const FVector V10 = Origin + AxisVert(d, u, v, Pd, Iu + W, Jv,     S);
		const FVector V11 = Origin + AxisVert(d, u, v, Pd, Iu + W, Jv + H, S);
		const FVector V01 = Origin + AxisVert(d, u, v, Pd, Iu,     Jv + H, S);

		FVector Normal(0.f, 0.f, 0.f);
		Normal[d] = bPositive ? 1.f : -1.f;

		const int32 Base = Out.Vertices.Num();

		// UV в воксельных единицах → текстура тайлится по поверхности, не растягивается на квад.
		Out.Vertices.Add(V00); Out.UV0.Add(FVector2D(0.f,        0.f));
		Out.Vertices.Add(V10); Out.UV0.Add(FVector2D((float)W,   0.f));
		Out.Vertices.Add(V11); Out.UV0.Add(FVector2D((float)W,   (float)H));
		Out.Vertices.Add(V01); Out.UV0.Add(FVector2D(0.f,        (float)H));

		for (int32 C = 0; C < 4; ++C)
		{
			Out.Normals.Add(Normal);
			Out.VertexColors.Add(Col);
		}

		// Winding выверен по проверенному наивному мешеру (геом. cross смотрит против внешней нормали).
		if (bPositive)
		{
			Out.Triangles.Add(Base + 0); Out.Triangles.Add(Base + 2); Out.Triangles.Add(Base + 1);
			Out.Triangles.Add(Base + 0); Out.Triangles.Add(Base + 3); Out.Triangles.Add(Base + 2);
		}
		else
		{
			Out.Triangles.Add(Base + 0); Out.Triangles.Add(Base + 1); Out.Triangles.Add(Base + 2);
			Out.Triangles.Add(Base + 0); Out.Triangles.Add(Base + 2); Out.Triangles.Add(Base + 3);
		}
	}
}

void FVoxelMesher::GenerateNaive(const FVoxelChunk& Chunk, const FChunkNeighbors& Neighbors,
	const FVector& OriginCm, FVoxelMeshData& Out)
{
	Out.Reset();

	const float S = NgxVoxel::VoxelSizeCm;
	const int32 N = NgxVoxel::ChunkSize;

	for (int32 Z = 0; Z < N; ++Z)
	for (int32 Y = 0; Y < N; ++Y)
	for (int32 X = 0; X < N; ++X)
	{
		const uint8 Material = Chunk.Get(X, Y, Z);
		if (Material == 0)
		{
			continue;
		}

		const FVector Base(OriginCm.X + (float)X * S, OriginCm.Y + (float)Y * S, OriginCm.Z + (float)Z * S);

		for (int32 F = 0; F < 6; ++F)
		{
			const FIntVector Dir = FaceDirs[F];
			if (SampleMat(X + Dir.X, Y + Dir.Y, Z + Dir.Z, Chunk, Neighbors) != 0)
			{
				continue; // грань скрыта соседом (в т.ч. в соседнем чанке)
			}

			const int32 V0 = Out.Vertices.Num();
			const FColor Col = MaterialColor(Material);

			for (int32 C = 0; C < 4; ++C)
			{
				Out.Vertices.Add(Base + FaceCorners[F][C] * S);
				Out.Normals.Add(FaceNormals[F]);
				Out.UV0.Add(FVector2D((C == 1 || C == 2) ? 1.f : 0.f, (C >= 2) ? 1.f : 0.f));
				Out.VertexColors.Add(Col);
			}

			// Winding: CW при взгляде снаружи (UE front-face). Если меш наизнанку — переставить.
			Out.Triangles.Add(V0 + 0);
			Out.Triangles.Add(V0 + 2);
			Out.Triangles.Add(V0 + 1);
			Out.Triangles.Add(V0 + 0);
			Out.Triangles.Add(V0 + 3);
			Out.Triangles.Add(V0 + 2);
		}
	}
}

void FVoxelMesher::GenerateGreedy(const FVoxelChunk& Chunk, const FChunkNeighbors& Neighbors,
	const FVector& OriginCm, FVoxelMeshData& Out)
{
	Out.Reset();

	const float S = NgxVoxel::VoxelSizeCm;
	const int32 N = NgxVoxel::ChunkSize;
	const int32 Dims[3] = { N, N, N };

	// Маска одного среза: знак = направление грани, |val| = material id, 0 = нет грани.
	TArray<int32> Mask;
	Mask.SetNumUninitialized(N * N);

	// d — основная ось (нормаль грани), u/v — две поперечные оси (циклически правые).
	for (int32 d = 0; d < 3; ++d)
	{
		const int32 u = (d + 1) % 3;
		const int32 v = (d + 2) % 3;

		int32 x[3] = { 0, 0, 0 };
		int32 q[3] = { 0, 0, 0 };
		q[d] = 1;

		// Идём по срезам вдоль d, включая обе внешние границы чанка (-1 .. Dims[d]-1).
		for (x[d] = -1; x[d] < Dims[d]; )
		{
			// 1) Построить маску видимых граней между срезом x[d] и x[d]+1.
			int32 MaskIdx = 0;
			for (x[v] = 0; x[v] < Dims[v]; ++x[v])
			for (x[u] = 0; x[u] < Dims[u]; ++x[u])
			{
				// Грань между срезом x[d] и x[d]+1. Крайние срезы (x[d]==-1 или x[d]+1==N)
				// уходят в соседний чанк через SampleMat → межчанковые грани куллятся.
				const uint8 Va = SampleMat(x[0], x[1], x[2], Chunk, Neighbors);
				const uint8 Vb = SampleMat(x[0] + q[0], x[1] + q[1], x[2] + q[2], Chunk, Neighbors);

				const bool Sa = Va != 0;
				const bool Sb = Vb != 0;

				if (Sa == Sb)            Mask[MaskIdx] = 0;       // оба solid (внутр.) или оба пусто
				else if (Sa)             Mask[MaskIdx] = (int32)Va;   // грань смотрит в +d
				else                     Mask[MaskIdx] = -(int32)Vb;  // грань смотрит в -d
				++MaskIdx;
			}

			++x[d];

			// 2) Жадно собрать прямоугольники из маски.
			MaskIdx = 0;
			for (int32 j = 0; j < Dims[v]; ++j)
			{
				for (int32 i = 0; i < Dims[u]; )
				{
					const int32 C = Mask[MaskIdx];
					if (C == 0)
					{
						++i;
						++MaskIdx;
						continue;
					}

					// Ширина: тянем вдоль u, пока совпадает значение маски.
					int32 W = 1;
					while (i + W < Dims[u] && Mask[MaskIdx + W] == C)
					{
						++W;
					}

					// Высота: тянем вдоль v, пока вся строка шириной W совпадает.
					int32 H = 1;
					bool bStop = false;
					while (j + H < Dims[v])
					{
						for (int32 k = 0; k < W; ++k)
						{
							if (Mask[MaskIdx + k + H * Dims[u]] != C)
							{
								bStop = true;
								break;
							}
						}
						if (bStop) break;
						++H;
					}

					AddGreedyQuad(Out, OriginCm, d, u, v, /*Pd=*/x[d], /*Iu=*/i, /*Jv=*/j, W, H, C, S);

					// Затереть использованную область маски.
					for (int32 hh = 0; hh < H; ++hh)
					for (int32 ww = 0; ww < W; ++ww)
					{
						Mask[MaskIdx + ww + hh * Dims[u]] = 0;
					}

					i += W;
					MaskIdx += W;
				}
			}
		}
	}
}
