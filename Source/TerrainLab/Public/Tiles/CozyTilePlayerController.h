#pragma once
// 작업 3(조작) · 마우스로 칸을 고르고 도구를 쓴다
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Tiles/CozyTileTypes.h"
#include "CozyTilePlayerController.generated.h"

class ACozyTileTerrain;
class UCozyTileHudWidget;

/** 커서 아래 칸 표시 · 1/2/3 도구 · 왼쪽 클릭 사용 · 오른쪽 클릭 내리기 · Ctrl+Z 되돌리기 · F9 처음으로 */
UCLASS()
class TERRAINLAB_API ACozyTilePlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	ACozyTilePlayerController();
	virtual void PlayerTick(float DeltaTime) override;

	/** 알림 표시 시간 (초) */
	UPROPERTY(EditAnywhere, Category="칸 조작") float WarningSeconds = 2.5f;
	UPROPERTY(EditAnywhere, Category="칸 조작") float OkSeconds = 1.2f;

protected:
	virtual void BeginPlay() override;

private:
	void UseTool(ECozyTileTool Tool);
	void DrawHover() const;

	UPROPERTY(Transient) TObjectPtr<ACozyTileTerrain> Terrain;
	UPROPERTY(Transient) TObjectPtr<UCozyTileHudWidget> Hud;
	ECozyTileTool CurrentTool = ECozyTileTool::Raise;
	FIntPoint HoverCell = FIntPoint(-1, -1);
	bool bHasHover = false;
};
