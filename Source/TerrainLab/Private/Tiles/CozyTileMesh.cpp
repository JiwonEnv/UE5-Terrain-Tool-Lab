#include "Tiles/CozyTileMesh.h"
#include "Tiles/CozyTileGrid.h"

// 칸 데이터 → 모양 (작업 2)
// 규칙: 칸마다 윗면 하나 · 이웃 칸이 더 낮으면 그 경계에 절벽 옆면(이웃 윗면 → 내 윗면) · 영지 밖 경계는 바닥판 높이까지 옆면
// 옆면은 항상 높은 칸이 만든다 → 구역(청크)을 나눠 그려도 한 번씩만 생기고 이음매가 없다
namespace CozyTileMeshLocal
{
	const FColor GrassColor(0x91, 0xB5, 0x7F);
	const FColor DirtColor(0xD9, 0xC4, 0x9A);
	const FColor StoneColor(0xA8, 0xAD, 0xB7);
	const FColor WaterBottomColor(0x5E, 0x8F, 0xA8);
	const FColor WaterSurfaceColor(0x7F, 0xB6, 0xD6);
	const FColor CliffColor(0x9B, 0x74, 0x56);
	const FColor CliffDarkColor(0x6E, 0x52, 0x40);
	constexpr float LockedScale = 0.6f;   // 잠긴 칸은 어둡게
	constexpr float SkirtExtra = 60.f;    // 바닥판이 물 바닥보다 더 내려가는 깊이

	FColor Scale(const FColor& C, float S)
	{
		auto Ch = [S](uint8 V) { return uint8(FMath::Clamp(FMath::RoundToInt(V * S), 0, 255)); };
		return FColor(Ch(C.R), Ch(C.G), Ch(C.B), C.A);
	}
	FColor GroundColor(const FCozyTileCell& C)
	{
		FColor Col = GrassColor;
		switch (C.Ground)
		{
		case ECozyTileGround::Dirt:  Col = DirtColor; break;
		case ECozyTileGround::Stone: Col = StoneColor; break;
		case ECozyTileGround::Water: Col = WaterBottomColor; break;
		default: break;
		}
		// 층이 높을수록 조금씩 밝게 (위에서 볼 때 아래층과 구분되게)
		if (!C.IsWater()) Col = Scale(Col, 1.f + 0.12f * C.Level);
		return C.bLocked ? Scale(Col, LockedScale) : Col;
	}
	float BaseLevel(const FCozyTileSettings& S) { return S.LevelHeights.Num() > 0 ? S.LevelHeights[0] : 0.f; }
	float SkirtBottom(const FCozyTileSettings& S) { return BaseLevel(S) - S.WaterDepth - SkirtExtra; }
	// 절벽 색은 높이(Z)로만 정한다 → 꺾이는 모서리에서 이웃 옆면과 색이 똑같이 맞는다 (아래쪽일수록 어둡게)
	FColor CliffColorAt(const FCozyTileSettings& S, float Z, bool bLocked)
	{
		const float Lo = BaseLevel(S) - S.WaterDepth;
		const float Hi = S.LevelHeights.Num() > 0 ? S.LevelHeights.Last() : Lo + 1.f;
		const float T = FMath::Clamp((Z - Lo) / FMath::Max(Hi - Lo, 1.f), 0.f, 1.f);
		auto Ch = [T](uint8 A, uint8 B) { return uint8(FMath::Clamp(FMath::RoundToInt(FMath::Lerp(float(A), float(B), T)), 0, 255)); };
		const FColor C(Ch(CliffDarkColor.R, CliffColor.R), Ch(CliffDarkColor.G, CliffColor.G), Ch(CliffDarkColor.B, CliffColor.B), 255);
		return bLocked ? Scale(C, LockedScale) : C;
	}

	// 사각형 하나 · 네 점은 둘레 순서(돌아가는 방향은 상관없음)
	// UE 앞면 규칙: 삼각형 (A,B,C)의 앞쪽은 Cross(C-A, B-A) 방향 (엔진 GenerateBoxMesh와 같은 규칙)
	// → 법선 N 쪽이 앞면이 되도록 감는 순서를 고른다
	void AddQuad(FCozyTileMeshData& Out, const FVector (&P)[4], const FVector2D (&UV)[4], const FColor (&Col)[4], const FVector& N)
	{
		const int32 B = Out.Vertices.Num();
		for (int32 I = 0; I < 4; ++I)
		{
			Out.Vertices.Add(P[I]); Out.Normals.Add(N); Out.UVs.Add(UV[I]); Out.Colors.Add(Col[I]);
		}
		const FVector G = FVector::CrossProduct(P[2] - P[0], P[1] - P[0]);
		if (FVector::DotProduct(G, N) >= 0) Out.Triangles.Append({B, B + 1, B + 2, B, B + 2, B + 3});
		else                                Out.Triangles.Append({B, B + 2, B + 1, B, B + 3, B + 2});
	}

	// 수직 옆면 · 경계선 A→B, 높이 ZLo→ZHi, 바깥(낮은 쪽)을 향하는 법선 N
	void AddWall(FCozyTileMeshData& Out, const FCozyTileSettings& S, FVector2D A, FVector2D B, float ZLo, float ZHi, const FVector& N, bool bLocked)
	{
		const float CS = FMath::Max(S.CellSize, 1.f);
		const FVector P[4] = {FVector(A.X, A.Y, ZLo), FVector(B.X, B.Y, ZLo), FVector(B.X, B.Y, ZHi), FVector(A.X, A.Y, ZHi)};
		// 가로 UV는 경계선 방향 월드 좌표, 세로 UV는 높이
		auto U = [&](FVector2D Q) { return (FMath::Abs(N.X) > 0.5f ? Q.Y : Q.X) / CS; };
		const FVector2D UV[4] = {FVector2D(U(A), -ZLo / CS), FVector2D(U(B), -ZLo / CS), FVector2D(U(B), -ZHi / CS), FVector2D(U(A), -ZHi / CS)};
		const FColor Lo = CliffColorAt(S, ZLo, bLocked), Hi = CliffColorAt(S, ZHi, bLocked);
		const FColor Col[4] = {Lo, Lo, Hi, Hi};
		AddQuad(Out, P, UV, Col, N);
	}

	// 수평 사각형 (윗면·물 표면)
	void AddTop(FCozyTileMeshData& Out, float CS, int32 X, int32 Y, float Z, const FColor& C)
	{
		const float X0 = X * CS, X1 = (X + 1) * CS, Y0 = Y * CS, Y1 = (Y + 1) * CS;
		const FVector P[4] = {FVector(X0, Y0, Z), FVector(X1, Y0, Z), FVector(X1, Y1, Z), FVector(X0, Y1, Z)};
		const double U0 = X, U1 = X + 1, V0 = Y, V1 = Y + 1;
		const FVector2D UV[4] = {FVector2D(U0, V0), FVector2D(U1, V0), FVector2D(U1, V1), FVector2D(U0, V1)};
		const FColor Col[4] = {C, C, C, C};
		AddQuad(Out, P, UV, Col, FVector::UpVector);
	}

	bool ClampRegion(const UCozyTileGrid& Grid, FIntPoint& Min, FIntPoint& Max)
	{
		Min.X = FMath::Max(Min.X, 0); Min.Y = FMath::Max(Min.Y, 0);
		Max.X = FMath::Min(Max.X, Grid.GetWidth() - 1); Max.Y = FMath::Min(Max.Y, Grid.GetHeight() - 1);
		return Min.X <= Max.X && Min.Y <= Max.Y;
	}
}

void FCozyTileMeshBuilder::BuildRegion(const UCozyTileGrid& Grid, FIntPoint Min, FIntPoint Max, FCozyTileMeshData& Out)
{
	using namespace CozyTileMeshLocal;
	if (!ClampRegion(Grid, Min, Max)) return;
	const FCozyTileSettings& S = Grid.GetSettings();
	const float CS = FMath::Max(S.CellSize, 1.f);
	const float Bottom = SkirtBottom(S);
	// 네 방향: 이웃 칸 차이, 경계선 두 점(칸 모서리 기준 0/1), 바깥 법선
	struct FSide { FIntPoint D; FVector2D A, B; FVector N; };
	static const FSide Sides[4] = {
		{FIntPoint(-1, 0), FVector2D(0, 0), FVector2D(0, 1), FVector(-1, 0, 0)},
		{FIntPoint( 1, 0), FVector2D(1, 0), FVector2D(1, 1), FVector( 1, 0, 0)},
		{FIntPoint(0, -1), FVector2D(0, 0), FVector2D(1, 0), FVector(0, -1, 0)},
		{FIntPoint(0,  1), FVector2D(0, 1), FVector2D(1, 1), FVector(0,  1, 0)}};

	for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
	for (int32 X = Min.X; X <= Max.X; ++X)
	{
		const FCozyTileCell C = Grid.GetCell(X, Y);
		const float Top = S.TopHeight(C);
		AddTop(Out, CS, X, Y, Top, GroundColor(C));
		for (const FSide& Side : Sides)
		{
			const int32 NX = X + Side.D.X, NY = Y + Side.D.Y;
			// 영지 밖은 바닥판 높이까지 (땅이 두꺼운 판처럼 보이게) · 구역 밖이어도 영지 안이면 이웃 칸을 본다
			const float NTop = Grid.IsInside(NX, NY) ? S.TopHeight(Grid.GetCell(NX, NY)) : Bottom;
			if (NTop >= Top - UE_KINDA_SMALL_NUMBER) continue;   // 같거나 높으면 옆면 없음 (높은 쪽이 만든다)
			const FVector2D A((X + Side.A.X) * CS, (Y + Side.A.Y) * CS), B((X + Side.B.X) * CS, (Y + Side.B.Y) * CS);
			AddWall(Out, S, A, B, NTop, Top, Side.N, C.bLocked);
		}
	}
}

void FCozyTileMeshBuilder::BuildWater(const UCozyTileGrid& Grid, FIntPoint Min, FIntPoint Max, FCozyTileMeshData& Out)
{
	using namespace CozyTileMeshLocal;
	if (!ClampRegion(Grid, Min, Max)) return;
	const FCozyTileSettings& S = Grid.GetSettings();
	const float CS = FMath::Max(S.CellSize, 1.f);
	const float Z = BaseLevel(S) - S.WaterSurfaceOffset;
	for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
	for (int32 X = Min.X; X <= Max.X; ++X)
	{
		const FCozyTileCell C = Grid.GetCell(X, Y);
		if (C.IsWater()) AddTop(Out, CS, X, Y, Z, C.bLocked ? Scale(WaterSurfaceColor, LockedScale) : WaterSurfaceColor);
	}
}
