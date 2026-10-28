#include "Tiles/CozyTileHudWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "CozyTileHud"

FText UCozyTileHudWidget::ToolName(ECozyTileTool Tool)
{
	switch (Tool)
	{
	case ECozyTileTool::Raise: return LOCTEXT("ToolRaise", "층 올리기");
	case ECozyTileTool::Lower: return LOCTEXT("ToolLower", "층 내리기");
	case ECozyTileTool::Water: return LOCTEXT("ToolWater", "물 파기·메우기");
	}
	return FText::GetEmpty();
}

FText UCozyTileHudWidget::LevelName(uint8 Level)
{
	return Level == 0 ? LOCTEXT("LevelBase", "기본층") : FText::Format(LOCTEXT("LevelN", "{0}층"), FText::AsNumber(Level));
}

FText UCozyTileHudWidget::HoverText(bool bInside, FIntPoint Coord, const FCozyTileCell& Cell)
{
	if (!bInside) return LOCTEXT("HoverOutside", "영지 밖");
	const FText Head = FText::Format(LOCTEXT("HoverHead", "칸 ({0}, {1})"), FText::AsNumber(Coord.X), FText::AsNumber(Coord.Y));
	if (Cell.bLocked) return FText::Format(LOCTEXT("HoverLocked", "{0} · 잠김"), Head);
	if (Cell.IsWater()) return FText::Format(LOCTEXT("HoverWater", "{0} · 물"), Head);
	FText Ground;
	switch (Cell.Ground)
	{
	case ECozyTileGround::Dirt: Ground = LOCTEXT("GroundDirt", "흙길"); break;
	case ECozyTileGround::Stone: Ground = LOCTEXT("GroundStone", "돌길"); break;
	default: Ground = LOCTEXT("GroundGrass", "풀"); break;
	}
	const FText Base = FText::Format(LOCTEXT("HoverCell", "{0} · {1} · {2}"), Head, LevelName(Cell.Level), Ground);
	return Cell.bOccupied ? FText::Format(LOCTEXT("HoverOccupied", "{0} · 물건 있음"), Base) : Base;
}

FText UCozyTileHudWidget::OkText(ECozyTileTool Tool, const FCozyTileCell& After)
{
	switch (Tool)
	{
	case ECozyTileTool::Raise: return FText::Format(LOCTEXT("OkRaise", "{0}으로 올렸습니다"), LevelName(After.Level));
	case ECozyTileTool::Lower: return FText::Format(LOCTEXT("OkLower", "{0}으로 내렸습니다"), LevelName(After.Level));
	case ECozyTileTool::Water: return After.IsWater() ? LOCTEXT("OkDig", "물을 팠습니다") : LOCTEXT("OkFill", "물을 메웠습니다");
	}
	return FText::GetEmpty();
}

UTextBlock* UCozyTileHudWidget::MakeText(float Size, const FText& Text)
{
	UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	// 기본 Slate 글꼴(Roboto 묶음)은 한글 대체 글꼴이 있어 글꼴 애셋 없이 한글이 나온다
	T->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", Size));
	T->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	T->SetShadowOffset(FVector2D(1.5, 1.5));
	T->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.85f));
	T->SetText(Text);
	return T;
}

void UCozyTileHudWidget::EnsureTree()
{
	if (!WidgetTree) Initialize();
	if (!WidgetTree || WidgetTree->RootWidget) return;
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	// 글자가 클릭을 가로채지 않게 (땅 클릭이 그대로 게임으로 간다)
	Canvas->SetVisibility(ESlateVisibility::HitTestInvisible);
	WidgetTree->RootWidget = Canvas;
	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UCanvasPanelSlot* BoxSlot = Canvas->AddChildToCanvas(Box);
	BoxSlot->SetAutoSize(true);
	BoxSlot->SetPosition(FVector2D(24.0, 24.0));

	ToolLine = MakeText(22.f, FText::GetEmpty());
	HelpLine = MakeText(16.f, LOCTEXT("Help", "[1] 층 올리기  [2] 층 내리기  [3] 물  ·  왼쪽 클릭 사용 · 오른쪽 클릭 내리기 · Ctrl+Z 되돌리기 · F9 처음으로\n휠 확대 · 가운데 버튼 끌기 회전 · Q/E 45도 회전 · WASD 이동"));
	HoverLine = MakeText(20.f, FText::GetEmpty());
	MessageLine = MakeText(20.f, FText::GetEmpty());
	for (UTextBlock* T : {ToolLine.Get(), HelpLine.Get(), HoverLine.Get(), MessageLine.Get()})
		Box->AddChildToVerticalBox(T)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	SetTool(ECozyTileTool::Raise);
}

TSharedRef<SWidget> UCozyTileHudWidget::RebuildWidget()
{
	// 위젯 블루프린트가 없으므로 WidgetTree를 직접 채운다 (부모가 RootWidget을 Slate로 바꾼다)
	EnsureTree();
	return Super::RebuildWidget();
}

void UCozyTileHudWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (MessageTimeLeft > 0.f && (MessageTimeLeft -= InDeltaTime) <= 0.f && MessageLine) MessageLine->SetText(FText::GetEmpty());
}

void UCozyTileHudWidget::SetTool(ECozyTileTool Tool)
{
	EnsureTree();
	if (ToolLine) ToolLine->SetText(FText::Format(LOCTEXT("ToolLine", "도구: {0}"), ToolName(Tool)));
}

void UCozyTileHudWidget::SetHoverText(const FText& Text)
{
	EnsureTree();
	if (HoverLine) HoverLine->SetText(Text);
}

void UCozyTileHudWidget::ShowMessage(const FText& Text, const FLinearColor& Color, float Seconds)
{
	EnsureTree();
	if (!MessageLine) return;
	MessageLine->SetText(Text);
	MessageLine->SetColorAndOpacity(FSlateColor(Color));
	MessageTimeLeft = Seconds;
}

#undef LOCTEXT_NAMESPACE
