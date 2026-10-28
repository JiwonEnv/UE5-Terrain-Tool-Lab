#pragma once
// 플레이어 땅 고치기(B) 1단계 · 공통 약속 (작업 0)
// 이 파일은 작업 1·2·3이 함께 쓴다. 고치려면 먼저 합의한다.
#include "CoreMinimal.h"
#include "CozyTileTypes.generated.h"

/** 칸 바닥 종류 · 물은 기본층(0)에서만 */
UENUM(BlueprintType)
enum class ECozyTileGround : uint8
{
	Grass UMETA(DisplayName="풀"),
	Dirt  UMETA(DisplayName="흙길"),
	Stone UMETA(DisplayName="돌길"),
	Water UMETA(DisplayName="물")
};

/** 플레이어 도구 */
UENUM(BlueprintType)
enum class ECozyTileTool : uint8
{
	Raise UMETA(DisplayName="층 올리기"),
	Lower UMETA(DisplayName="층 내리기"),
	Water UMETA(DisplayName="물 파기·메우기")
};

/** 도구를 썼을 때 결과 · Ok가 아니면 칸은 바뀌지 않는다 */
UENUM(BlueprintType)
enum class ECozyTileEditResult : uint8
{
	Ok,
	OutOfBounds,         // 영지 밖
	Locked,              // 아직 열지 않은 칸
	Occupied,            // 시설·꾸미기가 놓인 칸
	AtMaxLevel,          // 이미 최고층
	AtMinLevel,          // 이미 기본층 (더 내리려면 물 도구)
	IsWater,             // 물 칸은 올리기·내리기 불가 (먼저 메우기)
	WaterNeedsBaseLevel  // 물은 기본층에서만
};

/** 칸 하나 · 층은 0(기본)부터 MaxLevel까지 · 물은 Level 0 + Ground Water · 이웃 칸과 층 차이 제한 없음(사용자 결정) */
USTRUCT(BlueprintType)
struct TERRAINLAB_API FCozyTileCell
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) uint8 Level = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) ECozyTileGround Ground = ECozyTileGround::Grass;
	/** 아직 열지 않은 칸 (영지 확장 전) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bLocked = false;
	/** 시설·꾸미기가 놓여 있음 (저장하지 않음 · 배치에서 다시 채움) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bOccupied = false;

	bool IsWater() const { return Ground == ECozyTileGround::Water; }
	bool operator==(const FCozyTileCell& O) const { return Level == O.Level && Ground == O.Ground && bLocked == O.bLocked && bOccupied == O.bOccupied; }
};

/** 크기·높이 설정 (임시값 · 데이터로 바꿀 수 있게) · 단위 cm */
USTRUCT(BlueprintType)
struct TERRAINLAB_API FCozyTileSettings
{
	GENERATED_BODY()
	/** 칸 한 변 (2m) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float CellSize = 200.f;
	/** 층별 윗면 높이 · 개수 = 층 수 (0 / 2.4 / 4.8m) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<float> LevelHeights = {0.f, 240.f, 480.f};
	/** 물 칸 바닥이 기본층보다 낮은 깊이 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float WaterDepth = 120.f;
	/** 물 표면이 기본층보다 낮은 정도 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float WaterSurfaceOffset = 30.f;

	uint8 MaxLevel() const { return uint8(FMath::Max(0, LevelHeights.Num() - 1)); }
	/** 칸 윗면 높이 (물이면 물 바닥 높이) */
	float TopHeight(const FCozyTileCell& C) const
	{
		const float H = LevelHeights.IsValidIndex(C.Level) ? LevelHeights[C.Level] : 0.f;
		return C.IsWater() ? H - WaterDepth : H;
	}
};

/** 결과를 플레이어에게 보여 줄 한국어 문장 (작업 1이 CozyTileGrid.cpp에 구현) */
TERRAINLAB_API FText CozyTileResultText(ECozyTileEditResult Result);
