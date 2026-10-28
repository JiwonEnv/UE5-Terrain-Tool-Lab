#pragma once
// 작업 3(조작) · 화면 왼쪽 위 안내 글자 (위젯 블루프린트 없이 C++로만 만든다)
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Tiles/CozyTileTypes.h"
#include "CozyTileHudWidget.generated.h"

class UTextBlock;

/** 현재 도구 · 커서 아래 칸 · 최근 알림을 보여 준다 · 컨트롤러가 값을 넣어 준다 */
UCLASS()
class TERRAINLAB_API UCozyTileHudWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetTool(ECozyTileTool Tool);
	void SetHoverText(const FText& Text);
	/** 알림을 Seconds초 동안 보여 준다 */
	void ShowMessage(const FText& Text, const FLinearColor& Color, float Seconds);

	// 화면 글자 만들기 (순수 함수 · 자동 시험 대상)
	/** 도구 이름 */
	static FText ToolName(ECozyTileTool Tool);
	/** 층 이름 (0 = 기본층, 그 밖 = N층) */
	static FText LevelName(uint8 Level);
	/** 커서 아래 칸 설명 · bInside가 false면 '영지 밖' */
	static FText HoverText(bool bInside, FIntPoint Coord, const FCozyTileCell& Cell);
	/** 도구가 성공한 뒤 짧은 확인 문장 (After = 바뀐 뒤 칸) */
	static FText OkText(ECozyTileTool Tool, const FCozyTileCell& After);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	/** 처음 한 번 글자 위젯들을 만든다 (설정 함수가 화면 추가 전에 불려도 되게) */
	void EnsureTree();
	UTextBlock* MakeText(float Size, const FText& Text);

	UPROPERTY(Transient) TObjectPtr<UTextBlock> ToolLine;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HelpLine;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HoverLine;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MessageLine;
	float MessageTimeLeft = 0.f;
};
