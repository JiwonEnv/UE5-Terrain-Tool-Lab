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
}
