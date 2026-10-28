#include "Tiles/CozyTileTerrain.h"
#include "Tiles/CozyTileGrid.h"
#include "Tiles/CozyTileMesh.h"
#include "ProceduralMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

// 칸 땅 액터 (작업 2) · 칸 데이터 → 8×8칸 구역별 메시 · 바뀐 칸 주변 구역만 다시 그린다

ACozyTileTerrain::ACozyTileTerrain()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	WaterMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WaterMesh"));
	WaterMesh->SetupAttachment(Root);
	WaterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WaterMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	WaterMesh->CastShadow = false;
}

void ACozyTileTerrain::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// 편집기 미리보기 · 설정(크기·열린 영지)이 바뀌었을 수 있으니 새로 만든다 · 저장 파일은 읽지 않는다
	if (Grid) Grid->OnCellsChanged.RemoveAll(this);
	Grid = nullptr;
	CreateGridIfNeeded();
	RebuildAll();
}

void ACozyTileTerrain::BeginPlay()
{
	Super::BeginPlay();
	if (Grid) Grid->OnCellsChanged.RemoveAll(this);
	Grid = nullptr;
	CreateGridIfNeeded();
	const FString Path = SavePath();
	if (FPaths::FileExists(Path)) Grid->LoadFromFile(Path);   // 실패하면 처음 상태 그대로
	RefreshBlockers();
	RebuildAll();
}

void ACozyTileTerrain::CreateGridIfNeeded()
{
	const FIntPoint Size(FMath::Max(GridSize.X, 1), FMath::Max(GridSize.Y, 1));
	if (Grid && Grid->GetWidth() == Size.X && Grid->GetHeight() == Size.Y) return;
	if (Grid) Grid->OnCellsChanged.RemoveAll(this);
	Grid = NewObject<UCozyTileGrid>(this);
	Grid->Init(Size.X, Size.Y, Settings);
	// 전부 잠그고 처음 열린 영지만 푼다
	Grid->SetLockedRect(FIntPoint(0, 0), Size - FIntPoint(1, 1), true);
	const FIntPoint Lo(FMath::Clamp(OpenMin.X, 0, Size.X - 1), FMath::Clamp(OpenMin.Y, 0, Size.Y - 1));
	const FIntPoint Hi(FMath::Clamp(OpenMax.X, 0, Size.X - 1), FMath::Clamp(OpenMax.Y, 0, Size.Y - 1));
	if (Lo.X <= Hi.X && Lo.Y <= Hi.Y) Grid->SetLockedRect(Lo, Hi, false);
	Grid->OnCellsChanged.AddUObject(this, &ACozyTileTerrain::RebuildAround);
}

UMaterialInterface* ACozyTileTerrain::ResolveMaterial(UMaterialInterface* Preferred)
{
	if (Preferred) return Preferred;
	if (!DefaultMaterial) DefaultMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
	return DefaultMaterial;
}

void ACozyTileTerrain::EnsureChunks()
{
	if (!Grid) return;
	const int32 NX = FMath::DivideAndRoundUp(Grid->GetWidth(), ChunkCells);
	const int32 NY = FMath::DivideAndRoundUp(Grid->GetHeight(), ChunkCells);
	bool bValid = Chunks.Num() == NX * NY;
	for (const TObjectPtr<UProceduralMeshComponent>& C : Chunks) bValid &= IsValid(C);
	if (bValid) { ChunksX = NX; ChunksY = NY; return; }

	// 개수가 바뀌었거나 복제(PIE 등)로 남은 옛 구역 부품은 지우고 새로 만든다
	TArray<UProceduralMeshComponent*> Old;
	GetComponents(Old);
	for (UProceduralMeshComponent* C : Old) if (C && C != WaterMesh) C->DestroyComponent();
	Chunks.Reset();
	ChunksX = NX; ChunksY = NY;
	for (int32 I = 0; I < NX * NY; ++I)
	{
		UProceduralMeshComponent* C = NewObject<UProceduralMeshComponent>(this, NAME_None, RF_Transient);
		C->SetupAttachment(Root);
		// 플레이어 칸 고르기(Visibility 선 검사)·카메라에 걸리도록 모두 막음 · 복잡한 모양을 그대로 충돌로
		C->bUseComplexAsSimpleCollision = true;
		C->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		C->RegisterComponent();
		Chunks.Add(C);
	}
}

void ACozyTileTerrain::BuildChunk(int32 ChunkIndex)
{
	if (!Grid || !Chunks.IsValidIndex(ChunkIndex) || !IsValid(Chunks[ChunkIndex])) return;
	UProceduralMeshComponent* C = Chunks[ChunkIndex];
	const FIntPoint Min((ChunkIndex % ChunksX) * ChunkCells, (ChunkIndex / ChunksX) * ChunkCells);
	const FIntPoint Max(FMath::Min(Min.X + ChunkCells - 1, Grid->GetWidth() - 1), FMath::Min(Min.Y + ChunkCells - 1, Grid->GetHeight() - 1));
	FCozyTileMeshData Data;
	FCozyTileMeshBuilder::BuildRegion(*Grid, Min, Max, Data);
	if (Data.Triangles.IsEmpty()) { C->ClearMeshSection(0); return; }
	C->CreateMeshSection(0, Data.Vertices, Data.Triangles, Data.Normals, Data.UVs, Data.Colors, TArray<FProcMeshTangent>(), true);
	C->SetMaterial(0, ResolveMaterial(LandMaterial));
}

void ACozyTileTerrain::BuildWater()
{
	if (!Grid || !WaterMesh) return;
	FCozyTileMeshData Data;
	FCozyTileMeshBuilder::BuildWater(*Grid, FIntPoint(0, 0), FIntPoint(Grid->GetWidth() - 1, Grid->GetHeight() - 1), Data);
	if (Data.Triangles.IsEmpty()) { WaterMesh->ClearAllMeshSections(); return; }
	WaterMesh->CreateMeshSection(0, Data.Vertices, Data.Triangles, Data.Normals, Data.UVs, Data.Colors, TArray<FProcMeshTangent>(), false);
	WaterMesh->SetMaterial(0, ResolveMaterial(WaterMaterial));
}

void ACozyTileTerrain::RebuildAll()
{
	EnsureChunks();
	for (int32 I = 0; I < Chunks.Num(); ++I) BuildChunk(I);
	BuildWater();
}

void ACozyTileTerrain::RebuildAround(const TArray<FIntPoint>& Changed)
{
	if (!Grid) return;
	EnsureChunks();
	if (ChunksX <= 0) return;
	// 바뀐 칸과 8방향 이웃이 속한 구역만 (옆면은 이웃 칸 높이에 따라 달라지므로)
	TSet<int32> Dirty;
	for (const FIntPoint& P : Changed)
		for (int32 DY = -1; DY <= 1; ++DY)
			for (int32 DX = -1; DX <= 1; ++DX)
			{
				const int32 X = P.X + DX, Y = P.Y + DY;
				if (Grid->IsInside(X, Y)) Dirty.Add((Y / ChunkCells) * ChunksX + X / ChunkCells);
			}
	for (int32 I : Dirty) BuildChunk(I);
	BuildWater();   // 물 표면은 칸 수가 적어 통째로
}

bool ACozyTileTerrain::WorldToCell(const FVector& World, FIntPoint& OutCell) const
{
	const float CS = FMath::Max(Settings.CellSize, 1.f);
	const FVector L = GetActorTransform().InverseTransformPosition(World);
	OutCell = FIntPoint(FMath::FloorToInt(L.X / CS), FMath::FloorToInt(L.Y / CS));
	return Grid ? Grid->IsInside(OutCell.X, OutCell.Y) : (OutCell.X >= 0 && OutCell.Y >= 0 && OutCell.X < GridSize.X && OutCell.Y < GridSize.Y);
}

FVector ACozyTileTerrain::CellToWorld(FIntPoint Cell) const
{
	const float CS = FMath::Max(Settings.CellSize, 1.f);
	const float Z = Grid ? Grid->GetSettings().TopHeight(Grid->GetCell(Cell.X, Cell.Y)) : Settings.TopHeight(FCozyTileCell());
	return GetActorTransform().TransformPosition(FVector((Cell.X + 0.5f) * CS, (Cell.Y + 0.5f) * CS, Z));
}

ECozyTileEditResult ACozyTileTerrain::ApplyTool(ECozyTileTool Tool, FIntPoint Cell)
{
	if (!Grid) return ECozyTileEditResult::OutOfBounds;
	const ECozyTileEditResult R = Grid->Apply(Tool, Cell.X, Cell.Y);   // 다시 그리기는 OnCellsChanged → RebuildAround
	if (R == ECozyTileEditResult::Ok) Grid->SaveToFile(SavePath());
	return R;
}

bool ACozyTileTerrain::UndoLast()
{
	if (!Grid || !Grid->Undo()) return false;
	Grid->SaveToFile(SavePath());
	return true;
}

void ACozyTileTerrain::ResetToDefault()
{
	IFileManager::Get().Delete(*SavePath(), false, false, true);
	if (Grid) Grid->OnCellsChanged.RemoveAll(this);
	Grid = nullptr;
	CreateGridIfNeeded();
	RefreshBlockers();
	RebuildAll();
}

void ACozyTileTerrain::RefreshBlockers()
{
	if (!Grid) return;
	Grid->ClearOccupied();
	UWorld* World = GetWorld();
	if (!World || BlockerTag.IsNone()) return;
	const float CS = FMath::Max(Settings.CellSize, 1.f);
	constexpr float Shrink = 5.f;   // 칸 경계에 살짝 걸친 것은 옆 칸까지 막지 않게
	const FTransform& T = GetActorTransform();
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!A || A == this || !A->ActorHasTag(BlockerTag)) continue;
		FVector Origin, Extent;
		A->GetActorBounds(false, Origin, Extent);
		const FVector L0 = T.InverseTransformPosition(Origin - Extent), L1 = T.InverseTransformPosition(Origin + Extent);
		const FVector Lo = L0.ComponentMin(L1), Hi = L0.ComponentMax(L1);
		FIntPoint Min(FMath::FloorToInt((Lo.X + Shrink) / CS), FMath::FloorToInt((Lo.Y + Shrink) / CS));
		FIntPoint Max(FMath::FloorToInt((Hi.X - Shrink) / CS), FMath::FloorToInt((Hi.Y - Shrink) / CS));
		// 아주 작은 물건은 중심 칸 하나
		const FVector C = T.InverseTransformPosition(Origin);
		const FIntPoint Center(FMath::FloorToInt(C.X / CS), FMath::FloorToInt(C.Y / CS));
		if (Min.X > Max.X) Min.X = Max.X = Center.X;
		if (Min.Y > Max.Y) Min.Y = Max.Y = Center.Y;
		Min.X = FMath::Max(Min.X, 0); Min.Y = FMath::Max(Min.Y, 0);
		Max.X = FMath::Min(Max.X, Grid->GetWidth() - 1); Max.Y = FMath::Min(Max.Y, Grid->GetHeight() - 1);
		if (Min.X <= Max.X && Min.Y <= Max.Y) Grid->SetOccupiedRect(Min, Max, true);
	}
}

FString ACozyTileTerrain::SavePath() const
{
	return FPaths::ProjectSavedDir() / TEXT("CozyTiles") / (SaveName + TEXT(".json"));
}
