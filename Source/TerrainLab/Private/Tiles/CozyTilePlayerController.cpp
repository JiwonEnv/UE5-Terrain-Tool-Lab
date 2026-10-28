#include "Tiles/CozyTilePlayerController.h"
#include "Tiles/CozyTileTerrain.h"
#include "Tiles/CozyTileGrid.h"
#include "Tiles/CozyTileHudWidget.h"
#include "Blueprint/UserWidget.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "InputCoreTypes.h"

#define LOCTEXT_NAMESPACE "CozyTileController"

// 알림 색 (경고 = 주황 · 성공 = 연두)
static const FLinearColor CozyTileWarningColor(1.f, 0.55f, 0.25f);
static const FLinearColor CozyTileOkColor(0.6f, 1.f, 0.6f);

ACozyTilePlayerController::ACozyTilePlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
}

void ACozyTilePlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController()) return;
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);

	for (TActorIterator<ACozyTileTerrain> It(GetWorld()); It; ++It) { Terrain = *It; break; }
	Hud = CreateWidget<UCozyTileHudWidget>(this, UCozyTileHudWidget::StaticClass());
	if (Hud)
	{
		Hud->AddToViewport();
		Hud->SetTool(CurrentTool);
		if (!Terrain) Hud->ShowMessage(LOCTEXT("NoTerrain", "레벨에 칸 땅(CozyTileTerrain)이 없습니다"), CozyTileWarningColor, 10.f);
	}
}

void ACozyTilePlayerController::PlayerTick(float DeltaTime)
{
	// Super가 이번 프레임 입력을 처리한 뒤라 '방금 눌림' 값이 이번 프레임 기준이다
	Super::PlayerTick(DeltaTime);
	if (!Terrain || !Terrain->GetGrid()) return;

	// 커서 아래 칸 · 절벽 옆면을 맞혔을 때도 그 칸이 잡히게 면 안쪽으로 조금 민다
	FHitResult Hit;
	bHasHover = false;
	if (GetHitResultUnderCursor(ECC_Visibility, false, Hit) && Hit.bBlockingHit)
		bHasHover = Terrain->WorldToCell(Hit.ImpactPoint - Hit.ImpactNormal * 5.0, HoverCell);
	UCozyTileGrid* Grid = Terrain->GetGrid();
	if (Hud) Hud->SetHoverText(UCozyTileHudWidget::HoverText(bHasHover, HoverCell, Grid->GetCell(HoverCell.X, HoverCell.Y)));

	// 도구 고르기
	const ECozyTileTool Before = CurrentTool;
	if (WasInputKeyJustPressed(EKeys::One)) CurrentTool = ECozyTileTool::Raise;
	if (WasInputKeyJustPressed(EKeys::Two)) CurrentTool = ECozyTileTool::Lower;
	if (WasInputKeyJustPressed(EKeys::Three)) CurrentTool = ECozyTileTool::Water;
	if (Hud && Before != CurrentTool) Hud->SetTool(CurrentTool);

	// 사용 · 되돌리기 · 처음으로
	if (WasInputKeyJustPressed(EKeys::LeftMouseButton)) UseTool(CurrentTool);
	if (WasInputKeyJustPressed(EKeys::RightMouseButton)) UseTool(ECozyTileTool::Lower);
	const bool bCtrl = IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl);
	if (bCtrl && WasInputKeyJustPressed(EKeys::Z))
	{
		const bool bUndone = Terrain->UndoLast();
		if (Hud) Hud->ShowMessage(bUndone ? LOCTEXT("Undo", "되돌렸습니다") : LOCTEXT("NoUndo", "되돌릴 것이 없습니다"), bUndone ? CozyTileOkColor : CozyTileWarningColor, OkSeconds);
	}
	if (WasInputKeyJustPressed(EKeys::F9))
	{
		Terrain->ResetToDefault();
		if (Hud) Hud->ShowMessage(LOCTEXT("Reset", "처음 상태로 되돌렸습니다"), CozyTileOkColor, WarningSeconds);
	}
	DrawHover();
}

void ACozyTilePlayerController::UseTool(ECozyTileTool Tool)
{
	if (!bHasHover) return;
	const ECozyTileEditResult Result = Terrain->ApplyTool(Tool, HoverCell);
	if (!Hud) return;
	if (Result == ECozyTileEditResult::Ok)
		Hud->ShowMessage(UCozyTileHudWidget::OkText(Tool, Terrain->GetGrid()->GetCell(HoverCell.X, HoverCell.Y)), CozyTileOkColor, OkSeconds);
	else
		Hud->ShowMessage(CozyTileResultText(Result), CozyTileWarningColor, WarningSeconds);
}

void ACozyTilePlayerController::DrawHover() const
{
	if (!bHasHover) return;
	const UCozyTileGrid* Grid = Terrain->GetGrid();
	const bool bOk = Grid->CanApply(CurrentTool, HoverCell.X, HoverCell.Y) == ECozyTileEditResult::Ok;
	// 칸 윗면보다 조금 위에 얇은 상자 테두리 (매 프레임 다시 그림)
	const float Half = Grid->GetSettings().CellSize * 0.48f;
	const FVector Center = Terrain->CellToWorld(HoverCell) + FVector(0.0, 0.0, 8.0);
	DrawDebugBox(GetWorld(), Center, FVector(Half, Half, 4.0), Terrain->GetActorQuat(), bOk ? FColor(80, 255, 120) : FColor(255, 70, 60), false, 0.f, 0, 6.f);
}

#undef LOCTEXT_NAMESPACE
