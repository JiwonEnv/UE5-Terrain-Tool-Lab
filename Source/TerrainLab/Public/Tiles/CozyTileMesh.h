#pragma once
// 작업 2(모양 만들기)가 Private/Tiles/CozyTileMesh.cpp에 구현한다. 공개 함수 모양은 약속이다.
#include "CoreMinimal.h"
#include "Tiles/CozyTileTypes.h"

class UCozyTileGrid;

/** 칸 묶음 하나의 메시 데이터 (ProceduralMeshComponent에 그대로 넣는 형태) */
struct TERRAINLAB_API FCozyTileMeshData
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> Colors;   // 윗면은 바닥 종류 색, 절벽 옆면은 흙·바위 색
	void Reset() { Vertices.Reset(); Triangles.Reset(); Normals.Reset(); UVs.Reset(); Colors.Reset(); }
};

/** 칸 데이터 → 모양. 이웃 칸 층을 보고 윗면·절벽 옆면·바깥/안쪽 모서리를 골라 이어지게 만든다. 순수 계산 (자동 시험 대상) */
struct TERRAINLAB_API FCozyTileMeshBuilder
{
	/** 사각형 범위 [Min, Max] 칸들의 땅 모양을 Out에 덧붙인다 · 좌표는 지형 액터 기준(칸 (0,0)의 모서리가 원점, 칸 X는 +X, 칸 Y는 +Y) */
	static void BuildRegion(const UCozyTileGrid& Grid, FIntPoint Min, FIntPoint Max, FCozyTileMeshData& Out);
	/** 같은 범위의 물 표면 (물 칸만) */
	static void BuildWater(const UCozyTileGrid& Grid, FIntPoint Min, FIntPoint Max, FCozyTileMeshData& Out);
};
