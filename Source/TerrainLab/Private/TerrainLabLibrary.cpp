#include "TerrainLabLibrary.h"
#include "Landscape.h"
#include "LandscapeBlueprintBrushBase.h"

bool UTerrainLabLibrary::AddBrushToLandscape(ALandscape* Landscape, ALandscapeBlueprintBrushBase* Brush, int32 LayerIndex)
{
#if WITH_EDITOR
	if (!Landscape || !Brush) return false;
	if (Landscape->GetBrushLayer(Brush) == INDEX_NONE) Landscape->AddBrushToLayer(LayerIndex, Brush);
	Landscape->RequestLayersContentUpdateForceAll();
	return Landscape->GetBrushLayer(Brush) != INDEX_NONE;
#else
	return false;
#endif
}

int32 UTerrainLabLibrary::GetBrushCount(ALandscape* Landscape, int32 LayerIndex)
{
#if WITH_EDITOR
	int32 Count = 0;
	while (Landscape && Landscape->GetBrushForLayer(LayerIndex, Count)) ++Count;
	return Count;
#else
	return 0;
#endif
}
