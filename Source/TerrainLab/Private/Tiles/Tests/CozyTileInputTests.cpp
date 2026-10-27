// 작업 3(조작) · 화면 글자 만들기 시험 (입력·화면 자체는 플레이로 확인)
#include "Tiles/CozyTileHudWidget.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyTileInputTextTest, "TerrainLab.Tiles.Input.HudText", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCozyTileInputTextTest::RunTest(const FString&)
{
	TestEqual(TEXT("도구 이름 · 올리기"), UCozyTileHudWidget::ToolName(ECozyTileTool::Raise).ToString(), FString(TEXT("층 올리기")));
	TestEqual(TEXT("도구 이름 · 물"), UCozyTileHudWidget::ToolName(ECozyTileTool::Water).ToString(), FString(TEXT("물 파기·메우기")));
	TestEqual(TEXT("층 이름 · 기본층"), UCozyTileHudWidget::LevelName(0).ToString(), FString(TEXT("기본층")));
	TestEqual(TEXT("층 이름 · 2층"), UCozyTileHudWidget::LevelName(2).ToString(), FString(TEXT("2층")));

	FCozyTileCell C; C.Level = 1;
	TestEqual(TEXT("칸 설명 · 풀"), UCozyTileHudWidget::HoverText(true, FIntPoint(3, 7), C).ToString(), FString(TEXT("칸 (3, 7) · 1층 · 풀")));
	TestEqual(TEXT("칸 설명 · 영지 밖"), UCozyTileHudWidget::HoverText(false, FIntPoint(-1, -1), C).ToString(), FString(TEXT("영지 밖")));
	C.bOccupied = true;
	TestEqual(TEXT("칸 설명 · 물건 있음"), UCozyTileHudWidget::HoverText(true, FIntPoint(3, 7), C).ToString(), FString(TEXT("칸 (3, 7) · 1층 · 풀 · 물건 있음")));
	FCozyTileCell W; W.Ground = ECozyTileGround::Water;
	TestEqual(TEXT("칸 설명 · 물"), UCozyTileHudWidget::HoverText(true, FIntPoint(0, 0), W).ToString(), FString(TEXT("칸 (0, 0) · 물")));
	FCozyTileCell L; L.bLocked = true; L.Ground = ECozyTileGround::Water;
	TestEqual(TEXT("칸 설명 · 잠김이 먼저"), UCozyTileHudWidget::HoverText(true, FIntPoint(1, 2), L).ToString(), FString(TEXT("칸 (1, 2) · 잠김")));

	TestEqual(TEXT("성공 문장 · 올리기"), UCozyTileHudWidget::OkText(ECozyTileTool::Raise, C).ToString(), FString(TEXT("1층으로 올렸습니다")));
	TestEqual(TEXT("성공 문장 · 기본층으로 내리기"), UCozyTileHudWidget::OkText(ECozyTileTool::Lower, FCozyTileCell()).ToString(), FString(TEXT("기본층으로 내렸습니다")));
	TestEqual(TEXT("성공 문장 · 물 파기"), UCozyTileHudWidget::OkText(ECozyTileTool::Water, W).ToString(), FString(TEXT("물을 팠습니다")));
	TestEqual(TEXT("성공 문장 · 물 메우기"), UCozyTileHudWidget::OkText(ECozyTileTool::Water, FCozyTileCell()).ToString(), FString(TEXT("물을 메웠습니다")));
	return true;
}
#endif
