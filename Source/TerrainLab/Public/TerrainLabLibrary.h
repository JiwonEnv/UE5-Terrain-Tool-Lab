#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TerrainLabLibrary.generated.h"

class ALandscape;
class ALandscapeBlueprintBrushBase;

/** 시험용 도우미 · 파이썬에서 부르려고 만든 함수 모음 (에디터에서만 동작) */
UCLASS()
class UTerrainLabLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** 블루프린트 브러시(Landmass 등)를 지형의 편집 레이어에 등록한다 · 이미 있으면 그대로 둔다 */
	UFUNCTION(BlueprintCallable, Category="TerrainLab")
	static bool AddBrushToLandscape(ALandscape* Landscape, ALandscapeBlueprintBrushBase* Brush, int32 LayerIndex = 0);

	/** 편집 레이어에 등록된 브러시 수 */
	UFUNCTION(BlueprintCallable, Category="TerrainLab")
	static int32 GetBrushCount(ALandscape* Landscape, int32 LayerIndex = 0);
};
