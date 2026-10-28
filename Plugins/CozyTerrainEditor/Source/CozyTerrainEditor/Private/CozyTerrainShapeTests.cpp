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

namespace CozyShapeRandomTestUtil
{
    // 중심 0,0 · 반지름 R · N각형
    TArray<FCozyShapePoint> Ngon(int32 N,double R,bool bSharp)
    {
        TArray<FCozyShapePoint> Res;
        for(int32 i=0;i<N;++i){ const double A=2.0*UE_DOUBLE_PI*i/N; FCozyShapePoint X; X.Position=FVector2D(FMath::Cos(A),FMath::Sin(A))*R; X.bSharp=bSharp; Res.Add(X); }
        return Res;
    }
    // 울퉁불퉁한 부드러운 덩어리 (반지름 15~25m · 중심 30,-10)
    TArray<FCozyShapePoint> Blob()
    {
        const double Rs[8]={20,25,17,22,24,15,21,19};
        TArray<FCozyShapePoint> Res;
        for(int32 i=0;i<8;++i){ const double A=2.0*UE_DOUBLE_PI*i/8; FCozyShapePoint X; X.Position=FVector2D(FMath::Cos(A),FMath::Sin(A))*Rs[i]+FVector2D(30,-10); Res.Add(X); }
        return Res;
    }
    bool SamePoly(const FCozyPolyShape& A,const FCozyPolyShape& B)
    {
        if(A.Poly.Num()!=B.Poly.Num()||A.Span!=B.Span) return false;
        for(int32 i=0;i<A.Poly.Num();++i) if(!A.Poly[i].Equals(B.Poly[i],1e-9)) return false;
        return true;
    }
    // 띠가 바깥 모양 안 · 차례로 안쪽인지 (띠 꺾은선 꼭짓점 모두 검사)
    bool Nested(const TArray<FCozyShapePoint>& Outline,const TArray<FCozyContourBand>& Bands)
    {
        FCozyPolyShape Prev; CozyTerrainShape::BuildPolyline(Outline,8,Prev);
        for(const FCozyContourBand& B:Bands)
        {
            FCozyPolyShape S; CozyTerrainShape::BuildPolyline(B.Points,8,S);
            if(!S.IsValid()) return false;
            for(const FVector2D& V:S.Poly) if(!CozyTerrainShape::Contains(Prev,V)) return false;
            Prev=MoveTemp(S);
        }
        return true;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyShapeNoiseTest,"CozyRealm.TerrainEditor.Shape.Noise",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCozyShapeNoiseTest::RunTest(const FString&)
{
    using CozyTerrainShape::Noise2D;
    bool bSame=true, bRange=true, bSmooth=true; double Lo=1e9, Hi=-1e9, Diff=0;
    for(int32 y=0;y<40;++y) for(int32 x=0;x<40;++x)
    {
        const FVector2D P(x*3.7-70.0,y*2.9+1234.5);
        const float V=Noise2D(P,15.f,7);
        bSame&=V==Noise2D(P,15.f,7);
        bRange&=V>=-1.2f&&V<=1.2f;
        Lo=FMath::Min(Lo,double(V)); Hi=FMath::Max(Hi,double(V));
        Diff+=FMath::Abs(double(V)-double(Noise2D(P,15.f,8)));
        bSmooth&=FMath::Abs(double(V)-double(Noise2D(P+FVector2D(0.05,0.05),15.f,7)))<0.1;
    }
    TestTrue(TEXT("같은 씨앗 → 같은 값"),bSame);
    TestTrue(TEXT("값이 -1.2~1.2 안"),bRange);
    TestTrue(TEXT("값이 고르게 퍼짐 (최대-최소 > 0.8)"),Hi-Lo>0.8);
    TestTrue(TEXT("씨앗이 다르면 값이 다름"),Diff/1600.0>0.05);
    TestTrue(TEXT("가까운 점은 비슷한 값 (부드러움)"),bSmooth);
    TestTrue(TEXT("크기 0도 터지지 않음"),FMath::IsFinite(Noise2D(FVector2D(1,2),0.f,1)));
    TestTrue(TEXT("먼 좌표도 정상"),FMath::Abs(Noise2D(FVector2D(1e6,-3e6),10.f,3))<=1.2f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyShapeEdgeJitterTest,"CozyRealm.TerrainEditor.Shape.EdgeJitter",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCozyShapeEdgeJitterTest::RunTest(const FString&)
{
    using namespace CozyShapeRandomTestUtil;
    const TArray<FCozyShapePoint> Gon=Ngon(48,20,true);
    FCozyPolyShape Orig; CozyTerrainShape::BuildPolyline(Gon,8,Orig);

    FCozyPolyShape Zero=Orig; CozyTerrainShape::ApplyEdgeJitter(Zero,0.f,12.f,1);
    TestTrue(TEXT("세기 0 → 그대로"),SamePoly(Zero,Orig));
    FCozyPolyShape Bad; CozyTerrainShape::BuildPolyline(Ngon(2,5,true),8,Bad); const int32 BadNum=Bad.Poly.Num();
    CozyTerrainShape::ApplyEdgeJitter(Bad,3.f,12.f,1);
    TestEqual(TEXT("무효 모양 → 그대로"),Bad.Poly.Num(),BadNum);

    // 거의 0인 세기 = 다시 나누기만 한 것 (기준)
    FCozyPolyShape Base=Orig; CozyTerrainShape::ApplyEdgeJitter(Base,1e-6f,12.f,1);
    double MaxSeg=0; for(int32 i=0;i<Base.Poly.Num();++i) MaxSeg=FMath::Max(MaxSeg,FVector2D::Distance(Base.Poly[i],Base.Poly[(i+1)%Base.Poly.Num()]));
    TestTrue(TEXT("다시 나눈 선분 길이 ≤ 크기/6"),MaxSeg<=12.0/6.0+0.01);
    TestTrue(TEXT("점이 늘어남"),Base.Poly.Num()>Orig.Poly.Num());
    bool bSpan=Base.Span.Num()==Base.Poly.Num(); for(int32 Sp:Base.Span) bSpan&=Sp>=0&&Sp<48;
    TestTrue(TEXT("구간 번호 유지"),bSpan);

    FCozyPolyShape J=Orig; CozyTerrainShape::ApplyEdgeJitter(J,3.f,12.f,1);
    TestEqual(TEXT("꼭짓점 수 = 기준과 같음"),J.Poly.Num(),Base.Poly.Num());
    bool bWithin=J.Poly.Num()==Base.Poly.Num(); double MaxMove=0;
    TArray<double> Off;
    for(int32 i=0;bWithin&&i<J.Poly.Num();++i)
    {
        const double Mv=FVector2D::Distance(J.Poly[i],Base.Poly[i]); MaxMove=FMath::Max(MaxMove,Mv);
        const double Sd=CozyTerrainShape::SignedDistance(Orig,J.Poly[i]); // 바깥 +, 안 -
        bWithin&=Mv<=3.0+1e-3&&FMath::Abs(Sd)<=3.0+1e-3;
        Off.Add(Sd);
    }
    TestTrue(TEXT("움직인 거리 ≤ 세기"),bWithin);
    TestTrue(TEXT("실제로 움직임"),MaxMove>0.3);
    bool bOut=false, bIn=false; for(double O:Off){ bOut|=O>0.1; bIn|=O<-0.1; }
    TestTrue(TEXT("바깥·안쪽 양쪽으로 움직임"),bOut&&bIn);
    // 고리 이음새: 마지막 → 첫 점 변화가 다른 이웃들 변화보다 튀지 않음
    double MaxInner=0; for(int32 i=0;i+1<Off.Num();++i) MaxInner=FMath::Max(MaxInner,FMath::Abs(Off[i+1]-Off[i]));
    TestTrue(TEXT("끝과 처음이 부드럽게 이어짐"),Off.Num()>2&&FMath::Abs(Off[0]-Off.Last())<=2.0*MaxInner+0.01);

    FCozyPolyShape J2=Orig; CozyTerrainShape::ApplyEdgeJitter(J2,3.f,12.f,1);
    TestTrue(TEXT("같은 입력 → 같은 결과"),SamePoly(J,J2));
    FCozyPolyShape J3=Orig; CozyTerrainShape::ApplyEdgeJitter(J3,3.f,12.f,2);
    TestFalse(TEXT("씨앗이 다르면 다른 모양"),SamePoly(J,J3));

    // 시계 방향 모양: 첫 꼭짓점(0도)은 같은 자리 · 둘레 0 → 같은 잡음값 → 바깥/안 방향이 같아야 함
    TArray<FCozyShapePoint> Rev; Rev.Add(Gon[0]); for(int32 i=Gon.Num()-1;i>0;--i) Rev.Add(Gon[i]);
    FCozyPolyShape RO; CozyTerrainShape::BuildPolyline(Rev,8,RO);
    FCozyPolyShape RJ=RO; CozyTerrainShape::ApplyEdgeJitter(RJ,3.f,12.f,1);
    const double RevOff0=RJ.Poly.Num()>0?double(CozyTerrainShape::SignedDistance(RO,RJ.Poly[0])):0.0;
    TestTrue(TEXT("방향과 상관없이 바깥 방향 일정"),Off.Num()>0&&(FMath::Abs(Off[0])<0.05||FMath::Abs(RevOff0-Off[0])<1e-3));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyShapeBandsTest,"CozyRealm.TerrainEditor.Shape.Bands",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCozyShapeBandsTest::RunTest(const FString&)
{
    using namespace CozyShapeRandomTestUtil;
    const TArray<FCozyShapePoint> Outline=Blob();
    TArray<FCozyContourBand> B; CozyTerrainShape::GenerateBands(Outline,3,30.f,5,B);
    TestEqual(TEXT("띠 3개"),B.Num(),3);
    if(B.Num()==3)
    {
        TestEqual(TEXT("높이 7.5"),B[0].Height,7.5f);
        TestEqual(TEXT("높이 15"),B[1].Height,15.f);
        TestEqual(TEXT("높이 22.5"),B[2].Height,22.5f);
    }
    bool bSmooth=true; for(const FCozyContourBand& X:B){ bSmooth&=X.Points.Num()>=3; for(const FCozyShapePoint& P:X.Points) bSmooth&=!P.bSharp; }
    TestTrue(TEXT("띠 점은 부드러운 점"),bSmooth);
    TestTrue(TEXT("띠가 바깥 모양 안 · 차례로 안쪽"),Nested(Outline,B));

    TArray<FCozyContourBand> B2; CozyTerrainShape::GenerateBands(Outline,3,30.f,5,B2);
    bool bSame=B.Num()==B2.Num();
    for(int32 k=0;bSame&&k<B.Num();++k){ bSame&=B[k].Points.Num()==B2[k].Points.Num(); for(int32 i=0;bSame&&i<B[k].Points.Num();++i) bSame&=B[k].Points[i].Position==B2[k].Points[i].Position; }
    TestTrue(TEXT("같은 씨앗 → 같은 띠"),bSame);

    // 씨앗을 바꾸면 꼭대기(가장 안쪽 띠의 중심)가 옮겨 감
    auto Peak=[](const TArray<FCozyContourBand>& Bs){ TArray<FVector2D> Ps; if(Bs.Num()>0) for(const FCozyShapePoint& P:Bs.Last().Points) Ps.Add(P.Position); return CozyTerrainShape::Centroid(Ps); };
    const FVector2D P0=Peak(B); bool bMoved=false, bAllNested=true;
    for(int32 Seed=6;Seed<12;++Seed)
    {
        TArray<FCozyContourBand> Bs; CozyTerrainShape::GenerateBands(Outline,3,30.f,Seed,Bs);
        bAllNested&=Bs.Num()==3&&Nested(Outline,Bs);
        bMoved|=FVector2D::Distance(Peak(Bs),P0)>1.0;
    }
    TestTrue(TEXT("씨앗마다 차례로 안쪽"),bAllNested);
    TestTrue(TEXT("씨앗을 바꾸면 꼭대기가 옮겨 감"),bMoved);

    // 개수 범위 · 오목한 ㄱ자
    TArray<FCozyContourBand> Many; CozyTerrainShape::GenerateBands(Outline,20,90.f,1,Many);
    TestEqual(TEXT("띠 최대 8개"),Many.Num(),8);
    TestTrue(TEXT("8개도 차례로 안쪽"),Nested(Outline,Many));
    if(Many.Num()==8) TestEqual(TEXT("8개 맨 안쪽 높이 80"),Many.Last().Height,80.f);
    TArray<FCozyContourBand> Few; CozyTerrainShape::GenerateBands(Outline,0,10.f,1,Few);
    TestEqual(TEXT("띠 최소 1개"),Few.Num(),1);
    TArray<FCozyShapePoint> L;
    for(const FVector2D& P:{FVector2D(0,0),FVector2D(40,0),FVector2D(40,16),FVector2D(16,16),FVector2D(16,40),FVector2D(0,40)}){ FCozyShapePoint X; X.Position=P; X.bSharp=true; L.Add(X); }
    TArray<FCozyContourBand> LB; CozyTerrainShape::GenerateBands(L,3,30.f,3,LB);
    TestTrue(TEXT("ㄱ자 모양도 차례로 안쪽"),LB.Num()==3&&Nested(L,LB));

    // 점이 모자라거나 한 자리 → 비어 있음
    TArray<FCozyContourBand> E; CozyTerrainShape::GenerateBands(Ngon(2,10,false),3,30.f,1,E);
    TestEqual(TEXT("점 2개 → 띠 없음"),E.Num(),0);
    TArray<FCozyShapePoint> Same; for(int32 i=0;i<4;++i){ FCozyShapePoint X; X.Position=FVector2D(3,3); Same.Add(X); }
    CozyTerrainShape::GenerateBands(Same,3,30.f,1,E);
    TestEqual(TEXT("한 자리 점 → 띠 없음"),E.Num(),0);
    return true;
}
#endif
