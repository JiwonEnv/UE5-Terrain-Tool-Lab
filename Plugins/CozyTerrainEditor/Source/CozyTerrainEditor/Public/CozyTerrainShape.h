#pragma once
// 자유 모양(스플라인) 계산 · 작업 3이 Private/CozyTerrainShape.cpp에 구현한다. 공개 모양은 작업 1·2와의 약속이다.
// 좌표는 모두 평지·산의 중심 기준 미터(회전을 되돌린 뒤)다.
#include "CoreMinimal.h"
#include "CozyTerrainLayout.h"

/** 닫힌 곡선을 꺾은선으로 펼친 것 + 계산용 정보 */
struct COZYTERRAINEDITOR_API FCozyPolyShape
{
    /** 닫힌 꺾은선 꼭짓점 (마지막 → 첫 점이 이어진다) */
    TArray<FVector2D> Poly;
    /** 꺾은선 꼭짓점마다 몇 번째 구간(조절점 i → i+1)에서 나왔는지 */
    TArray<int32> Span;
    FBox2D Bounds=FBox2D(ForceInit);
    /** 모양 안쪽에서 테두리까지 가장 먼 거리 (산의 높이 모양을 정할 때 씀) */
    float MaxInset=0;
    bool IsValid() const { return Poly.Num()>=3; }
};

namespace CozyTerrainShape
{
    /** 조절점 → 닫힌 꺾은선. 뾰족한 점은 그대로 꺾이고(양쪽 구간이 직선으로 들어옴), 부드러운 점은 곡선(구심 Catmull-Rom)으로 지난다.
     *  모든 점이 뾰족하면 결과는 조절점을 잇는 다각형과 같다. 곡선은 반드시 모든 조절점을 지난다. Subdivisions = 구간 하나를 나누는 수 */
    COZYTERRAINEDITOR_API void BuildPolyline(const TArray<FCozyShapePoint>& Points, int32 Subdivisions, FCozyPolyShape& Out);
    /** Bounds·MaxInset을 채운다 (BuildPolyline 뒤에 부름) */
    COZYTERRAINEDITOR_API void Prepare(FCozyPolyShape& Shape);
    /** 테두리까지 거리 · 바깥이면 양수, 안쪽이면 음수 (자기 교차 모양도 짝수-홀수 규칙으로 안팎 판정) */
    COZYTERRAINEDITOR_API float SignedDistance(const FCozyPolyShape& Shape, FVector2D P);
    COZYTERRAINEDITOR_API bool Contains(const FCozyPolyShape& Shape, FVector2D P);
    /** P에서 MaxDistance 안에 있는 가장 가까운 구간 번호(조절점 i → i+1) · 없으면 INDEX_NONE (지도에서 점 끼워 넣기용) */
    COZYTERRAINEDITOR_API int32 FindSpan(const FCozyPolyShape& Shape, FVector2D P, float MaxDistance);
    /** 조절점들의 무게중심 (새 자유 모양의 중심을 정할 때) */
    COZYTERRAINEDITOR_API FVector2D Centroid(const TArray<FVector2D>& Points);

    // ---- 랜덤 (띠·울퉁불퉁 단계 · 작업 3) ----
    /** 같은 씨앗이면 항상 같은 값인 부드러운 2D 잡음 · 대략 -1~1 · Size(m)는 한 덩어리 크기 (여러 겹 합친 펄린 잡음) */
    COZYTERRAINEDITOR_API float Noise2D(FVector2D P, float Size, int32 Seed);
    /** 꺾은선 테두리를 구불구불하게: 간격 Size/6 이하로 다시 나눈 뒤 각 꼭짓점을 바깥 방향으로 잡음×Amount만큼 민다 (BuildPolyline 뒤, Prepare 전에 부름)
     *  Span은 유지 · Amount<=0이면 아무것도 안 함 · 모양 위치에 대해 결정적(같은 입력 → 같은 결과) */
    COZYTERRAINEDITOR_API void ApplyEdgeJitter(FCozyPolyShape& Shape, float Amount, float Size, int32 Seed);
    /** 바깥 모양 안쪽에 띠 Count개를 무작위로 만든다 · 씨앗으로 정한 꼭대기 쪽으로 치우치며 안으로 줄어들고, 서로 겹치지 않게 차례로 안쪽에 놓인다
     *  높이는 PeakHeight를 Count+1로 나눠 고르게(1/(Count+1) … Count/(Count+1)) · 점은 바깥 모양과 같은 기준(중심 기준 m) */
    COZYTERRAINEDITOR_API void GenerateBands(const TArray<FCozyShapePoint>& Outline, int32 Count, float PeakHeight, int32 Seed, TArray<FCozyContourBand>& OutBands);
}
