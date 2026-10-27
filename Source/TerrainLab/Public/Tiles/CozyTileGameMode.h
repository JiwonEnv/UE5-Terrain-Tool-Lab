#pragma once
// 작업 3(조작) · 칸 땅 시험용 게임 모드 (쿼터뷰 카메라 + 마우스 조작)
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CozyTileGameMode.generated.h"

/** 칸 땅 고치기 시험 맵에 쓰는 게임 모드 · 월드 설정에서 이 모드를 고른다 */
UCLASS()
class TERRAINLAB_API ACozyTileGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	ACozyTileGameMode();
};
