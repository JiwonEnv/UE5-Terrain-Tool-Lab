#include "Tiles/CozyTileCameraPawn.h"
#include "Tiles/CozyTileTerrain.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"

ACozyTileCameraPawn::ACozyTileCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Arm = CreateDefaultSubobject<USpringArmComponent>(TEXT("Arm"));
	Arm->SetupAttachment(Root);
	Arm->bDoCollisionTest = false;
	Arm->bEnableCameraLag = false;
	Arm->bUsePawnControlRotation = false;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Arm, USpringArmComponent::SocketName);
	Camera->SetProjectionMode(ECameraProjectionMode::Perspective);
}

void ACozyTileCameraPawn::BeginPlay()
{
	Super::BeginPlay();
	Yaw = TargetYaw = StartYaw;
	Distance = TargetDistance = FMath::Clamp(StartDistance, MinDistance, MaxDistance);
	Camera->SetFieldOfView(FieldOfView);
	Arm->TargetArmLength = Distance;
	Arm->SetRelativeRotation(FRotator(Pitch, Yaw, 0.f));
	// 칸 땅이 있으면 열린 영지 가운데를 바라본다
	for (TActorIterator<ACozyTileTerrain> It(GetWorld()); It; ++It)
	{
		const FVector Center = (It->CellToWorld(It->OpenMin) + It->CellToWorld(It->OpenMax)) * 0.5;
		SetActorLocation(FVector(Center.X, Center.Y, It->GetActorLocation().Z));
		break;
	}
}

void ACozyTileCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	// 휠: 휠 이벤트는 같은 프레임에 눌림·뗌이 함께 오므로 '방금 눌림'으로 센다
	if (PC->WasInputKeyJustPressed(EKeys::MouseScrollUp)) TargetDistance *= 1.f - ZoomStep;
	if (PC->WasInputKeyJustPressed(EKeys::MouseScrollDown)) TargetDistance /= 1.f - ZoomStep;
	TargetDistance = FMath::Clamp(TargetDistance, MinDistance, MaxDistance);

	// 가운데 버튼 끌기: 커서 위치 차이로 회전 (커서가 보이는 상태라 마우스 축 값보다 확실)
	FVector2D Mouse;
	const bool bHasMouse = PC->GetMousePosition(Mouse.X, Mouse.Y);
	if (bHasMouse && PC->IsInputKeyDown(EKeys::MiddleMouseButton))
	{
		if (bDragging) { Yaw += (Mouse.X - LastMouse.X) * DragYawPerPixel; TargetYaw = Yaw; }
		bDragging = true;
	}
	else bDragging = false;
	if (bHasMouse) LastMouse = Mouse;

	// Q/E: 45도씩 회전 (가장 가까운 45도 칸으로 맞춘 뒤 한 칸)
	if (PC->WasInputKeyJustPressed(EKeys::Q)) TargetYaw = FMath::RoundToFloat(TargetYaw / 45.f) * 45.f - 45.f;
	if (PC->WasInputKeyJustPressed(EKeys::E)) TargetYaw = FMath::RoundToFloat(TargetYaw / 45.f) * 45.f + 45.f;

	Yaw = FMath::FInterpTo(Yaw, TargetYaw, DeltaSeconds, 10.f);
	Distance = FMath::FInterpTo(Distance, TargetDistance, DeltaSeconds, 10.f);
	Arm->TargetArmLength = Distance;
	Arm->SetRelativeRotation(FRotator(Pitch, Yaw, 0.f));

	// WASD·화살표: 카메라 방향 기준으로 땅 위 초점 이동
	FVector2D Move = FVector2D::ZeroVector;
	if (PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::Up)) Move.X += 1.f;
	if (PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Down)) Move.X -= 1.f;
	if (PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Right)) Move.Y += 1.f;
	if (PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Left)) Move.Y -= 1.f;
	if (!Move.IsNearlyZero())
	{
		const FRotator Flat(0.f, Yaw, 0.f);
		const FVector Dir = Flat.RotateVector(FVector(Move.X, Move.Y, 0.f)).GetSafeNormal2D();
		AddActorWorldOffset(Dir * Distance * PanSpeedPerDistance * DeltaSeconds);
	}
}
