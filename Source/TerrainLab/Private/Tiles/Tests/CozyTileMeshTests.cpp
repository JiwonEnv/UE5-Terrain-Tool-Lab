#include "Misc/AutomationTest.h"
#include "Tiles/CozyTileGrid.h"
#include "Tiles/CozyTileMesh.h"

#if WITH_DEV_AUTOMATION_TESTS

// 모양 만들기(작업 2) 자동 시험 · 칸은 SetCellRaw로 직접 놓는다 (작업 1 규칙과 무관)
namespace CozyTileMeshTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	UCozyTileGrid* MakeGrid(int32 W, int32 H)
	{
		UCozyTileGrid* G = NewObject<UCozyTileGrid>();
		G->Init(W, H, FCozyTileSettings());
		return G;
	}
	void SetLevel(UCozyTileGrid* G, int32 X, int32 Y, uint8 Level, ECozyTileGround Ground = ECozyTileGround::Grass)
	{
		FCozyTileCell C = G->GetCell(X, Y); C.Level = Level; C.Ground = Ground; G->SetCellRaw(X, Y, C);
	}
	FCozyTileMeshData BuildAll(const UCozyTileGrid* G)
	{
		FCozyTileMeshData D;
		FCozyTileMeshBuilder::BuildRegion(*G, FIntPoint(0, 0), FIntPoint(G->GetWidth() - 1, G->GetHeight() - 1), D);
		return D;
	}

	/** 삼각형 하나 (꼭짓점 세 개 + 첫 꼭짓점 법선) */
	struct FTri { FVector P[3]; FVector N; FColor Col; };
	TArray<FTri> Tris(const FCozyTileMeshData& D)
	{
		TArray<FTri> Out;
		for (int32 I = 0; I + 2 < D.Triangles.Num(); I += 3)
		{
			FTri T;
			for (int32 K = 0; K < 3; ++K) T.P[K] = D.Vertices[D.Triangles[I + K]];
			T.N = D.Normals[D.Triangles[I]]; T.Col = D.Colors[D.Triangles[I]];
			Out.Add(T);
		}
		return Out;
	}
	bool IsWall(const FTri& T) { return FMath::Abs(T.N.Z) < 0.01; }
	bool IsTop(const FTri& T) { return T.N.Z > 0.99; }
	/** 영지 안쪽 옆면 (바깥 테두리 판이 아님) */
	bool IsInnerWall(const FTri& T, const UCozyTileGrid* G)
	{
		if (!IsWall(T)) return false;
		const float CS = G->GetSettings().CellSize, MX = G->GetWidth() * CS, MY = G->GetHeight() * CS;
		for (const FVector& P : T.P)
			if (P.X < 1 || P.Y < 1 || P.X > MX - 1 || P.Y > MY - 1) return false;
		return true;
	}
	void ZRange(const FTri& T, float& Lo, float& Hi)
	{
		Lo = FMath::Min3(T.P[0].Z, T.P[1].Z, T.P[2].Z); Hi = FMath::Max3(T.P[0].Z, T.P[1].Z, T.P[2].Z);
	}
	/** 삼각형을 비교용 글자로 (순서 무관 비교) */
	FString Key(const FTri& T)
	{
		const FVector C = (T.P[0] + T.P[1] + T.P[2]) / 3;
		return FString::Printf(TEXT("%.1f,%.1f,%.1f|%.0f,%.0f,%.0f"), C.X, C.Y, C.Z, T.N.X, T.N.Y, T.N.Z);
	}
	/** UE 앞면 방향 = Cross(C-A, B-A) */
	FVector FrontNormal(const FTri& T) { return FVector::CrossProduct(T.P[2] - T.P[0], T.P[1] - T.P[0]).GetSafeNormal(); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyTileMeshFlatTest, "TerrainLab.Tiles.Mesh.FlatGridHasOnlyTopsAndSkirt", CozyTileMeshTest::Flags)
bool FCozyTileMeshFlatTest::RunTest(const FString& Parameters)
{
	UCozyTileGrid* G = CozyTileMeshTest::MakeGrid(3, 3);
	const TArray<CozyTileMeshTest::FTri> T = CozyTileMeshTest::Tris(CozyTileMeshTest::BuildAll(G));
	int32 Tops = 0, Inner = 0, Skirt = 0;
	for (const CozyTileMeshTest::FTri& X : T)
	{
		if (CozyTileMeshTest::IsTop(X)) { ++Tops; for (const FVector& P : X.P) TestEqual(TEXT("윗면 높이 0"), float(P.Z), 0.f); }
		else if (CozyTileMeshTest::IsInnerWall(X, G)) ++Inner;
		else if (CozyTileMeshTest::IsWall(X)) { ++Skirt; float Lo, Hi; CozyTileMeshTest::ZRange(X, Lo, Hi); TestTrue(TEXT("테두리 판은 0 아래로"), Lo < -G->GetSettings().WaterDepth); TestEqual(TEXT("테두리 판 위끝 0"), Hi, 0.f); }
	}
	TestEqual(TEXT("윗면 삼각형 9칸×2"), Tops, 18);
	TestEqual(TEXT("안쪽 옆면 없음"), Inner, 0);
	TestEqual(TEXT("테두리 판 12변×2"), Skirt, 24);
	TestEqual(TEXT("전체 삼각형"), T.Num(), 42);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyTileMeshSingleRaisedTest, "TerrainLab.Tiles.Mesh.SingleRaisedCellHasFourWalls", CozyTileMeshTest::Flags)
bool FCozyTileMeshSingleRaisedTest::RunTest(const FString& Parameters)
{
	UCozyTileGrid* G = CozyTileMeshTest::MakeGrid(3, 3);
	CozyTileMeshTest::SetLevel(G, 1, 1, 1);
	const float H = G->GetSettings().LevelHeights[1];
	int32 Inner = 0;
	for (const CozyTileMeshTest::FTri& X : CozyTileMeshTest::Tris(CozyTileMeshTest::BuildAll(G)))
	{
		if (!CozyTileMeshTest::IsInnerWall(X, G)) continue;
		++Inner;
		float Lo, Hi; CozyTileMeshTest::ZRange(X, Lo, Hi);
		TestEqual(TEXT("옆면 아래끝 0"), Lo, 0.f);
		TestEqual(TEXT("옆면 위끝 = 1층 높이"), Hi, H);
		// 법선은 가운데 칸 바깥(낮은 쪽)을 향한다
		const FVector Mid = (X.P[0] + X.P[1] + X.P[2]) / 3, Center(300, 300, Mid.Z);
		TestTrue(TEXT("옆면 법선이 바깥쪽"), FVector::DotProduct(X.N, Mid - Center) > 0);
	}
	TestEqual(TEXT("안쪽 옆면 4장(삼각형 8)"), Inner, 8);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyTileMeshBlockTest, "TerrainLab.Tiles.Mesh.RaisedBlockWallsOnlyOnOuterEdges", CozyTileMeshTest::Flags)
bool FCozyTileMeshBlockTest::RunTest(const FString& Parameters)
{
	UCozyTileGrid* G = CozyTileMeshTest::MakeGrid(4, 4);
	for (int32 Y = 1; Y <= 2; ++Y) for (int32 X = 1; X <= 2; ++X) CozyTileMeshTest::SetLevel(G, X, Y, 1);
	int32 Inner = 0;
	for (const CozyTileMeshTest::FTri& X : CozyTileMeshTest::Tris(CozyTileMeshTest::BuildAll(G)))
	{
		if (!CozyTileMeshTest::IsInnerWall(X, G)) continue;
		++Inner;
		// 2×2 덩어리 바깥 둘레(200 또는 600 선) 위에만 있어야 한다 · 덩어리 안 경계(400)에는 없음
		auto OnLine = [&](int32 Axis, double V) { for (const FVector& P : X.P) if (FMath::Abs(P[Axis] - V) > 0.1) return false; return true; };
		TestTrue(TEXT("옆면이 덩어리 둘레 위"), OnLine(0, 200) || OnLine(0, 600) || OnLine(1, 200) || OnLine(1, 600));
	}
	TestEqual(TEXT("바깥 변 8개(삼각형 16)"), Inner, 16);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyTileMeshEqualTest, "TerrainLab.Tiles.Mesh.EqualHeightNeighborsHaveNoWall", CozyTileMeshTest::Flags)
bool FCozyTileMeshEqualTest::RunTest(const FString& Parameters)
{
	UCozyTileGrid* G = CozyTileMeshTest::MakeGrid(3, 3);
	for (int32 Y = 0; Y < 3; ++Y) for (int32 X = 0; X < 3; ++X) CozyTileMeshTest::SetLevel(G, X, Y, 2, X == 1 ? ECozyTileGround::Stone : ECozyTileGround::Dirt);
	int32 Inner = 0;
	for (const CozyTileMeshTest::FTri& X : CozyTileMeshTest::Tris(CozyTileMeshTest::BuildAll(G))) if (CozyTileMeshTest::IsInnerWall(X, G)) ++Inner;
	TestEqual(TEXT("같은 높이(바닥 종류만 다름)는 옆면 없음"), Inner, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyTileMeshWaterTest, "TerrainLab.Tiles.Mesh.WaterCellFloorAndWalls", CozyTileMeshTest::Flags)
bool FCozyTileMeshWaterTest::RunTest(const FString& Parameters)
{
	UCozyTileGrid* G = CozyTileMeshTest::MakeGrid(3, 3);
	CozyTileMeshTest::SetLevel(G, 1, 1, 0, ECozyTileGround::Water);
	const FCozyTileSettings& S = G->GetSettings();
	int32 Inner = 0, Floor = 0;
	for (const CozyTileMeshTest::FTri& X : CozyTileMeshTest::Tris(CozyTileMeshTest::BuildAll(G)))
	{
		const FVector Mid = (X.P[0] + X.P[1] + X.P[2]) / 3;
		if (CozyTileMeshTest::IsTop(X) && Mid.X > 200 && Mid.X < 400 && Mid.Y > 200 && Mid.Y < 400)
		{
			++Floor; TestEqual(TEXT("물 바닥 = -물 깊이"), float(Mid.Z), -S.WaterDepth);
			TestTrue(TEXT("물 바닥 색"), X.Col == FColor(0x5E, 0x8F, 0xA8));
		}
		if (!CozyTileMeshTest::IsInnerWall(X, G)) continue;
		++Inner;
		float Lo, Hi; CozyTileMeshTest::ZRange(X, Lo, Hi);
		TestEqual(TEXT("물가 옆면 아래끝 = 물 바닥"), Lo, -S.WaterDepth);
		TestEqual(TEXT("물가 옆면 위끝 = 기본층"), Hi, 0.f);
		// 이웃(높은 칸)이 만든 옆면이므로 법선은 물 칸 쪽을 향한다
		TestTrue(TEXT("물가 옆면 법선이 물 쪽"), FVector::DotProduct(X.N, FVector(300, 300, Mid.Z) - Mid) > 0);
	}
	TestEqual(TEXT("물 바닥 삼각형 2"), Floor, 2);
	TestEqual(TEXT("물가 옆면 4장(삼각형 8)"), Inner, 8);

	FCozyTileMeshData W;
	FCozyTileMeshBuilder::BuildWater(*G, FIntPoint(0, 0), FIntPoint(2, 2), W);
	TestEqual(TEXT("물 표면 삼각형 2"), W.Triangles.Num(), 6);
	for (const FVector& V : W.Vertices) TestEqual(TEXT("물 표면 높이"), float(V.Z), S.LevelHeights[0] - S.WaterSurfaceOffset);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyTileMeshSeamTest, "TerrainLab.Tiles.Mesh.SplitRegionsMatchWhole", CozyTileMeshTest::Flags)
bool FCozyTileMeshSeamTest::RunTest(const FString& Parameters)
{
	UCozyTileGrid* G = CozyTileMeshTest::MakeGrid(6, 5);
	// 층·물이 섞인 모양 (구역 경계 x=2|3 을 가로지름)
	const uint8 L[5][6] = {{0,1,2,2,1,0},{1,2,2,1,0,0},{0,0,1,2,2,1},{2,1,0,0,1,2},{0,0,0,1,0,0}};
	for (int32 Y = 0; Y < 5; ++Y) for (int32 X = 0; X < 6; ++X) CozyTileMeshTest::SetLevel(G, X, Y, L[Y][X]);
	CozyTileMeshTest::SetLevel(G, 2, 3, 0, ECozyTileGround::Water); CozyTileMeshTest::SetLevel(G, 3, 3, 0, ECozyTileGround::Water); CozyTileMeshTest::SetLevel(G, 3, 4, 0, ECozyTileGround::Water);

	FCozyTileMeshData Whole, Split;
	FCozyTileMeshBuilder::BuildRegion(*G, FIntPoint(0, 0), FIntPoint(5, 4), Whole);
	FCozyTileMeshBuilder::BuildRegion(*G, FIntPoint(0, 0), FIntPoint(2, 4), Split);
	FCozyTileMeshBuilder::BuildRegion(*G, FIntPoint(3, 0), FIntPoint(5, 1), Split);
	FCozyTileMeshBuilder::BuildRegion(*G, FIntPoint(3, 2), FIntPoint(5, 4), Split);
	TArray<FString> A, B;
	for (const CozyTileMeshTest::FTri& X : CozyTileMeshTest::Tris(Whole)) A.Add(CozyTileMeshTest::Key(X));
	for (const CozyTileMeshTest::FTri& X : CozyTileMeshTest::Tris(Split)) B.Add(CozyTileMeshTest::Key(X));
	A.Sort(); B.Sort();
	TestEqual(TEXT("삼각형 수 같음"), A.Num(), B.Num());
	TestTrue(TEXT("나눠 그려도 같은 모양(이음매 없음)"), A == B);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyTileMeshWindingTest, "TerrainLab.Tiles.Mesh.WindingMatchesNormals", CozyTileMeshTest::Flags)
bool FCozyTileMeshWindingTest::RunTest(const FString& Parameters)
{
	UCozyTileGrid* G = CozyTileMeshTest::MakeGrid(5, 5);
	const uint8 L[5][5] = {{0,1,2,1,0},{1,2,0,2,1},{2,0,1,0,2},{1,2,0,2,1},{0,1,2,1,0}};
	for (int32 Y = 0; Y < 5; ++Y) for (int32 X = 0; X < 5; ++X) CozyTileMeshTest::SetLevel(G, X, Y, L[Y][X]);
	CozyTileMeshTest::SetLevel(G, 2, 1, 0, ECozyTileGround::Water);
	FCozyTileMeshData D = CozyTileMeshTest::BuildAll(G);
	FCozyTileMeshBuilder::BuildWater(*G, FIntPoint(0, 0), FIntPoint(4, 4), D);
	TestEqual(TEXT("배열 길이 맞음"), D.Normals.Num(), D.Vertices.Num());
	TestEqual(TEXT("UV 길이 맞음"), D.UVs.Num(), D.Vertices.Num());
	TestEqual(TEXT("색 길이 맞음"), D.Colors.Num(), D.Vertices.Num());
	int32 Bad = 0;
	for (const CozyTileMeshTest::FTri& X : CozyTileMeshTest::Tris(D))
		if (FVector::DotProduct(CozyTileMeshTest::FrontNormal(X), X.N) < 0.99) ++Bad;
	TestEqual(TEXT("모든 삼각형 앞면이 법선 쪽"), Bad, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyTileMeshLockedTest, "TerrainLab.Tiles.Mesh.LockedCellsAreDarker", CozyTileMeshTest::Flags)
bool FCozyTileMeshLockedTest::RunTest(const FString& Parameters)
{
	UCozyTileGrid* G = CozyTileMeshTest::MakeGrid(2, 1);
	FCozyTileCell C = G->GetCell(1, 0); C.bLocked = true; G->SetCellRaw(1, 0, C);
	FColor Open, Locked;
	for (const CozyTileMeshTest::FTri& X : CozyTileMeshTest::Tris(CozyTileMeshTest::BuildAll(G)))
		if (CozyTileMeshTest::IsTop(X)) ((X.P[0].X + X.P[1].X + X.P[2].X) / 3 < 200 ? Open : Locked) = X.Col;
	TestTrue(TEXT("열린 칸은 풀색"), Open == FColor(0x91, 0xB5, 0x7F));
	TestTrue(TEXT("잠긴 칸은 더 어둡다"), Locked.R < Open.R && Locked.G < Open.G && Locked.B < Open.B);
	return true;
}

#endif
