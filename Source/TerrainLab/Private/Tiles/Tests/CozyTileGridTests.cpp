// 작업 1 · 영지 칸 데이터·규칙 자동 시험 (세션 프론트엔드 > 자동화 > TerrainLab.Tiles.Grid)
#include "Misc/AutomationTest.h"
#include "Tiles/CozyTileGrid.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

#if WITH_DEV_AUTOMATION_TESTS

#define COZY_TILE_GRID_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace CozyTileGridTestUtil
{
	// 기본 설정(층 3개) 4x3 영지
	UCozyTileGrid* MakeGrid(int32 W = 4, int32 H = 3)
	{
		UCozyTileGrid* Grid = NewObject<UCozyTileGrid>();
		Grid->Init(W, H, FCozyTileSettings());
		return Grid;
	}

	int32 R(ECozyTileEditResult Result) { return int32(Result); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyTileGridInitTest, "TerrainLab.Tiles.Grid.InitDefaults", COZY_TILE_GRID_TEST_FLAGS)
bool FCozyTileGridInitTest::RunTest(const FString& Parameters)
{
	UCozyTileGrid* Grid = CozyTileGridTestUtil::MakeGrid(5, 4);
	TestEqual(TEXT("가로"), Grid->GetWidth(), 5);
	TestEqual(TEXT("세로"), Grid->GetHeight(), 4);
	TestEqual(TEXT("최고층"), int32(Grid->GetSettings().MaxLevel()), 2);
	TestEqual(TEXT("되돌리기 비어 있음"), Grid->GetUndoCount(), 0);
	for (int32 Y = 0; Y < 4; ++Y)
		for (int32 X = 0; X < 5; ++X)
			TestTrue(FString::Printf(TEXT("칸 %d,%d 기본값"), X, Y), Grid->GetCell(X, Y) == FCozyTileCell());
	TestTrue(TEXT("밖 칸은 기본값"), Grid->GetCell(-1, 99) == FCozyTileCell());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyTileGridRaiseTest, "TerrainLab.Tiles.Grid.RaiseToMax", COZY_TILE_GRID_TEST_FLAGS)
bool FCozyTileGridRaiseTest::RunTest(const FString& Parameters)
{
	UCozyTileGrid* Grid = CozyTileGridTestUtil::MakeGrid();
	TestEqual(TEXT("1층"), CozyTileGridTestUtil::R(Grid->Apply(ECozyTileTool::Raise, 1, 1)), CozyTileGridTestUtil::R(ECozyTileEditResult::Ok));
	TestEqual(TEXT("2층"), CozyTileGridTestUtil::R(Grid->Apply(ECozyTileTool::Raise, 1, 1)), CozyTileGridTestUtil::R(ECozyTileEditResult::Ok));
	TestEqual(TEXT("층 2"), int32(Grid->GetCell(1, 1).Level), 2);
	TestEqual(TEXT("최고층에서 막힘"), CozyTileGridTestUtil::R(Grid->Apply(ECozyTileTool::Raise, 1, 1)), CozyTileGridTestUtil::R(ECozyTileEditResult::AtMaxLevel));
	TestEqual(TEXT("CanApply도 같음"), CozyTileGridTestUtil::R(Grid->CanApply(ECozyTileTool::Raise, 1, 1)), CozyTileGridTestUtil::R(ECozyTileEditResult::AtMaxLevel));
	TestEqual(TEXT("이웃은 그대로"), int32(Grid->GetCell(0, 1).Level), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyTileGridLowerTest, "TerrainLab.Tiles.Grid.LowerToMin", COZY_TILE_GRID_TEST_FLAGS)
bool FCozyTileGridLowerTest::RunTest(const FString& Parameters)
{
	UCozyTileGrid* Grid = CozyTileGridTestUtil::MakeGrid();
	Grid->Apply(ECozyTileTool::Raise, 2, 0);
	Grid->Apply(ECozyTileTool::Raise, 2, 0);
	TestEqual(TEXT("내리기 1"), CozyTileGridTestUtil::R(Grid->Apply(ECozyTileTool::Lower, 2, 0)), CozyTileGridTestUtil::R(ECozyTileEditResult::Ok));
	TestEqual(TEXT("내리기 2"), CozyTileGridTestUtil::R(Grid->Apply(ECozyTileTool::Lower, 2, 0)), CozyTileGridTestUtil::R(ECozyTileEditResult::Ok));
	TestEqual(TEXT("층 0"), int32(Grid->GetCell(2, 0).Level), 0);
	TestEqual(TEXT("기본층에서 막힘"), CozyTileGridTestUtil::R(Grid->Apply(ECozyTileTool::Lower, 2, 0)), CozyTileGridTestUtil::R(ECozyTileEditResult::AtMinLevel));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyTileGridWaterToggleTest, "TerrainLab.Tiles.Grid.WaterToggle", COZY_TILE_GRID_TEST_FLAGS)
bool FCozyTileGridWaterToggleTest::RunTest(const FString& Parameters)
{
	UCozyTileGrid* Grid = CozyTileGridTestUtil::MakeGrid();
	TestEqual(TEXT("물 파기"), CozyTileGridTestUtil::R(Grid->Apply(ECozyTileTool::Water, 0, 0)), CozyTileGridTestUtil::R(ECozyTileEditResult::Ok));
	TestTrue(TEXT("물 칸"), Grid->GetCell(0, 0).IsWater());
	TestEqual(TEXT("물 칸 층 0"), int32(Grid->GetCell(0, 0).Level), 0);
	TestEqual(TEXT("물 메우기"), CozyTileGridTestUtil::R(Grid->Apply(ECozyTileTool::Water, 0, 0)), CozyTileGridTestUtil::R(ECozyTileEditResult::Ok));
	TestEqual(TEXT("풀로 돌아옴"), int32(Grid->GetCell(0, 0).Ground), int32(ECozyTileGround::Grass));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyTileGridWaterNeedsBaseTest, "TerrainLab.Tiles.Grid.WaterNeedsBaseLevel", COZY_TILE_GRID_TEST_FLAGS)
bool FCozyTileGridWaterNeedsBaseTest::RunTest(const FString& Parameters)
{
	UCozyTileGrid* Grid = CozyTileGridTestUtil::MakeGrid();
	Grid->Apply(ECozyTileTool::Raise, 1, 0);
	TestEqual(TEXT("1층에서는 물 못 팜"), CozyTileGridTestUtil::R(Grid->Apply(ECozyTileTool::Water, 1, 0)), CozyTileGridTestUtil::R(ECozyTileEditResult::WaterNeedsBaseLevel));
	TestFalse(TEXT("물 아님"), Grid->GetCell(1, 0).IsWater());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyTileGridIsWaterTest, "TerrainLab.Tiles.Grid.RaiseLowerOnWater", COZY_TILE_GRID_TEST_FLAGS)
bool FCozyTileGridIsWaterTest::RunTest(const FString& Parameters)
{
	UCozyTileGrid* Grid = CozyTileGridTestUtil::MakeGrid();
	Grid->Apply(ECozyTileTool::Water, 3, 2);
	TestEqual(TEXT("물 칸 올리기"), CozyTileGridTestUtil::R(Grid->Apply(ECozyTileTool::Raise, 3, 2)), CozyTileGridTestUtil::R(ECozyTileEditResult::IsWater));
	TestEqual(TEXT("물 칸 내리기"), CozyTileGridTestUtil::R(Grid->Apply(ECozyTileTool::Lower, 3, 2)), CozyTileGridTestUtil::R(ECozyTileEditResult::IsWater));
	TestTrue(TEXT("여전히 물"), Grid->GetCell(3, 2).IsWater());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyTileGridLockedTest, "TerrainLab.Tiles.Grid.Locked", COZY_TILE_GRID_TEST_FLAGS)
bool FCozyTileGridLockedTest::RunTest(const FString& Parameters)
{
	UCozyTileGrid* Grid = CozyTileGridTestUtil::MakeGrid();
	Grid->SetLockedRect(FIntPoint(2, 0), FIntPoint(10, 10), true); // 밖 범위는 잘라냄
	TestTrue(TEXT("잠김"), Grid->GetCell(3, 2).bLocked);
	TestFalse(TEXT("범위 밖은 안 잠김"), Grid->GetCell(1, 0).bLocked);
	TestEqual(TEXT("되돌리기 기록 없음"), Grid->GetUndoCount(), 0);
	TestEqual(TEXT("올리기"), CozyTileGridTestUtil::R(Grid->Apply(ECozyTileTool::Raise, 2, 1)), CozyTileGridTestUtil::R(ECozyTileEditResult::Locked));
	TestEqual(TEXT("내리기"), CozyTileGridTestUtil::R(Grid->Apply(ECozyTileTool::Lower, 2, 1)), CozyTileGridTestUtil::R(ECozyTileEditResult::Locked));
	TestEqual(TEXT("물"), CozyTileGridTestUtil::R(Grid->Apply(ECozyTileTool::Water, 2, 1)), CozyTileGridTestUtil::R(ECozyTileEditResult::Locked));
	Grid->SetLockedRect(FIntPoint(2, 1), FIntPoint(2, 1), false);
	TestEqual(TEXT("풀면 다시 됨"), CozyTileGridTestUtil::R(Grid->Apply(ECozyTileTool::Raise, 2, 1)), CozyTileGridTestUtil::R(ECozyTileEditResult::Ok));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyTileGridOccupiedTest, "TerrainLab.Tiles.Grid.Occupied", COZY_TILE_GRID_TEST_FLAGS)
bool FCozyTileGridOccupiedTest::RunTest(const FString& Parameters)
{
	UCozyTileGrid* Grid = CozyTileGridTestUtil::MakeGrid();
	Grid->SetOccupiedRect(FIntPoint(1, 1), FIntPoint(0, 0), true); // Min·Max 순서가 바뀌어도 됨
	TestTrue(TEXT("놓임"), Grid->GetCell(0, 0).bOccupied && Grid->GetCell(1, 1).bOccupied);
	TestEqual(TEXT("올리기"), CozyTileGridTestUtil::R(Grid->Apply(ECozyTileTool::Raise, 1, 0)), CozyTileGridTestUtil::R(ECozyTileEditResult::Occupied));
	TestEqual(TEXT("물"), CozyTileGridTestUtil::R(Grid->Apply(ECozyTileTool::Water, 0, 1)), CozyTileGridTestUtil::R(ECozyTileEditResult::Occupied));
	// 잠김이 놓인 물건보다 먼저 검사됨
	Grid->SetLockedRect(FIntPoint(0, 0), FIntPoint(0, 0), true);
	TestEqual(TEXT("잠김 먼저"), CozyTileGridTestUtil::R(Grid->Apply(ECozyTileTool::Raise, 0, 0)), CozyTileGridTestUtil::R(ECozyTileEditResult::Locked));
	Grid->ClearOccupied();
	TestFalse(TEXT("모두 지움"), Grid->GetCell(1, 1).bOccupied);
	TestEqual(TEXT("지운 뒤 됨"), CozyTileGridTestUtil::R(Grid->Apply(ECozyTileTool::Raise, 1, 1)), CozyTileGridTestUtil::R(ECozyTileEditResult::Ok));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyTileGridOutOfBoundsTest, "TerrainLab.Tiles.Grid.OutOfBounds", COZY_TILE_GRID_TEST_FLAGS)
bool FCozyTileGridOutOfBoundsTest::RunTest(const FString& Parameters)
{
	UCozyTileGrid* Grid = CozyTileGridTestUtil::MakeGrid();
	TestEqual(TEXT("왼쪽 밖"), CozyTileGridTestUtil::R(Grid->Apply(ECozyTileTool::Raise, -1, 0)), CozyTileGridTestUtil::R(ECozyTileEditResult::OutOfBounds));
	TestEqual(TEXT("오른쪽 밖"), CozyTileGridTestUtil::R(Grid->Apply(ECozyTileTool::Lower, 4, 0)), CozyTileGridTestUtil::R(ECozyTileEditResult::OutOfBounds));
	TestEqual(TEXT("아래 밖"), CozyTileGridTestUtil::R(Grid->Apply(ECozyTileTool::Water, 0, 3)), CozyTileGridTestUtil::R(ECozyTileEditResult::OutOfBounds));
	TestEqual(TEXT("되돌리기 기록 없음"), Grid->GetUndoCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyTileGridFailNoChangeTest, "TerrainLab.Tiles.Grid.FailedApplyNoChange", COZY_TILE_GRID_TEST_FLAGS)
bool FCozyTileGridFailNoChangeTest::RunTest(const FString& Parameters)
{
	UCozyTileGrid* Grid = CozyTileGridTestUtil::MakeGrid();
	Grid->Apply(ECozyTileTool::Raise, 0, 0);
	const FCozyTileCell Before = Grid->GetCell(0, 0);
	const int32 UndoBefore = Grid->GetUndoCount();
	int32 Fired = 0;
	Grid->OnCellsChanged.AddLambda([&Fired](const TArray<FIntPoint>&) { ++Fired; });
	TestEqual(TEXT("물 실패"), CozyTileGridTestUtil::R(Grid->Apply(ECozyTileTool::Water, 0, 0)), CozyTileGridTestUtil::R(ECozyTileEditResult::WaterNeedsBaseLevel));
	TestTrue(TEXT("칸 그대로"), Grid->GetCell(0, 0) == Before);
	TestEqual(TEXT("되돌리기 수 그대로"), Grid->GetUndoCount(), UndoBefore);
	TestEqual(TEXT("알림 없음"), Fired, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyTileGridUndoTest, "TerrainLab.Tiles.Grid.Undo", COZY_TILE_GRID_TEST_FLAGS)
bool FCozyTileGridUndoTest::RunTest(const FString& Parameters)
{
	UCozyTileGrid* Grid = CozyTileGridTestUtil::MakeGrid();
	TestFalse(TEXT("비었을 때 false"), Grid->Undo());
	Grid->Apply(ECozyTileTool::Raise, 1, 2);
	Grid->Apply(ECozyTileTool::Water, 0, 0);
	TestEqual(TEXT("기록 2개"), Grid->GetUndoCount(), 2);
	TestTrue(TEXT("물 되돌리기"), Grid->Undo());
	TestFalse(TEXT("물 아님"), Grid->GetCell(0, 0).IsWater());
	TestTrue(TEXT("올리기 되돌리기"), Grid->Undo());
	TestEqual(TEXT("층 0"), int32(Grid->GetCell(1, 2).Level), 0);
	TestFalse(TEXT("다 되돌린 뒤 false"), Grid->Undo());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyTileGridUndoCapTest, "TerrainLab.Tiles.Grid.UndoCap", COZY_TILE_GRID_TEST_FLAGS)
bool FCozyTileGridUndoCapTest::RunTest(const FString& Parameters)
{
	UCozyTileGrid* Grid = CozyTileGridTestUtil::MakeGrid();
	for (int32 i = 0; i < 250; ++i) Grid->Apply(ECozyTileTool::Water, 0, 0);
	TestEqual(TEXT("최대 200개"), Grid->GetUndoCount(), 200);
	int32 Undone = 0;
	while (Grid->Undo()) ++Undone;
	TestEqual(TEXT("200번 되돌림"), Undone, 200);
	// 250번 중 오래된 50번은 버려졌으므로 50번 바꾼 상태(짝수 → 풀)로 남는다
	TestFalse(TEXT("풀 상태"), Grid->GetCell(0, 0).IsWater());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyTileGridChangedEventTest, "TerrainLab.Tiles.Grid.OnCellsChanged", COZY_TILE_GRID_TEST_FLAGS)
bool FCozyTileGridChangedEventTest::RunTest(const FString& Parameters)
{
	UCozyTileGrid* Grid = CozyTileGridTestUtil::MakeGrid();
	TArray<TArray<FIntPoint>> Calls;
	Grid->OnCellsChanged.AddLambda([&Calls](const TArray<FIntPoint>& Changed) { Calls.Add(Changed); });

	Grid->Apply(ECozyTileTool::Raise, 2, 1);
	TestEqual(TEXT("Apply 알림 1번"), Calls.Num(), 1);
	if (Calls.Num() == 1) TestTrue(TEXT("Apply 칸"), Calls[0].Num() == 1 && Calls[0][0] == FIntPoint(2, 1));

	Grid->Undo();
	TestEqual(TEXT("Undo 알림"), Calls.Num(), 2);
	if (Calls.Num() == 2) TestTrue(TEXT("Undo 칸"), Calls[1].Num() == 1 && Calls[1][0] == FIntPoint(2, 1));

	Grid->SetLockedRect(FIntPoint(0, 0), FIntPoint(1, 1), true);
	TestEqual(TEXT("잠금 알림 한 번"), Calls.Num(), 3);
	if (Calls.Num() == 3) TestEqual(TEXT("잠금 칸 4개"), Calls[2].Num(), 4);

	FCozyTileCell Raw; Raw.Level = 2;
	Grid->SetCellRaw(3, 2, Raw);
	TestEqual(TEXT("Raw 알림"), Calls.Num(), 4);
	TestEqual(TEXT("Raw는 되돌리기 없음"), Grid->GetUndoCount(), 0);
	TestEqual(TEXT("Raw 층"), int32(Grid->GetCell(3, 2).Level), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyTileGridJsonTest, "TerrainLab.Tiles.Grid.JsonRoundtrip", COZY_TILE_GRID_TEST_FLAGS)
bool FCozyTileGridJsonTest::RunTest(const FString& Parameters)
{
	UCozyTileGrid* Src = CozyTileGridTestUtil::MakeGrid();
	Src->Apply(ECozyTileTool::Raise, 1, 1);
	Src->Apply(ECozyTileTool::Raise, 1, 1);
	Src->Apply(ECozyTileTool::Water, 3, 0);
	FCozyTileCell Stone; Stone.Ground = ECozyTileGround::Stone; Stone.Level = 1;
	Src->SetCellRaw(0, 2, Stone);
	Src->SetLockedRect(FIntPoint(3, 2), FIntPoint(3, 2), true);
	Src->SetOccupiedRect(FIntPoint(2, 2), FIntPoint(2, 2), true);
	const FString Json = Src->ToJson();

	UCozyTileGrid* Dst = CozyTileGridTestUtil::MakeGrid();
	Dst->Apply(ECozyTileTool::Raise, 0, 0);
	Dst->SetOccupiedRect(FIntPoint(0, 1), FIntPoint(0, 1), true);
	int32 Fired = 0;
	Dst->OnCellsChanged.AddLambda([&Fired](const TArray<FIntPoint>& Changed) { Fired += Changed.Num(); });
	TestTrue(TEXT("불러오기 성공"), Dst->FromJson(Json));
	TestEqual(TEXT("모든 칸 알림"), Fired, 12);
	TestEqual(TEXT("되돌리기 비움"), Dst->GetUndoCount(), 0);
	TestEqual(TEXT("층"), int32(Dst->GetCell(1, 1).Level), 2);
	TestTrue(TEXT("물"), Dst->GetCell(3, 0).IsWater());
	TestEqual(TEXT("돌길"), int32(Dst->GetCell(0, 2).Ground), int32(ECozyTileGround::Stone));
	TestEqual(TEXT("돌길 층"), int32(Dst->GetCell(0, 2).Level), 1);
	TestTrue(TEXT("잠금"), Dst->GetCell(3, 2).bLocked);
	TestEqual(TEXT("덮어쓴 칸"), int32(Dst->GetCell(0, 0).Level), 0);
	TestFalse(TEXT("놓인 물건은 저장 안 됨"), Dst->GetCell(2, 2).bOccupied);
	TestTrue(TEXT("지금 놓인 물건은 지킴"), Dst->GetCell(0, 1).bOccupied);
	TestFalse(TEXT("깨진 JSON 실패"), Dst->FromJson(TEXT("{ not json")));
	TestEqual(TEXT("실패 뒤 그대로"), int32(Dst->GetCell(1, 1).Level), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyTileGridJsonSizeTest, "TerrainLab.Tiles.Grid.JsonSizeMismatch", COZY_TILE_GRID_TEST_FLAGS)
bool FCozyTileGridJsonSizeTest::RunTest(const FString& Parameters)
{
	UCozyTileGrid* Small = CozyTileGridTestUtil::MakeGrid(2, 2);
	const FString Json = Small->ToJson();
	UCozyTileGrid* Grid = CozyTileGridTestUtil::MakeGrid();
	Grid->Apply(ECozyTileTool::Raise, 1, 1);
	TestFalse(TEXT("크기 다르면 실패"), Grid->FromJson(Json));
	TestEqual(TEXT("데이터 그대로"), int32(Grid->GetCell(1, 1).Level), 1);
	TestEqual(TEXT("되돌리기 그대로"), Grid->GetUndoCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyTileGridFileTest, "TerrainLab.Tiles.Grid.FileRoundtrip", COZY_TILE_GRID_TEST_FLAGS)
bool FCozyTileGridFileTest::RunTest(const FString& Parameters)
{
	const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CozyTilesTest"));
	const FString Path = FPaths::Combine(Dir, TEXT("Sub"), TEXT("Grid.json"));
	IFileManager::Get().DeleteDirectory(*Dir, false, true);

	UCozyTileGrid* Grid = CozyTileGridTestUtil::MakeGrid();
	TestFalse(TEXT("없는 파일은 실패"), Grid->LoadFromFile(Path));
	Grid->Apply(ECozyTileTool::Raise, 2, 2);
	Grid->SetLockedRect(FIntPoint(0, 0), FIntPoint(0, 0), true);
	TestTrue(TEXT("저장 (폴더 만들기 포함)"), Grid->SaveToFile(Path));
	TestTrue(TEXT("파일 생김"), IFileManager::Get().FileExists(*Path));

	UCozyTileGrid* Loaded = CozyTileGridTestUtil::MakeGrid();
	TestTrue(TEXT("불러오기"), Loaded->LoadFromFile(Path));
	TestEqual(TEXT("층"), int32(Loaded->GetCell(2, 2).Level), 1);
	TestTrue(TEXT("잠금"), Loaded->GetCell(0, 0).bLocked);

	IFileManager::Get().DeleteDirectory(*Dir, false, true);
	TestFalse(TEXT("정리됨"), IFileManager::Get().FileExists(*Path));
	return true;
}

#undef COZY_TILE_GRID_TEST_FLAGS

#endif // WITH_DEV_AUTOMATION_TESTS
