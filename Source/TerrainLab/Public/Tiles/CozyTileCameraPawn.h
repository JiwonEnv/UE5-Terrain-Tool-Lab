#pragma once
// 작업 3(조작) · 본 게임(CozyRealm)과 같은 쿼터뷰 카메라
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "CozyTileCameraPawn.generated.h"

class USpringArmComponent;
class UCameraComponent;

/** 땅 위 초점을 바라보는 카메라 · 휠 확대 · 가운데 버튼 끌기 회전 · WASD 이동 · Q/E 45도 회전 */
UCLASS()
class TERRAINLAB_API ACozyTileCameraPawn : public APawn
{
	GENERATED_BODY()
public:
	ACozyTileCameraPawn();
	virtual void Tick(float DeltaSeconds) override;

	/** 시야각 (본 게임과 같게) */
	UPROPERTY(EditAnywhere, Category="카메라") float FieldOfView = 30.f;
	/** 내려다보는 각도 */
	UPROPERTY(EditAnywhere, Category="카메라") float Pitch = -45.f;
	/** 처음 방향 */
	UPROPERTY(EditAnywhere, Category="카메라") float StartYaw = 45.f;
	/** 처음 거리 (cm) */
	UPROPERTY(EditAnywhere, Category="카메라") float StartDistance = 8500.f;
	UPROPERTY(EditAnywhere, Category="카메라") float MinDistance = 3500.f;
	UPROPERTY(EditAnywhere, Category="카메라") float MaxDistance = 12000.f;
	/** 휠 한 칸에 바뀌는 거리 비율 */
	UPROPERTY(EditAnywhere, Category="카메라") float ZoomStep = 0.12f;
	/** 초당 이동 거리 = 카메라 거리 × 이 값 */
	UPROPERTY(EditAnywhere, Category="카메라") float PanSpeedPerDistance = 0.8f;
	/** 가운데 버튼 끌기 1픽셀당 회전 각도 */
	UPROPERTY(EditAnywhere, Category="카메라") float DragYawPerPixel = 0.25f;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> Arm;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;

	float Yaw = 45.f, TargetYaw = 45.f;
	float Distance = 8500.f, TargetDistance = 8500.f;
	FVector2D LastMouse = FVector2D::ZeroVector;
	bool bDragging = false;
};
