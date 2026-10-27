#include "Tiles/CozyTileGameMode.h"
#include "Tiles/CozyTileCameraPawn.h"
#include "Tiles/CozyTilePlayerController.h"

ACozyTileGameMode::ACozyTileGameMode()
{
	DefaultPawnClass = ACozyTileCameraPawn::StaticClass();
	PlayerControllerClass = ACozyTilePlayerController::StaticClass();
	// HUDClass는 기본값 그대로 (화면 글자는 위젯이 그린다)
}
