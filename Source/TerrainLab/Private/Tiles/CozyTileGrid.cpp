#include "Tiles/CozyTileGrid.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace CozyTileGridPrivate
{
	// 저장 형식 버전 · 형식을 바꾸면 올린다
	constexpr int32 SaveVersion = 1;
	// 되돌리기 기록 최대 개수 (넘으면 가장 오래된 것부터 버림)
	constexpr int32 MaxUndo = 200;
}

FText CozyTileResultText(ECozyTileEditResult Result)
{
	switch (Result)
	{
	case ECozyTileEditResult::Ok:                  return FText::GetEmpty();
	case ECozyTileEditResult::OutOfBounds:         return NSLOCTEXT("CozyTile", "OutOfBounds", "영지 밖입니다");
	case ECozyTileEditResult::Locked:              return NSLOCTEXT("CozyTile", "Locked", "아직 열리지 않은 땅입니다");
	case ECozyTileEditResult::Occupied:            return NSLOCTEXT("CozyTile", "Occupied", "시설이나 꾸미기가 놓인 칸은 고칠 수 없습니다");
	case ECozyTileEditResult::AtMaxLevel:          return NSLOCTEXT("CozyTile", "AtMaxLevel", "더 높이 올릴 수 없습니다");
	case ECozyTileEditResult::AtMinLevel:          return NSLOCTEXT("CozyTile", "AtMinLevel", "더 내릴 수 없습니다 (물을 파려면 물 도구)");
	case ECozyTileEditResult::IsWater:             return NSLOCTEXT("CozyTile", "IsWater", "물 칸입니다. 먼저 물을 메워 주세요");
	case ECozyTileEditResult::WaterNeedsBaseLevel: return NSLOCTEXT("CozyTile", "WaterNeedsBaseLevel", "물은 가장 낮은 땅에서만 팔 수 있습니다");
	}
	return FText::GetEmpty();
}

void UCozyTileGrid::Init(int32 InWidth, int32 InHeight, const FCozyTileSettings& InSettings)
{
	Width = FMath::Max(0, InWidth);
	Height = FMath::Max(0, InHeight);
	Settings = InSettings;
	Cells.Init(FCozyTileCell(), Width * Height);
	UndoStack.Reset();
}

FCozyTileCell UCozyTileGrid::GetCell(int32 X, int32 Y) const
{
	return IsInside(X, Y) ? Cells[Index(X, Y)] : FCozyTileCell();
}

ECozyTileEditResult UCozyTileGrid::CanApply(ECozyTileTool Tool, int32 X, int32 Y) const
{
	// 검사 순서: 영지 밖 → 잠김 → 놓인 물건 → 도구별 규칙
	if (!IsInside(X, Y)) return ECozyTileEditResult::OutOfBounds;
	const FCozyTileCell& C = Cells[Index(X, Y)];
	if (C.bLocked) return ECozyTileEditResult::Locked;
	if (C.bOccupied) return ECozyTileEditResult::Occupied;

	switch (Tool)
	{
	case ECozyTileTool::Raise:
		if (C.IsWater()) return ECozyTileEditResult::IsWater;
		if (C.Level >= Settings.MaxLevel()) return ECozyTileEditResult::AtMaxLevel;
		return ECozyTileEditResult::Ok;
	case ECozyTileTool::Lower:
		if (C.IsWater()) return ECozyTileEditResult::IsWater;
		if (C.Level == 0) return ECozyTileEditResult::AtMinLevel;
		return ECozyTileEditResult::Ok;
	case ECozyTileTool::Water:
		// 물 칸이면 메우기 · 땅이면 기본층에서만 파기
		if (C.IsWater()) return ECozyTileEditResult::Ok;
		if (C.Level > 0) return ECozyTileEditResult::WaterNeedsBaseLevel;
		return ECozyTileEditResult::Ok;
	}
	return ECozyTileEditResult::Ok;
}

ECozyTileEditResult UCozyTileGrid::Apply(ECozyTileTool Tool, int32 X, int32 Y)
{
	const ECozyTileEditResult Result = CanApply(Tool, X, Y);
	if (Result != ECozyTileEditResult::Ok) return Result;

	FCozyTileCell& C = Cells[Index(X, Y)];
	if (UndoStack.Num() >= CozyTileGridPrivate::MaxUndo) UndoStack.RemoveAt(0);
	UndoStack.Add(FUndoEntry{FIntPoint(X, Y), C});

	switch (Tool)
	{
	case ECozyTileTool::Raise: ++C.Level; break;
	case ECozyTileTool::Lower: --C.Level; break;
	case ECozyTileTool::Water:
		C.Level = 0;
		C.Ground = C.IsWater() ? ECozyTileGround::Grass : ECozyTileGround::Water;
		break;
	}
	OnCellsChanged.Broadcast({FIntPoint(X, Y)});
	return Result;
}

bool UCozyTileGrid::Undo()
{
	if (UndoStack.Num() == 0) return false;
	const FUndoEntry Entry = UndoStack.Pop();
	if (!IsInside(Entry.Cell.X, Entry.Cell.Y)) return false;
	// 층·바닥만 되돌린다 · 잠금·놓인 물건은 그 뒤 바뀐 지금 값을 지킨다
	FCozyTileCell& C = Cells[Index(Entry.Cell.X, Entry.Cell.Y)];
	C.Level = Entry.Before.Level;
	C.Ground = Entry.Before.Ground;
	OnCellsChanged.Broadcast({Entry.Cell});
	return true;
}

void UCozyTileGrid::SetLockedRect(FIntPoint Min, FIntPoint Max, bool bLocked)
{
	TArray<FIntPoint> Changed;
	const int32 X0 = FMath::Max(0, FMath::Min(Min.X, Max.X)), X1 = FMath::Min(Width - 1, FMath::Max(Min.X, Max.X));
	const int32 Y0 = FMath::Max(0, FMath::Min(Min.Y, Max.Y)), Y1 = FMath::Min(Height - 1, FMath::Max(Min.Y, Max.Y));
	for (int32 Y = Y0; Y <= Y1; ++Y)
		for (int32 X = X0; X <= X1; ++X)
		{
			FCozyTileCell& C = Cells[Index(X, Y)];
			if (C.bLocked != bLocked) { C.bLocked = bLocked; Changed.Add(FIntPoint(X, Y)); }
		}
	if (Changed.Num() > 0) OnCellsChanged.Broadcast(Changed);
}

void UCozyTileGrid::SetOccupiedRect(FIntPoint Min, FIntPoint Max, bool bOccupied)
{
	TArray<FIntPoint> Changed;
	const int32 X0 = FMath::Max(0, FMath::Min(Min.X, Max.X)), X1 = FMath::Min(Width - 1, FMath::Max(Min.X, Max.X));
	const int32 Y0 = FMath::Max(0, FMath::Min(Min.Y, Max.Y)), Y1 = FMath::Min(Height - 1, FMath::Max(Min.Y, Max.Y));
	for (int32 Y = Y0; Y <= Y1; ++Y)
		for (int32 X = X0; X <= X1; ++X)
		{
			FCozyTileCell& C = Cells[Index(X, Y)];
			if (C.bOccupied != bOccupied) { C.bOccupied = bOccupied; Changed.Add(FIntPoint(X, Y)); }
		}
	if (Changed.Num() > 0) OnCellsChanged.Broadcast(Changed);
}

void UCozyTileGrid::ClearOccupied()
{
	TArray<FIntPoint> Changed;
	for (int32 Y = 0; Y < Height; ++Y)
		for (int32 X = 0; X < Width; ++X)
		{
			FCozyTileCell& C = Cells[Index(X, Y)];
			if (C.bOccupied) { C.bOccupied = false; Changed.Add(FIntPoint(X, Y)); }
		}
	if (Changed.Num() > 0) OnCellsChanged.Broadcast(Changed);
}

void UCozyTileGrid::SetCellRaw(int32 X, int32 Y, const FCozyTileCell& Cell)
{
	if (!IsInside(X, Y)) return;
	Cells[Index(X, Y)] = Cell;
	OnCellsChanged.Broadcast({FIntPoint(X, Y)});
}

FString UCozyTileGrid::ToJson() const
{
	// 칸 순서: Y줄마다 X 순서 (Index = Y * Width + X)
	TArray<TSharedPtr<FJsonValue>> Levels, Grounds, Locked;
	Levels.Reserve(Cells.Num()); Grounds.Reserve(Cells.Num()); Locked.Reserve(Cells.Num());
	for (const FCozyTileCell& C : Cells)
	{
		Levels.Add(MakeShared<FJsonValueNumber>(C.Level));
		Grounds.Add(MakeShared<FJsonValueNumber>(int32(C.Ground)));
		Locked.Add(MakeShared<FJsonValueNumber>(C.bLocked ? 1 : 0));
	}
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("version"), CozyTileGridPrivate::SaveVersion);
	Root->SetNumberField(TEXT("width"), Width);
	Root->SetNumberField(TEXT("height"), Height);
	Root->SetArrayField(TEXT("level"), Levels);
	Root->SetArrayField(TEXT("ground"), Grounds);
	Root->SetArrayField(TEXT("locked"), Locked);

	FString Out;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	FJsonSerializer::Serialize(Root, Writer);
	return Out;
}

bool UCozyTileGrid::FromJson(const FString& Json)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return false;

	int32 Version = 0, W = 0, H = 0;
	if (!Root->TryGetNumberField(TEXT("version"), Version) || Version < 1 || Version > CozyTileGridPrivate::SaveVersion) return false;
	if (!Root->TryGetNumberField(TEXT("width"), W) || !Root->TryGetNumberField(TEXT("height"), H)) return false;
	if (W != Width || H != Height) return false;

	const TArray<TSharedPtr<FJsonValue>>* Levels = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Grounds = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Locked = nullptr;
	if (!Root->TryGetArrayField(TEXT("level"), Levels) || !Root->TryGetArrayField(TEXT("ground"), Grounds) || !Root->TryGetArrayField(TEXT("locked"), Locked)) return false;
	const int32 Num = Width * Height;
	if (Levels->Num() != Num || Grounds->Num() != Num || Locked->Num() != Num) return false;

	// 먼저 새 칸을 모두 읽어 검사한다 · 하나라도 이상하면 지금 데이터를 그대로 둔다
	TArray<FCozyTileCell> NewCells;
	NewCells.SetNum(Num);
	for (int32 i = 0; i < Num; ++i)
	{
		int32 L = 0, G = 0, K = 0;
		if (!(*Levels)[i].IsValid() || !(*Levels)[i]->TryGetNumber(L)) return false;
		if (!(*Grounds)[i].IsValid() || !(*Grounds)[i]->TryGetNumber(G)) return false;
		if (!(*Locked)[i].IsValid() || !(*Locked)[i]->TryGetNumber(K)) return false;
		if (L < 0 || L > Settings.MaxLevel()) return false;
		if (G < int32(ECozyTileGround::Grass) || G > int32(ECozyTileGround::Water)) return false;
		FCozyTileCell& C = NewCells[i];
		C.Level = uint8(L);
		C.Ground = ECozyTileGround(G);
		C.bLocked = K != 0;
		C.bOccupied = Cells[i].bOccupied; // 놓인 물건은 저장하지 않으므로 지금 값을 지킨다
	}

	Cells = MoveTemp(NewCells);
	UndoStack.Reset();
	TArray<FIntPoint> All;
	All.Reserve(Num);
	for (int32 Y = 0; Y < Height; ++Y)
		for (int32 X = 0; X < Width; ++X) All.Add(FIntPoint(X, Y));
	OnCellsChanged.Broadcast(All);
	return true;
}

bool UCozyTileGrid::SaveToFile(const FString& Path) const
{
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	return FFileHelper::SaveStringToFile(ToJson(), *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

bool UCozyTileGrid::LoadFromFile(const FString& Path)
{
	FString Json;
	if (!IFileManager::Get().FileExists(*Path) || !FFileHelper::LoadFileToString(Json, *Path)) return false;
	return FromJson(Json);
}
