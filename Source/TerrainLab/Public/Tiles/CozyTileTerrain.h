#pragma once
// 작업 2(모양 만들기)가 Private/Tiles/CozyTileTerrain.cpp에 구현한다. 공개 함수 모양은 작업 3(조작)과의 약속이다.
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Tiles/CozyTileTypes.h"
#include "CozyTileTerrain.generated.h"

class UCozyTileGrid;
class UProceduralMeshComponent;

/** 레벨에 놓는 칸 땅 · 칸 데이터를 갖고, 바뀐 곳만 다시 그린다 */
UCLASS()
class TERRAINLAB_API ACozyTileTerrain : public AActor
{
	GENERATED_BODY()
public:
	ACozyTileTerrain();

	/** 칸 수 (시작 영지 12×12 + 잠긴 확장 칸을 포함한 전체) */
	UPROPERTY(EditAnywhere, Category="칸 땅") FIntPoint GridSize = FIntPoint(20, 20);
	/** 처음 열려 있는 영지 (나머지는 잠김) · Min 포함 Max 포함 */
	UPROPERTY(EditAnywhere, Category="칸 땅") FIntPoint OpenMin = FIntPoint(4, 4);
	UPROPERTY(EditAnywhere, Category="칸 땅") FIntPoint OpenMax = FIntPoint(15, 15);
	UPROPERTY(EditAnywhere, Category="칸 땅") FCozyTileSettings Settings;
	/** 이 태그가 붙은 액터 아래 칸은 '놓인 물건'으로 막는다 (BeginPlay와 RefreshBlockers에서) */
	UPROPERTY(EditAnywhere, Category="칸 땅") FName BlockerTag = TEXT("CozyTileBlocker");
	/** 저장 파일 이름 (프로젝트 Saved/CozyTiles/<이름>.json) */
	UPROPERTY(EditAnywhere, Category="칸 땅") FString SaveName = TEXT("Estate");
	UPROPERTY(EditAnywhere, Category="칸 땅") TObjectPtr<UMaterialInterface> LandMaterial;
	UPROPERTY(EditAnywhere, Category="칸 땅") TObjectPtr<UMaterialInterface> WaterMaterial;

	UCozyTileGrid* GetGrid() const { return Grid; }

	/** 월드 위치 → 칸 (밖이면 false) · 높이는 무시 */
	bool WorldToCell(const FVector& World, FIntPoint& OutCell) const;
	/** 칸 중심의 월드 위치 (윗면 높이) */
	FVector CellToWorld(FIntPoint Cell) const;

	/** 도구 사용 → 결과 · Ok면 바뀐 곳을 다시 그리고 자동 저장 */
	ECozyTileEditResult ApplyTool(ECozyTileTool Tool, FIntPoint Cell);
	/** 되돌리기 → 되돌린 게 있으면 true · 다시 그리고 자동 저장 */
	bool UndoLast();
	/** 저장 파일을 지우고 처음 상태로 */
	void ResetToDefault();
	/** 막는 액터 아래 칸을 다시 계산 */
	void RefreshBlockers();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

private:
	// 아래는 작업 2가 자유롭게 바꿔도 된다 (구역별 메시 · 다시 그리기 · 저장 경로)
	UPROPERTY(Transient) TObjectPtr<UCozyTileGrid> Grid;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(Transient) TArray<TObjectPtr<UProceduralMeshComponent>> Chunks;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> WaterMesh;
	/** 기본 재질 (LandMaterial·WaterMaterial이 비었을 때 · 정점 색 재질) */
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> DefaultMaterial;
	/** 구역 한 변 칸 수 · 구역 개수 */
	static constexpr int32 ChunkCells = 8;
	int32 ChunksX = 0;
	int32 ChunksY = 0;
	void CreateGridIfNeeded();
	/** 칸 수에 맞게 구역 메시 부품을 만들거나 지운다 */
	void EnsureChunks();
	void BuildChunk(int32 ChunkIndex);
	void BuildWater();
	UMaterialInterface* ResolveMaterial(UMaterialInterface* Preferred);
	void RebuildAll();
	void RebuildAround(const TArray<FIntPoint>& Changed);
	FString SavePath() const;
};
