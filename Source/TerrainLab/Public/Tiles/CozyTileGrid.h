#pragma once
// 작업 1(데이터·규칙)이 Private/Tiles/CozyTileGrid.cpp에 구현한다. 공개 함수 모양은 약속이다.
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Tiles/CozyTileTypes.h"
#include "CozyTileGrid.generated.h"

/** 영지 칸 데이터와 규칙 · 화면·입력과 무관한 순수 데이터 (자동 시험 대상) */
UCLASS(BlueprintType)
class TERRAINLAB_API UCozyTileGrid : public UObject
{
	GENERATED_BODY()
public:
	/** 모든 칸을 기본층 풀로 초기화 · 되돌리기 기록도 비움 */
	void Init(int32 InWidth, int32 InHeight, const FCozyTileSettings& InSettings);

	int32 GetWidth() const { return Width; }
	int32 GetHeight() const { return Height; }
	const FCozyTileSettings& GetSettings() const { return Settings; }
	bool IsInside(int32 X, int32 Y) const { return X >= 0 && Y >= 0 && X < Width && Y < Height; }
	/** 밖이면 기본값 칸(Level 0 · Grass)을 돌려준다 */
	FCozyTileCell GetCell(int32 X, int32 Y) const;

	/** 규칙만 검사 (칸은 바꾸지 않음) */
	ECozyTileEditResult CanApply(ECozyTileTool Tool, int32 X, int32 Y) const;
	/** 규칙 검사 후 Ok면 칸을 바꾸고 되돌리기 기록에 쌓고 OnCellsChanged를 알린다 */
	ECozyTileEditResult Apply(ECozyTileTool Tool, int32 X, int32 Y);
	/** 마지막 Apply를 되돌린다 · 되돌린 게 있으면 true · OnCellsChanged를 알린다 */
	bool Undo();
	int32 GetUndoCount() const { return UndoStack.Num(); }

	/** 잠금·놓인 물건 설정 (되돌리기 기록에 넣지 않음) · 사각형은 Min 포함 Max 포함 */
	void SetLockedRect(FIntPoint Min, FIntPoint Max, bool bLocked);
	void SetOccupiedRect(FIntPoint Min, FIntPoint Max, bool bOccupied);
	/** 놓인 물건 표시를 모두 지움 */
	void ClearOccupied();
	/** 개발·시험용으로 칸을 바로 바꿈 (규칙·되돌리기 없음 · 알림은 보냄) */
	void SetCellRaw(int32 X, int32 Y, const FCozyTileCell& Cell);

	/** 저장 (크기·층·바닥·잠금 · 놓인 물건은 저장하지 않음) · 불러올 때 크기가 다르면 실패 */
	FString ToJson() const;
	bool FromJson(const FString& Json);
	bool SaveToFile(const FString& Path) const;
	bool LoadFromFile(const FString& Path);

	/** 바뀐 칸 좌표 (화면은 이 칸과 이웃만 다시 그리면 된다) */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnCellsChanged, const TArray<FIntPoint>& /*ChangedCells*/);
	FOnCellsChanged OnCellsChanged;

private:
	struct FUndoEntry { FIntPoint Cell; FCozyTileCell Before; };
	int32 Index(int32 X, int32 Y) const { return Y * Width + X; }

	UPROPERTY() int32 Width = 0;
	UPROPERTY() int32 Height = 0;
	UPROPERTY() FCozyTileSettings Settings;
	UPROPERTY() TArray<FCozyTileCell> Cells;
	TArray<FUndoEntry> UndoStack;
};
