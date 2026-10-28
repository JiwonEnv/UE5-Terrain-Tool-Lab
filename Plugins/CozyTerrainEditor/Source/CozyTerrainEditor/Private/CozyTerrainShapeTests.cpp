#include "CozyTerrainShape.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace CozyShapeTestUtil
{
    // 중심 0,0 · 한 변 10m 정사각형 (아래 → 오른쪽 → 위 → 왼쪽 변 순서)
    TArray<FCozyShapePoint> Square(bool bSharp)
    {
        TArray<FCozyShapePoint> R;
        for(const FVector2D& P:{FVector2D(-5,-5),FVector2D(5,-5),FVector2D(5,5),FVector2D(-5,5)}){ FCozyShapePoint X; X.Position=P; X.bSharp=bSharp; R.Add(X); }
        return R;
    }
    TArray<FCozyShapePoint> Sharp(const TArray<FVector2D>& Ps)
    {
        TArray<FCozyShapePoint> R;
        for(const FVector2D& P:Ps){ FCozyShapePoint X; X.Position=P; X.bSharp=true; R.Add(X); }
        return R;
    }
    bool HasPoint(const FCozyPolyShape& S,const FVector2D& P)
    {
        for(const FVector2D& V:S.Poly) if(V.Equals(P,1e-6)) return true;
        return false;
    }
    bool Near(double A,double B,double Tol){ return FMath::Abs(A-B)<=Tol; }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyShapePolylineTest,"CozyRealm.TerrainEditor.Shape.Polyline",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCozyShapePolylineTest::RunTest(const FString&)
{
    using namespace CozyShapeTestUtil;
    // 모두 뾰족 → 조절점 다각형 그대로
    FCozyPolyShape S; const TArray<FCozyShapePoint> Sq=Square(true);
    CozyTerrainShape::BuildPolyline(Sq,8,S);
    TestEqual(TEXT("뾰족 사각형 꼭짓점 4개"),S.Poly.Num(),4);
    bool bSame=S.Poly.Num()==4&&S.Span.Num()==4;
    for(int32 i=0;bSame&&i<4;++i) bSame=S.Poly[i]==Sq[i].Position&&S.Span[i]==i;
    TestTrue(TEXT("뾰족 사각형 = 조절점 순서 · 구간 0..3"),bSame);

    // 모두 부드러움 → 4×나눔수 · 조절점을 지남 · 사각형 테두리 2m 안 · 꺾임 작음
    FCozyPolyShape Box; CozyTerrainShape::BuildPolyline(Sq,8,Box);
    FCozyPolyShape R; CozyTerrainShape::BuildPolyline(Square(false),8,R);
    TestEqual(TEXT("부드러운 사각형 꼭짓점 수"),R.Poly.Num(),32);
    TestEqual(TEXT("구간 번호 수 = 꼭짓점 수"),R.Span.Num(),R.Poly.Num());
    bool bThrough=true; for(const FCozyShapePoint& P:Sq) bThrough&=HasPoint(R,P.Position);
    TestTrue(TEXT("부드러운 곡선이 모든 조절점을 지남"),bThrough);
    bool bBand=true; double MaxTurn=0;
    for(int32 i=0;i<R.Poly.Num();++i)
    {
        bBand&=FMath::Abs(CozyTerrainShape::SignedDistance(Box,R.Poly[i]))<=2.f;
        const FVector2D A=R.Poly[(i-1+R.Poly.Num())%R.Poly.Num()], B=R.Poly[i], C=R.Poly[(i+1)%R.Poly.Num()];
        const FVector2D U=B-A, V=C-B;
        MaxTurn=FMath::Max(MaxTurn,FMath::Abs(FMath::RadiansToDegrees(FMath::Atan2(U.X*V.Y-U.Y*V.X,U.X*V.X+U.Y*V.Y))));
    }
    TestTrue(TEXT("곡선이 사각형 테두리 2m 안"),bBand);
    TestTrue(TEXT("부드러운 곡선 · 이웃 선분 꺾임 35도 미만"),MaxTurn<35);
    bool bSpanOrder=true; for(int32 i=1;i<R.Span.Num();++i) bSpanOrder&=R.Span[i]>=R.Span[i-1];
    TestTrue(TEXT("구간 번호가 차례로 늘어남"),bSpanOrder&&R.Span[0]==0&&R.Span.Last()==3);

    // 섞임: 0·1 뾰족, 2·3 부드러움
    TArray<FCozyShapePoint> Mixed=Square(false); Mixed[0].bSharp=Mixed[1].bSharp=true;
    FCozyPolyShape M; CozyTerrainShape::BuildPolyline(Mixed,8,M);
    bThrough=true; for(const FCozyShapePoint& P:Mixed) bThrough&=HasPoint(M,P.Position);
    TestTrue(TEXT("섞인 모양도 모든 조절점을 지남"),bThrough);
    TestTrue(TEXT("뾰족-뾰족 구간은 직선 (꼭짓점 하나)"),M.Poly.Num()>=2&&M.Poly[0]==Mixed[0].Position&&M.Poly[1]==Mixed[1].Position&&M.Span[1]==1);
    TestEqual(TEXT("섞인 모양 꼭짓점 수 (1+8+8+8)"),M.Poly.Num(),25);

    // 나눔수 범위 제한
    FCozyPolyShape Big; CozyTerrainShape::BuildPolyline(Square(false),1000,Big);
    TestEqual(TEXT("나눔수 최대 64"),Big.Poly.Num(),4*64);
    FCozyPolyShape Small; CozyTerrainShape::BuildPolyline(Square(false),0,Small);
    TestEqual(TEXT("나눔수 최소 1"),Small.Poly.Num(),4);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyShapeDistanceTest,"CozyRealm.TerrainEditor.Shape.Distance",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCozyShapeDistanceTest::RunTest(const FString&)
{
    using namespace CozyShapeTestUtil;
    FCozyPolyShape S; CozyTerrainShape::BuildPolyline(Square(true),8,S); CozyTerrainShape::Prepare(S);
    TestTrue(TEXT("중심 -5"),Near(CozyTerrainShape::SignedDistance(S,FVector2D(0,0)),-5,1e-4));
    TestTrue(TEXT("(8,0) +3"),Near(CozyTerrainShape::SignedDistance(S,FVector2D(8,0)),3,1e-4));
    TestTrue(TEXT("(5,0) 테두리 0"),Near(CozyTerrainShape::SignedDistance(S,FVector2D(5,0)),0,1e-4));
    TestTrue(TEXT("모서리 바깥 (7,7) √8"),Near(CozyTerrainShape::SignedDistance(S,FVector2D(7,7)),FMath::Sqrt(8.0),1e-4));
    TestTrue(TEXT("안쪽 (4,-3) -1"),Near(CozyTerrainShape::SignedDistance(S,FVector2D(4,-3)),-1,1e-4));
    TestTrue(TEXT("안쪽 포함"),CozyTerrainShape::Contains(S,FVector2D(1,2)));
    TestFalse(TEXT("바깥 제외"),CozyTerrainShape::Contains(S,FVector2D(6,0)));
    TestTrue(TEXT("범위 계산"),S.Bounds.bIsValid&&S.Bounds.Min==FVector2D(-5,-5)&&S.Bounds.Max==FVector2D(5,5));
    TestTrue(TEXT("사각형 가장 깊은 안쪽 ≈ 5"),Near(S.MaxInset,5,0.15));

    // 오목한 ㄱ자
    FCozyPolyShape L; CozyTerrainShape::BuildPolyline(Sharp({FVector2D(0,0),FVector2D(10,0),FVector2D(10,4),FVector2D(4,4),FVector2D(4,10),FVector2D(0,10)}),8,L);
    CozyTerrainShape::Prepare(L);
    TestTrue(TEXT("ㄱ자 세로 팔 안"),CozyTerrainShape::Contains(L,FVector2D(2,8)));
    TestTrue(TEXT("ㄱ자 가로 팔 안"),CozyTerrainShape::Contains(L,FVector2D(8,2)));
    TestFalse(TEXT("ㄱ자 오목한 곳은 바깥"),CozyTerrainShape::Contains(L,FVector2D(8,8)));
    TestTrue(TEXT("오목한 곳 거리 +4"),Near(CozyTerrainShape::SignedDistance(L,FVector2D(8,8)),4,1e-4));
    TestTrue(TEXT("ㄱ자 가장 깊은 안쪽 0 초과 · 5 이하"),L.MaxInset>1.5f&&L.MaxInset<=5.f);

    // 부드러운 모양도 안팎·깊이 계산
    FCozyPolyShape R; CozyTerrainShape::BuildPolyline(Square(false),16,R); CozyTerrainShape::Prepare(R);
    TestTrue(TEXT("부드러운 사각형 중심 안"),CozyTerrainShape::SignedDistance(R,FVector2D(0,0))<-5.f);
    TestTrue(TEXT("부드러운 사각형 깊이 5~7.1"),R.MaxInset>=5.f&&R.MaxInset<=7.1f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyShapeSpanCentroidTest,"CozyRealm.TerrainEditor.Shape.SpanAndCentroid",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCozyShapeSpanCentroidTest::RunTest(const FString&)
{
    using namespace CozyShapeTestUtil;
    FCozyPolyShape S; CozyTerrainShape::BuildPolyline(Square(true),8,S);
    TestEqual(TEXT("아래 변 = 구간 0"),CozyTerrainShape::FindSpan(S,FVector2D(0,-5.5),1),0);
    TestEqual(TEXT("오른쪽 변 = 구간 1"),CozyTerrainShape::FindSpan(S,FVector2D(5.3,0),1),1);
    TestEqual(TEXT("위 변 = 구간 2"),CozyTerrainShape::FindSpan(S,FVector2D(0,4.8),1),2);
    TestEqual(TEXT("왼쪽 변 = 구간 3"),CozyTerrainShape::FindSpan(S,FVector2D(-5.4,1),1),3);
    TestEqual(TEXT("멀면 없음"),CozyTerrainShape::FindSpan(S,FVector2D(0,0),1),(int32)INDEX_NONE);
    FCozyPolyShape R; CozyTerrainShape::BuildPolyline(Square(false),8,R);
    TestEqual(TEXT("부드러운 곡선 오른쪽 볼록한 곳 = 구간 1"),CozyTerrainShape::FindSpan(R,FVector2D(6.2,0),1),1);

    TestEqual(TEXT("옮긴 사각형 무게중심"),CozyTerrainShape::Centroid({FVector2D(5,15),FVector2D(15,15),FVector2D(15,25),FVector2D(5,25)}),FVector2D(10,20));
    const FVector2D Tri=CozyTerrainShape::Centroid({FVector2D(0,0),FVector2D(6,0),FVector2D(0,3)});
    TestTrue(TEXT("삼각형 무게중심 (2,1)"),Tri.Equals(FVector2D(2,1),1e-6));
    // 점이 몰린 다각형: 평균이 아니라 넓이 기준
    const FVector2D Heavy=CozyTerrainShape::Centroid({FVector2D(0,0),FVector2D(1,0),FVector2D(2,0),FVector2D(3,0),FVector2D(4,0),FVector2D(4,4),FVector2D(0,4)});
    TestTrue(TEXT("한 변에 점이 몰려도 넓이 중심 (2,2)"),Heavy.Equals(FVector2D(2,2),1e-6));
    TestTrue(TEXT("한 줄이면 평균"),CozyTerrainShape::Centroid({FVector2D(0,0),FVector2D(1,1),FVector2D(2,2)}).Equals(FVector2D(1,1),1e-6));
    TestEqual(TEXT("빈 목록은 0"),CozyTerrainShape::Centroid(TArray<FVector2D>()),FVector2D::ZeroVector);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyShapeDegenerateTest,"CozyRealm.TerrainEditor.Shape.Degenerate",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCozyShapeDegenerateTest::RunTest(const FString&)
{
    using namespace CozyShapeTestUtil;
    // 점 2개 → 무효 · 계산 함수가 터지지 않음
    FCozyPolyShape Two; CozyTerrainShape::BuildPolyline(Sharp({FVector2D(0,0),FVector2D(3,0)}),8,Two); CozyTerrainShape::Prepare(Two);
    TestFalse(TEXT("점 2개는 무효"),Two.IsValid());
    TestEqual(TEXT("점 2개는 그대로"),Two.Poly.Num(),2);
    TestTrue(TEXT("무효 모양 거리 = 아주 큼"),CozyTerrainShape::SignedDistance(Two,FVector2D(0,0))>1e8f);
    TestFalse(TEXT("무효 모양은 아무것도 포함 안 함"),CozyTerrainShape::Contains(Two,FVector2D(1,0)));
    TestEqual(TEXT("무효 모양 깊이 0"),Two.MaxInset,0.f);
    FCozyPolyShape Empty; CozyTerrainShape::BuildPolyline(TArray<FCozyShapePoint>(),8,Empty); CozyTerrainShape::Prepare(Empty);
    TestEqual(TEXT("빈 모양 구간 없음"),CozyTerrainShape::FindSpan(Empty,FVector2D(0,0),100),(int32)INDEX_NONE);

    // 겹친 점(연달아 · 끝과 처음) → 길이 0 구간은 건너뜀
    TArray<FCozyShapePoint> Dup=Sharp({FVector2D(0,0),FVector2D(0,0),FVector2D(10,0),FVector2D(10,10),FVector2D(10,10),FVector2D(0,0)});
    FCozyPolyShape D; CozyTerrainShape::BuildPolyline(Dup,8,D); CozyTerrainShape::Prepare(D);
    TestTrue(TEXT("겹친 점을 합쳐 삼각형"),D.IsValid()&&D.Poly.Num()==3);
    bool bNoZero=true; for(int32 i=0;i<D.Poly.Num();++i) bNoZero&=FVector2D::Distance(D.Poly[i],D.Poly[(i+1)%D.Poly.Num()])>1e-3;
    TestTrue(TEXT("길이 0 선분 없음"),bNoZero);
    TestTrue(TEXT("겹친 점 모양 안팎"),CozyTerrainShape::Contains(D,FVector2D(8,2))&&!CozyTerrainShape::Contains(D,FVector2D(2,8)));
    TestEqual(TEXT("겹친 점 뒤 구간 번호는 실제 길이 있는 구간"),CozyTerrainShape::FindSpan(D,FVector2D(5,-0.3),1),1);
    // 부드러운 점이 겹쳐도 NaN 없음
    TArray<FCozyShapePoint> DupRound=Square(false); const FCozyShapePoint Copy=DupRound[1]; DupRound.Insert(Copy,1);
    FCozyPolyShape DR; CozyTerrainShape::BuildPolyline(DupRound,8,DR);
    bool bFinite=DR.Poly.Num()==32; for(const FVector2D& V:DR.Poly) bFinite&=FMath::IsFinite(V.X)&&FMath::IsFinite(V.Y);
    TestTrue(TEXT("부드러운 겹친 점도 정상 곡선"),bFinite);
    // 모든 점이 한 자리
    FCozyPolyShape One; CozyTerrainShape::BuildPolyline(Sharp({FVector2D(1,1),FVector2D(1,1),FVector2D(1,1)}),8,One); CozyTerrainShape::Prepare(One);
    TestFalse(TEXT("한 자리 점들은 무효"),One.IsValid());
    return true;
}
#endif
