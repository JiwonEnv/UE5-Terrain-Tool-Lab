#include "CozyTerrainLayout.h"
#include "CozyTerrainShape.h"
#include "ProceduralMeshComponent.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeEdit.h"
#include "LandscapeEditLayer.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "ScopedTransaction.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"

bool FCozyTerrainRecipe::Validate(FString& Reason) const
{
    auto Finite=[](double V){return FMath::IsFinite(V);};
    if(Version!=1||Components<1||Components>8||!Finite(MapSize.X)||!Finite(MapSize.Y)||MapSize.X<63||MapSize.Y<63||MapSize.X>1008||MapSize.Y>1008||!Finite(BaseHeight)||FMath::Abs(BaseHeight)>200||Landforms.Num()>128||Routes.Num()>64)
    {Reason=TEXT("영역은 63~1008m, 구획은 1~8, 기본 높이는 ±200m 범위여야 합니다.");return false;}
    for(const auto& F:Landforms)
    {
        if(!Finite(F.Center.X)||!Finite(F.Center.Y)||!Finite(F.Size.X)||!Finite(F.Size.Y)||F.Size.X<2||F.Size.Y<2||F.Size.X>2000||F.Size.Y>2000||!Finite(F.Height)||FMath::Abs(F.Height)>200||!Finite(F.Blend)||F.Blend<1||F.Blend>200||!Finite(F.Rotation)||!Finite(F.PeakFlat)||F.PeakFlat<0||F.PeakFlat>90||uint8(F.Type)>1)
        {Reason=TEXT("평지·산의 크기·높이·주변 연결 수치를 확인하세요.");return false;}
        if(uint8(F.Shape)>1||uint8(F.EdgeMode)>1||!Finite(F.SlopeAngle)||F.SlopeAngle<5||F.SlopeAngle>85)
        {Reason=TEXT("평지·산의 모양 종류와 경사 각도(5~85도)를 확인하세요.");return false;}
        if(F.Shape==ECozyLandformShape::Free&&(F.Points.Num()<3||F.Points.Num()>256))
        {Reason=TEXT("자유 모양은 점이 3~256개여야 합니다.");return false;}
        for(const auto& Pt:F.Points) if(!Finite(Pt.Position.X)||!Finite(Pt.Position.Y)||FMath::Abs(Pt.Position.X)>2000||FMath::Abs(Pt.Position.Y)>2000)
        {Reason=TEXT("자유 모양 점의 위치를 확인하세요.");return false;}
        if(!Finite(F.NoiseAmount)||F.NoiseAmount<0||F.NoiseAmount>30||!Finite(F.NoiseSize)||F.NoiseSize<2||F.NoiseSize>300||!Finite(F.EdgeJitter)||F.EdgeJitter<0||F.EdgeJitter>30||!Finite(F.EdgeJitterSize)||F.EdgeJitterSize<2||F.EdgeJitterSize>200)
        {Reason=TEXT("울퉁불퉁(0~30m·크기 2~300m)과 테두리 들쭉날쭉(0~30m·간격 2~200m) 수치를 확인하세요.");return false;}
        // 안쪽 띠는 자유 모양 산에서만 쓴다 (다른 모양에 남아 있으면 무시하고 거부하지 않음)
        if(F.Type!=ECozyLandform::Mountain||F.Shape!=ECozyLandformShape::Free) continue;
        if(F.Bands.Num()>32){Reason=TEXT("안쪽 띠는 32개까지입니다.");return false;}
        for(const auto& Band:F.Bands)
        {
            if(Band.Points.Num()<3||Band.Points.Num()>256||!Finite(Band.Height)||FMath::Abs(Band.Height)>200)
            {Reason=TEXT("안쪽 띠는 점이 3~256개, 높이는 ±200m여야 합니다.");return false;}
            for(const auto& Pt:Band.Points) if(!Finite(Pt.Position.X)||!Finite(Pt.Position.Y)||FMath::Abs(Pt.Position.X)>2000||FMath::Abs(Pt.Position.Y)>2000)
            {Reason=TEXT("안쪽 띠 점의 위치를 확인하세요.");return false;}
        }
    }
    for(const auto& R:Routes)
    {
        if(!Finite(R.Width)||R.Width<2||R.Width>80||!Finite(R.Blend)||R.Blend<1||R.Blend>80||R.Points.Num()>256)
        {Reason=TEXT("길 폭은 2~80m, 연결 폭은 1~80m 범위여야 합니다.");return false;}
        for(const auto& P:R.Points) if(P.ContainsNaN()||!Finite(P.X)||!Finite(P.Y)||!Finite(P.Z)||FMath::Abs(P.Z)>400)
        {Reason=TEXT("길 경유점의 위치와 높이를 확인하세요.");return false;}
    }
    return true;
}

// 자유 모양 꺾은선은 평가 한 바퀴(미리보기·적용)마다 한 번만 만든다 · 점·띠·들쭉날쭉·씨앗이 그대로면 다시 만들지 않음
struct FCozyPreparedShapes
{
    static constexpr int MaxBands=32;
    struct FEntry
    {
        // 바깥 모양 · 안쪽 띠(자유 모양 산만) · 띠 높이 (높이는 매번 새로 복사 · 모양과 무관)
        FCozyPolyShape Outline; TArray<FCozyPolyShape> Bands; TArray<float> BandHeights; bool bAnyBand=false;
        // 다시 만들지 판단하는 열쇠
        bool bBuilt=false,bMountain=false; float Jitter=0,JitterSize=0; int32 Seed=0;
        TArray<FCozyShapePoint> Points; TArray<TArray<FCozyShapePoint>> BandPoints;
    };
    TArray<FEntry> Entries;
    static bool Same(const TArray<FCozyShapePoint>& A,const TArray<FCozyShapePoint>& B){if(A.Num()!=B.Num())return false;for(int I=0;I<A.Num();++I)if(A[I].Position!=B[I].Position||A[I].bSharp!=B[I].bSharp)return false;return true;}
    // 꺾은선 → 테두리 들쭉날쭉(Prepare 전) → 준비 · K는 선마다 다른 씨앗 (바깥 모양 0, 띠 i는 i+1)
    static void Build(const FCozyLandform& F,const TArray<FCozyShapePoint>& Pts,int32 K,FCozyPolyShape& Out)
    {
        Out=FCozyPolyShape();
        if(Pts.Num()<3||Pts.Num()>256) return;
        CozyTerrainShape::BuildPolyline(Pts,12,Out);
        if(F.EdgeJitter>0&&Out.IsValid()) CozyTerrainShape::ApplyEdgeJitter(Out,F.EdgeJitter,FMath::Max(F.EdgeJitterSize,2.f),int32(uint32(F.RandomSeed)+uint32(K)));
        CozyTerrainShape::Prepare(Out);
    }
    void Update(const FCozyTerrainRecipe& R)
    {
        const int N=R.Landforms.Num(); Entries.SetNum(N);
        for(int I=0;I<N;++I)
        {
            const auto& F=R.Landforms[I]; FEntry& E=Entries[I];
            if(F.Shape!=ECozyLandformShape::Free||F.Points.Num()<3||F.Points.Num()>256){if(E.bBuilt)E=FEntry();continue;}
            // 띠는 자유 모양 산에만 (평지·타원은 무시)
            const bool bMountain=F.Type==ECozyLandform::Mountain;
            const int NB=bMountain?FMath::Min(F.Bands.Num(),MaxBands):0;
            bool bSame=E.bBuilt&&E.bMountain==bMountain&&E.Jitter==F.EdgeJitter&&E.JitterSize==F.EdgeJitterSize&&E.Seed==F.RandomSeed&&E.BandPoints.Num()==NB&&Same(E.Points,F.Points);
            for(int B=0;bSame&&B<NB;++B) bSame=Same(E.BandPoints[B],F.Bands[B].Points);
            if(!bSame)
            {
                E=FEntry();E.bBuilt=true;E.bMountain=bMountain;E.Jitter=F.EdgeJitter;E.JitterSize=F.EdgeJitterSize;E.Seed=F.RandomSeed;E.Points=F.Points;
                Build(F,F.Points,0,E.Outline);
                E.Bands.SetNum(NB);E.BandPoints.SetNum(NB);
                for(int B=0;B<NB;++B){E.BandPoints[B]=F.Bands[B].Points;Build(F,F.Bands[B].Points,B+1,E.Bands[B]);E.bAnyBand|=E.Bands[B].IsValid();}
            }
            E.BandHeights.SetNum(NB);
            for(int B=0;B<NB;++B) E.BandHeights[B]=F.Bands[B].Height;
        }
    }
    const FEntry* GetEntry(const FCozyTerrainRecipe& R,int I) const{return R.Landforms[I].Shape==ECozyLandformShape::Free&&Entries.IsValidIndex(I)&&Entries[I].Outline.IsValid()?&Entries[I]:nullptr;}
    const FCozyPolyShape* Get(const FCozyTerrainRecipe& R,int I) const{const FEntry* E=GetEntry(R,I);return E?&E->Outline:nullptr;}
};

// 단일 빌드(unity)에서 다른 파일 이름과 섞이지 않게 이름 공간을 둔다
namespace CozyTerrainEval
{
    double SlopeTan(const FCozyLandform& F){return FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(double(F.SlopeAngle),5.,85.)));}
    // 타원 정규화 거리 (1이 테두리)
    double EllipseNorm(const FCozyLandform& F,FVector2D Q){return FMath::Sqrt(FMath::Square(Q.X/FMath::Max(1.,F.Size.X*.5))+FMath::Square(Q.Y/FMath::Max(1.,F.Size.Y*.5)));}
    // 경사 각도 방식의 타원: (정규화 거리−1)×짧은 반지름을 테두리까지 거리(m)로 근사 · 바깥 양수, 안쪽 음수
    double EllipseHalf(const FCozyLandform& F){return FMath::Min(FMath::Max(1.,F.Size.X*.5),FMath::Max(1.,F.Size.Y*.5));}
    // 꼭대기 넓이 T 안은 꼭대기 높이 그대로, 그 바깥부터 줄어듦 (T가 클수록 옆면이 가파름) · D는 1이 테두리
    float PeakProfile(const FCozyLandform& F,double D){const double T=FMath::Clamp(F.PeakFlat,0.f,90.f)/100.,R=FMath::Max(0.,D-T)/(1-T);return float(F.Height*FMath::Exp(-2*R*R));}
    // 경사 각도 산: 테두리가 산기슭, 안쪽으로 정한 각도로 곧게 오르다가 높이에 닿으면 평평한 꼭대기
    float SlopeBump(const FCozyLandform& F,double Depth){return float(FMath::Sign(double(F.Height))*FMath::Min(double(FMath::Abs(F.Height)),FMath::Max(0.,Depth)*SlopeTan(F)));}
    // 경사 각도 평지: 안은 목표 높이, 밖은 정한 각도의 곧은 비탈로 주변 높이까지 내려가거나 올라감
    float Terrace(float B,float T,double D,double S){if(D<=0)return T;return T>=B?FMath::Max(B,float(T-D*S)):FMath::Min(B,float(T+D*S));}
    // 울퉁불퉁 잡음 (설계 좌표 P 기준 · 모양을 돌려도 무늬는 그대로) · 세기 0이면 0
    float Noise(const FCozyLandform& F,FVector2D P){return F.NoiseAmount>0?F.NoiseAmount*CozyTerrainShape::Noise2D(P,FMath::Max(F.NoiseSize,2.f),F.RandomSeed):0.f;}
    // 잡음이 넘을 수 있는 높이 여유 (잡음은 대략 ±1 · 넉넉히 1.5배)
    double NoiseReach(const FCozyLandform& F){return F.NoiseAmount>0?F.NoiseAmount*1.5:0.;}

    // 안쪽 띠가 있는 자유 모양 산 · Q는 바깥 모양 안(Depth = 바깥 테두리까지 안쪽 거리) · 높이는 산기슭 0 기준
    // 수준 = 바깥 모양(0m) + 띠들 · A = Q를 품은 수준 중 가장 높은 것(구덩이면 가장 깊은 것) · B = A보다 높은데 Q를 품지 않은 수준 중 가장 가까운 것
    // B가 있으면 A 테두리 → B 테두리 사이를 잇고, 없으면 A 안에서 꼭대기 높이(Height)까지 오른다
    float BandBump(const FCozyLandform& F,const FCozyPreparedShapes::FEntry& E,FVector2D Q,double Depth,bool bAngle)
    {
        const double Sg=F.Height<0?-1.:1.;
        const int NB=E.Bands.Num();
        double SD[FCozyPreparedShapes::MaxBands]; bool bKnown[FCozyPreparedShapes::MaxBands];
        int A=INDEX_NONE; double AKey=0,ADepth=Depth; float AH=0;
        for(int B=0;B<NB;++B)
        {
            bKnown[B]=false; const FCozyPolyShape& Band=E.Bands[B];
            // 상자 밖(테두리 위는 안으로 봄)이면 품지 않음
            if(!Band.IsValid()||Band.Bounds.ComputeSquaredDistanceToPoint(Q)>0) continue;
            SD[B]=CozyTerrainShape::SignedDistance(Band,Q);bKnown[B]=true;
            const double Key=Sg*E.BandHeights[B];
            if(SD[B]<=0&&Key>AKey){A=B;AKey=Key;ADepth=-SD[B];AH=E.BandHeights[B];}
        }
        int Next=INDEX_NONE; double NDist=DBL_MAX;
        for(int B=0;B<NB;++B)
        {
            const FCozyPolyShape& Band=E.Bands[B];
            if(B==A||!Band.IsValid()||Sg*E.BandHeights[B]<=AKey) continue;
            // 상자까지 거리가 지금 가장 가까운 것보다 멀면 건너뜀
            if(!bKnown[B]){if(Band.Bounds.ComputeSquaredDistanceToPoint(Q)>=NDist*NDist) continue;SD[B]=CozyTerrainShape::SignedDistance(Band,Q);bKnown[B]=true;}
            if(SD[B]>0&&SD[B]<NDist){Next=B;NDist=SD[B];}
        }
        if(Next!=INDEX_NONE)
        {
            const double T=ADepth+NDist>1e-9?ADepth/(ADepth+NDist):0.;
            // 경사 각도: 띠 사이 곧은 비탈 · 연결 폭: 부드럽게
            return FMath::Lerp(AH,E.BandHeights[Next],float(bAngle?T:FMath::SmoothStep(0.,1.,T)));
        }
        // 가장 안쪽 띠 안: 그 띠의 가장 깊은 곳에서 꼭대기 · 꼭대기 넓이만큼 평평하게
        if(Sg*F.Height<=Sg*AH) return AH;
        const FCozyPolyShape& Inner=A==INDEX_NONE?E.Outline:E.Bands[A];
        const double TopFlat=FMath::Clamp(F.PeakFlat,0.f,90.f)/100.;
        const double X=FMath::Min(1.,ADepth/FMath::Max(double(Inner.MaxInset),.01)/(1-TopFlat));
        return FMath::Lerp(AH,F.Height,float(bAngle?X:FMath::SmoothStep(0.,1.,X)));
    }

    // 길은 마지막에 (평지·산 위를 덮음)
    float EvaluateRoutes(const FCozyTerrainRecipe& Rec,FVector2D P,float H)
    {
        float BestWeight=0,BestHeight=H; double BestDistance=DBL_MAX;
        for(const auto& R:Rec.Routes) for(int I=1;I<R.Points.Num();++I)
        {
            const FVector2D A(R.Points[I-1]),B(R.Points[I]),D=B-A;
            const double T=FMath::Clamp(FVector2D::DotProduct(P-A,D)/FMath::Max(.00001,D.SizeSquared()),0.,1.);
            const double Distance=FVector2D::Distance(P,A+D*T);
            const float W=1-FMath::SmoothStep(R.Width*.5f,R.Width*.5f+R.Blend,float(Distance));
            if(W>BestWeight||(W==BestWeight&&Distance<BestDistance))
            {BestWeight=W;BestDistance=Distance;BestHeight=FMath::Lerp(R.Points[I-1].Z,R.Points[I].Z,T);}
        }
        return FMath::Lerp(H,BestHeight,BestWeight);
    }

    float EvaluateRecipe(const FCozyTerrainRecipe& Rec,const FCozyPreparedShapes& S,FVector2D P,float Base)
    {
        // 산끼리 겹치면 높이를 더하지 않고 부드럽게 높은 쪽을 따른다 (더하면 겹친 곳이 두 배로 솟았음)
        // 이음매 폭 K는 높이에 비례 (산에서 먼 평지에서는 0이라 바닥이 들뜨지 않음)
        auto SmoothMax=[](float A,float B){const float K=FMath::Min(4.f,.25f*FMath::Max(A,B));if(K<=KINDA_SMALL_NUMBER)return FMath::Max(A,B);const float W=FMath::Max(K-FMath::Abs(A-B),0.f)/K;return FMath::Max(A,B)+W*W*K*.25f;};
        const auto& Landforms=Rec.Landforms;
        float Up=0,Down=0;
        for(int I=0;I<Landforms.Num();++I)
        {
            const auto& F=Landforms[I];
            if(F.Type!=ECozyLandform::Mountain) continue;
            const FVector2D Q=(P-F.Center).GetRotated(-F.Rotation);
            const bool bAngle=F.EdgeMode==ECozyEdgeMode::Angle; float Bump;
            if(F.Shape==ECozyLandformShape::Free)
            {
                const FCozyPreparedShapes::FEntry* E=S.GetEntry(Rec,I);
                const FCozyPolyShape* Poly=E?&E->Outline:nullptr;
                if(!Poly||Poly->MaxInset<=KINDA_SMALL_NUMBER) continue;
                // 안쪽 띠가 있으면 산기슭(바깥 모양) 밖은 0
                const bool bBands=E->bAnyBand;
                // 각도 방식·띠 있는 산은 안쪽만, 폭 방식은 깊이의 3배 밖이면 사실상 0 → 상자 밖은 건너뜀
                if(!Poly->Bounds.ExpandBy(bAngle||bBands?.01:3.*Poly->MaxInset).IsInside(Q)) continue;
                const double D=CozyTerrainShape::SignedDistance(*Poly,Q);
                if(bBands)
                {
                    if(D>0) continue;
                    // 띠가 비탈을 정하므로 경사 각도 수치는 안쪽에서 쓰지 않음
                    Bump=BandBump(F,*E,Q,-D,bAngle);
                }
                // 폭 방식: 가장 깊은 곳 0 · 테두리 1 (타원 정규화 거리와 같은 뜻)
                else Bump=bAngle?SlopeBump(F,-D):PeakProfile(F,1+D/Poly->MaxInset);
            }
            else
            {
                const double D=EllipseNorm(F,Q);
                Bump=bAngle?SlopeBump(F,(1-D)*EllipseHalf(F)):PeakProfile(F,D);
            }
            // 울퉁불퉁: 산 높이에 비례해 섞음 → 산기슭(0m)에서는 0
            if(F.NoiseAmount>0&&Bump!=0) Bump+=Noise(F,P)*FMath::Clamp(FMath::Abs(Bump)/FMath::Max(FMath::Abs(F.Height),.01f),0.f,1.f);
            if(Bump>=0)Up=SmoothMax(Up,Bump);else Down=-SmoothMax(-Down,-Bump);
        }
        float H=Base+Up+Down;
        for(int I=0;I<Landforms.Num();++I)
        {
            const auto& F=Landforms[I];
            if(F.Type==ECozyLandform::Mountain) continue;
            const FVector2D Q=(P-F.Center).GetRotated(-F.Rotation);
            const bool bAngle=F.EdgeMode==ECozyEdgeMode::Angle;
            // 울퉁불퉁: 평지 목표 높이에 잡음을 더함 (연결 폭 방식은 섞는 비율이 곧 가림막 · 영향이 닿는 곳에서만 계산)
            auto Target=[&]{return F.NoiseAmount>0?F.Height+Noise(F,P):F.Height;};
            double D;
            if(F.Shape==ECozyLandformShape::Free)
            {
                const FCozyPolyShape* Poly=S.Get(Rec,I); if(!Poly) continue;
                // 영향이 닿는 거리 밖이면 높이 그대로
                const double Reach=bAngle?(FMath::Abs(F.Height-H)+NoiseReach(F))/SlopeTan(F):F.Blend;
                if(!Poly->Bounds.ExpandBy(Reach+.01).IsInside(Q)) continue;
                D=CozyTerrainShape::SignedDistance(*Poly,Q);
                if(!bAngle){const float W=D<=0?1.f:1-FMath::SmoothStep(0.f,F.Blend,float(D));H=FMath::Lerp(H,W>0?Target():F.Height,W);continue;}
            }
            else
            {
                const double N=EllipseNorm(F,Q);
                if(!bAngle){const float W=float(1-FMath::SmoothStep(1.,1.+F.Blend/FMath::Max(1.,FMath::Min(F.Size.X,F.Size.Y)*.5),N));H=FMath::Lerp(H,W>0?Target():F.Height,W);continue;}
                D=(N-1)*EllipseHalf(F);
            }
            // 잡음이 있어도 비탈이 닿지 않는 먼 곳은 잡음 계산 없이 그대로
            if(F.NoiseAmount>0&&D>0&&D*SlopeTan(F)>=FMath::Abs(F.Height-H)+NoiseReach(F)) continue;
            H=Terrace(H,Target(),D,SlopeTan(F));
        }
        return EvaluateRoutes(Rec,P,H);
    }
}

float FCozyTerrainRecipe::Evaluate(FVector2D P,float Base) const
{
    // 한 점만 계산할 때(검사 등)는 그 자리에서 모양을 만든다 · 반복 계산은 ACozyTerrainLayout의 캐시를 쓴다
    FCozyPreparedShapes S;S.Update(*this);return CozyTerrainEval::EvaluateRecipe(*this,S,P,Base);
}

ACozyTerrainLayout::ACozyTerrainLayout()
{
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("편집 영역 중심"));
    Preview=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("적용 전 미리보기"));
    Preview->SetupAttachment(RootComponent); Preview->SetCollisionEnabled(ECollisionEnabled::NoCollision);Preview->SetHiddenInGame(true);
    Preview->CastShadow=false; Preview->bIsEditorOnly=true;
    Preview->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial")));
}
void ACozyTerrainLayout::PostEditChangeProperty(FPropertyChangedEvent& E)
{
    Super::PostEditChangeProperty(E);
    if(E.MemberProperty&&E.MemberProperty->GetFName()==GET_MEMBER_NAME_CHECKED(ACozyTerrainLayout,Recipe)){bHasUnappliedChanges=true;bShowPreview=true;}
    RebuildPreview();
}
void ACozyTerrainLayout::PostEditUndo(){Super::PostEditUndo();RebuildPreview();}
void ACozyTerrainLayout::PostLoad()
{
    Super::PostLoad();
    // 적용 기록이 생기기 전에 저장한 설계: 적용된 상태로 저장됐으면 지금 설계를 적용분으로 본다 (미리보기 기준)
    if(!bHasApplied&&EditLayerGuid.IsValid()&&!bHasUnappliedChanges){AppliedRecipe=Recipe;bHasApplied=true;}
}

float ACozyTerrainLayout::SampleApplied(FVector2D P) const
{
    if(bHasApplied) return SampleWith(AppliedRecipe,P);
    // 아직 적용 전: 연결 당시 원래 높이 그대로 (새로 만들 지형이면 0)
    return BaselineHeight(P);
}

float ACozyTerrainLayout::Sample(FVector2D P) const{return SampleWith(Recipe,P);}
float ACozyTerrainLayout::SampleWith(const FCozyTerrainRecipe& R,FVector2D P) const{return SampleFast(R,PreparedFor(R),P);}
float ACozyTerrainLayout::SampleFast(const FCozyTerrainRecipe& R,const FCozyPreparedShapes& S,FVector2D P) const{return CozyTerrainEval::EvaluateRecipe(R,S,P,BaselineHeight(P)+R.BaseHeight);}
const FCozyPreparedShapes& ACozyTerrainLayout::PreparedFor(const FCozyTerrainRecipe& R) const
{
    // 지금 설계와 적용분은 따로 보관 (미리보기가 둘을 번갈아 계산해도 다시 만들지 않게) · 점이 바뀐 것만 새로 만듦
    TSharedPtr<FCozyPreparedShapes>& Cache=&R==&Recipe?PreparedNow:PreparedApplied;
    if(!Cache.IsValid())Cache=MakeShared<FCozyPreparedShapes>();
    Cache->Update(R);return *Cache;
}

float ACozyTerrainLayout::BaselineHeight(FVector2D P) const
{
    float Base=0;
    const int W=GridMax.X-GridMin.X+1,H=GridMax.Y-GridMin.Y+1;
    if(Baseline.Num()==W*H&&W>1&&H>1&&BoundSize.X>0&&BoundSize.Y>0)
    {
        const double GX=FMath::Clamp((P.X/BoundSize.X+.5)*(W-1),0.,double(W-1));
        const double GY=FMath::Clamp((P.Y/BoundSize.Y+.5)*(H-1),0.,double(H-1));
        const int X=FMath::FloorToInt(GX),Y=FMath::FloorToInt(GY),X2=FMath::Min(X+1,W-1),Y2=FMath::Min(Y+1,H-1);
        // 원래 높이 + 지금까지 굳힌 높이 (굳힌 뒤 새 설계는 바뀐 지형 위에서 시작)
        const bool bBaked=Baked.Num()==Baseline.Num();
        auto At=[&](int I){return double(Baseline[I])+(bBaked?double(Baked[I])-32768:0.);};
        const double Raw=FMath::Lerp(FMath::Lerp(At(Y*W+X),At(Y*W+X2),GX-X),FMath::Lerp(At(Y2*W+X),At(Y2*W+X2),GX-X),GY-Y);
        Base=((Raw-32768)/128*CapturedTransform.GetScale3D().Z+CapturedTransform.GetLocation().Z-GetActorLocation().Z)/100;
    }
    return Base;
}

void ACozyTerrainLayout::RebuildPreview()
{
    FString Reason;
    if(!Recipe.Validate(Reason)){Preview->ClearAllMeshSections();return;}
    // 실제 지형과 높이가 달라지는 곳만 그린다 (전체를 덮으면 실제 지형 위에 하얗게 겹쳐 알아보기 어려웠음)
    // 색: 올라가는 곳 주황 · 내려가는 곳 파랑 · 급경사 빨강
    constexpr int N=65; constexpr float ChangedMeters=.05f;
    TArray<FVector> V,Normals; TArray<int32> Indices; TArray<FVector2D> UV; TArray<FLinearColor> Colors; TArray<FProcMeshTangent> Tangents;
    TArray<bool> Changed; V.Reserve(N*N); Colors.Reserve(N*N); Changed.Reserve(N*N);
    // 자유 모양은 여기서 한 번만 준비하고 아래 반복에서 그대로 씀
    const FCozyPreparedShapes& Now=PreparedFor(Recipe);const FCozyPreparedShapes* Old=bHasApplied?&PreparedFor(AppliedRecipe):nullptr;
    auto SampleNow=[&](FVector2D Q){return SampleFast(Recipe,Now,Q);};
    auto SampleOld=[&](FVector2D Q){return Old?SampleFast(AppliedRecipe,*Old,Q):BaselineHeight(Q);};
    for(int Y=0;Y<N;++Y) for(int X=0;X<N;++X)
    {
        FVector2D P((double(X)/(N-1)-.5)*Recipe.MapSize.X,(double(Y)/(N-1)-.5)*Recipe.MapSize.Y);
        const float Z=SampleNow(P),Delta=Z-SampleOld(P);
        V.Add(FVector(P.X*100,P.Y*100,Z*100+15));
        const float DX=SampleNow(P+FVector2D(.5,0))-SampleNow(P-FVector2D(.5,0));
        const float DY=SampleNow(P+FVector2D(0,.5))-SampleNow(P-FVector2D(0,.5));
        Normals.Add(FVector(-DX,-DY,1).GetSafeNormal());
        UV.Add(FVector2D(double(X)/(N-1),double(Y)/(N-1)));
        FLinearColor C=Delta>=0?FLinearColor(.95,.55,.15):FLinearColor(.2,.45,.95);
        if(FMath::RadiansToDegrees(FMath::Atan(FMath::Sqrt(DX*DX+DY*DY)))>SlopeLimit) C=FLinearColor(.9,.16,.08);
        Colors.Add(C); Changed.Add(FMath::Abs(Delta)>ChangedMeters);
    }
    for(int Y=0;Y<N-1;++Y) for(int X=0;X<N-1;++X)
    {
        const int A=Y*N+X;
        if(Changed[A]||Changed[A+1]||Changed[A+N]||Changed[A+N+1]) Indices.Append({A,A+N,A+1,A+1,A+N,A+N+1});
    }
    if(Indices.IsEmpty()){Preview->ClearAllMeshSections();if(GEditor)GEditor->RedrawLevelEditingViewports();return;}
    Preview->CreateMeshSection_LinearColor(0,V,Indices,Normals,UV,Colors,Tangents,false);
    Preview->SetVisibility(bShowPreview); if(GEditor) GEditor->RedrawLevelEditingViewports();
}

bool ACozyTerrainLayout::BindLandscape(ALandscape* L,FString& Result)
{
    if(!L||L->GetWorld()!=GetWorld()||EditLayerGuid.IsValid()) {Result=TEXT("현재 레벨의 Landscape를 선택하세요. 이미 연결된 설계는 새 설계에서 다시 연결하세요.");return false;}
    ULandscapeInfo* Info=L->GetLandscapeInfo();
    if(!Info||!L->GetActorRotation().IsNearlyZero()||L->GetActorScale3D().GetMin()<=0||!Info->GetLandscapeExtent(GridMin.X,GridMin.Y,GridMax.X,GridMax.Y))
    {Result=TEXT("양수 배율·회전 0인 로드된 Landscape가 필요합니다.");return false;}
    const int64 Count=int64(GridMax.X-GridMin.X+1)*(GridMax.Y-GridMin.Y+1);
    if(Count<=0||Count>1100000){Result=TEXT("한 번에 최대 약 100만 높이점까지 연결합니다. 분할 월드는 별도 검증이 필요합니다.");return false;}
    Target=L; CapturedTransform=L->LandscapeActorToWorld();
    const FVector Min=CapturedTransform.TransformPosition(FVector(GridMin.X,GridMin.Y,0)),Max=CapturedTransform.TransformPosition(FVector(GridMax.X,GridMax.Y,0));
    BoundCenter=(Min+Max)*.5; BoundSize=FVector2D(Max-Min)/100;
    Recipe.MapSize=BoundSize; SetActorLocation(BoundCenter);
    if(!ReadTarget(Baseline,Result)){Target=nullptr;return false;}
    Baked.Reset();BakeUndoLayer.Reset();bCanUndoBake=false;
    GroundMaterial=L->GetLandscapeMaterial(); RebuildPreview();
    Result=TEXT("현재 지형을 기준으로 연결했습니다. 원본 편집 레이어는 유지하고 새 전용 레이어로 적용합니다."); return true;
}

bool ACozyTerrainLayout::ReadTarget(TArray<uint16>& Data,FString& Result) const
{
    if(!IsValid(Target)||Target->GetWorld()!=GetWorld()||!Target->GetLandscapeInfo()) {Result=TEXT("대상 지형이 현재 레벨에 없습니다.");return false;}
    FIntPoint Min,Max;
    if(!Target->GetLandscapeInfo()->GetLandscapeExtent(Min.X,Min.Y,Max.X,Max.Y)||Min!=GridMin||Max!=GridMax)
    {Result=TEXT("지형의 구획 범위가 변경됐습니다. 새 설계에서 다시 연결하세요.");return false;}
    Target->ForceUpdateLayersContent();
    Data.Init(32768,(GridMax.X-GridMin.X+1)*(GridMax.Y-GridMin.Y+1));
    FHeightmapAccessor<false> Access(Target->GetLandscapeInfo()); Access.SetEditLayer(FGuid());
    Access.GetDataFast(GridMin.X,GridMin.Y,GridMax.X,GridMax.Y,Data.GetData());return true;
}

bool ACozyTerrainLayout::WriteLayer(const TArray<uint16>& Data,FString& Result)
{
    if(!Target||!Target->GetEditLayer(EditLayerGuid)){Result=TEXT("전용 지형 편집 레이어를 찾지 못했습니다.");return false;}
    FScopedSetLandscapeEditingLayer Scope(Target,EditLayerGuid);
    FHeightmapAccessor<false> Access(Target->GetLandscapeInfo()); Access.SetEditLayer(EditLayerGuid);
    Access.SetData(GridMin.X,GridMin.Y,GridMax.X,GridMax.Y,Data.GetData());Access.Flush();
    Target->RequestLayersContentUpdateForceAll(ELandscapeLayerUpdateMode::Update_All,true);
    Target->MarkPackageDirty(); return true;
}

bool ACozyTerrainLayout::BuildDelta(TArray<uint16>& Delta,FString& Result) const
{
    const int W=GridMax.X-GridMin.X+1,H=GridMax.Y-GridMin.Y+1;
    if(W<2||H<2||Baseline.Num()!=W*H){Result=TEXT("연결 당시 높이 기록이 지형 범위와 맞지 않습니다. 새 설계에서 다시 연결하세요.");return false;}
    // 자유 모양은 한 번만 준비해 모든 높이점에 씀
    const FCozyPreparedShapes& Shapes=PreparedFor(Recipe);
    Delta.SetNum(Baseline.Num());
    for(int Y=0;Y<H;++Y) for(int X=0;X<W;++X)
    {
        FVector2D P((double(X)/(W-1)-.5)*BoundSize.X,(double(Y)/(H-1)-.5)*BoundSize.Y);
        const float Base=((int(Baseline[Y*W+X])-32768)/128.f*CapturedTransform.GetScale3D().Z+CapturedTransform.GetLocation().Z-GetActorLocation().Z)/100;
        const double Value=32768+(SampleFast(Recipe,Shapes,P)-Base)*100*128/CapturedTransform.GetScale3D().Z;
        if(!FMath::IsFinite(Value)||Value<0||Value>65535){Result=TEXT("높이가 대상 지형의 표현 범위를 넘습니다. 봉우리나 기본 높이를 낮추세요.");return false;}
        Delta[Y*W+X]=FMath::RoundToInt(Value);
    }
    return true;
}

bool ACozyTerrainLayout::Apply(FString& Result)
{
    if(!GEditor||GEditor->PlayWorld||!Recipe.Validate(Result)) return false;
    if(!GetActorRotation().IsNearlyZero()||!GetActorScale3D().Equals(FVector::OneVector,.001))
    {Result=TEXT("설계 액터의 회전은 0, 배율은 1을 유지하세요. 크기는 설계의 미터 수치로 바꾸세요.");return false;}
    const FScopedTransaction Transaction(NSLOCTEXT("CozyTerrain","Apply","지형 설계 적용")); Modify();
    if(!Target)
    {
        // 기존 지형을 자동 선택하거나 삭제하지 않는다. 겹치는 새 지형 생성도 거부한다.
        const FVector P=GetActorLocation();
        const FBox Area(P-FVector(Recipe.MapSize.X*50,Recipe.MapSize.Y*50,100000),P+FVector(Recipe.MapSize.X*50,Recipe.MapSize.Y*50,100000));
        for(TActorIterator<ALandscapeProxy> It(GetWorld());It;++It) if(Area.Intersect(It->GetComponentsBoundingBox(true)))
        {Result=TEXT("기존 Landscape와 영역이 겹칩니다. 기존 지형 연결을 사용하거나 편집 영역을 옮기세요.");return false;}
        Target=GetWorld()->SpawnActor<ALandscape>();Target->SetFlags(RF_Transactional);Target->Modify();
        const int Q=Recipe.Components*63;
        Target->SetActorLocation(P-FVector(Recipe.MapSize.X*50,Recipe.MapSize.Y*50,0));
        Target->SetActorScale3D(FVector(Recipe.MapSize.X*100/Q,Recipe.MapSize.Y*100/Q,200));
        TMap<FGuid,TArray<uint16>> Heights;Heights.Add(FGuid(),TArray<uint16>());Heights[FGuid()].Init(32768,(Q+1)*(Q+1));
        TMap<FGuid,TArray<FLandscapeImportLayerInfo>> ImportLayers;ImportLayers.Add(FGuid(),{});
        Target->Import(FGuid::NewGuid(),0,0,Q,Q,1,63,Heights,TEXT(""),ImportLayers,ELandscapeImportAlphamapType::Additive,TArrayView<const FLandscapeLayer>());
        Target->SetActorLabel(TEXT("설계에서 만든 Landscape"));Target->SetFolderPath(TEXT("지형 편집기/적용 지형"));
        GridMin=FIntPoint(0,0);GridMax=FIntPoint(Q,Q);CapturedTransform=Target->LandscapeActorToWorld();
        BoundCenter=GetActorLocation();BoundSize=Recipe.MapSize;Baseline.Init(32768,(Q+1)*(Q+1));Baked.Reset();BakeUndoLayer.Reset();bCanUndoBake=false;
        Target->LandscapeMaterial=GroundMaterial?GroundMaterial.Get():LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
        Target->PostEditChange();
    }
    if(!Target->LandscapeActorToWorld().Equals(CapturedTransform,.01)||!GetActorLocation().Equals(BoundCenter,.01)||!Recipe.MapSize.Equals(BoundSize,.01))
    {Result=TEXT("연결 후 영역 위치·크기 또는 대상 지형의 변환이 바뀌었습니다. 새 설계에서 다시 연결하세요.");return false;}
    TArray<uint16> Current; if(!ReadTarget(Current,Result)) return false;
    const bool Existed=EditLayerGuid.IsValid()&&Target->GetEditLayer(EditLayerGuid);
    TArray<uint16> Old;
    if(Existed){Old.Init(32768,Baseline.Num());FHeightmapAccessor<false> A(Target->GetLandscapeInfo());A.SetEditLayer(EditLayerGuid);A.GetDataFast(GridMin.X,GridMin.Y,GridMax.X,GridMax.Y,Old.GetData());}
    for(int I=0;I<Baseline.Num();++I)
    {
        const int Expected=FMath::Clamp(int(Baseline[I])+(Existed?int(Old[I])-32768:0),0,65535);
        if(FMath::Abs(int(Current[I])-Expected)>3){Result=TEXT("다른 편집 레이어의 높이가 연결 당시와 달라졌습니다. 원본을 보존하기 위해 적용을 중단했습니다. 새 설계에서 다시 연결하세요.");return false;}
    }
    // 실제 원본 레이어를 덮어쓰지 않고 전용 가산 레이어에 높이 차이만 기록한다.
    TArray<uint16> Delta;if(!BuildDelta(Delta,Result)) return false;
    FBufferArchive Backup;int32 Version=1;FGuid Guid=Target->GetLandscapeGuid();FIntPoint Min=GridMin,Max=GridMax;bool Had=Existed;
    Backup<<Version<<Guid<<Min<<Max<<Had<<Old;
    const FString Folder=FPaths::ProjectSavedDir()/TEXT("CozyTerrainEditor/Backups");IFileManager::Get().MakeDirectory(*Folder,true);
    const FString BackupPath=Folder/(FGuid::NewGuid().ToString()+TEXT(".bin"));
    if(!FFileHelper::SaveArrayToFile(Backup,*BackupPath)){Result=TEXT("사본 저장에 실패해 적용하지 않았습니다.");return false;}
    LastBackup=BackupPath;Target->Modify();
    if(!Existed)
    {
        const int Index=Target->CreateLayer(FName(*FString::Printf(TEXT("지형 편집기_%s"),*GetName())));
        if(Index==INDEX_NONE){Result=TEXT("편집 레이어를 추가하지 못했습니다.");return false;}
        EditLayerGuid=Target->GetEditLayer(Index)->GetGuid();
    }
    if(!WriteLayer(Delta,Result)) return false;
    PrevAppliedRecipe=AppliedRecipe;bHasPrevApplied=bHasApplied;
    if(bBakeOnApply)
    {
        // 원형을 지형에 굳힌다: 쓴 레이어를 새 기준으로 삼고 평지·산·길 목록을 비움 (기본 높이도 레이어에 들어감)
        BakeUndoLayer=Baked;BakeUndoRecipe=Recipe;bCanUndoBake=true;
        Baked=Delta;Recipe.Landforms.Reset();Recipe.Routes.Reset();Recipe.BaseHeight=0;
    }
    else bCanUndoBake=false;
    AppliedRecipe=Recipe;bHasApplied=true;
    bHasUnappliedChanges=false;bShowPreview=false;Preview->SetVisibility(false);MarkPackageDirty();
    Result=bBakeOnApply?TEXT("지형에 굳혔습니다 · 원형 목록을 비웠습니다. 되돌리려면 '적용 전 높이 복구'. 맵 저장(Ctrl+S)은 직접 하세요.")
        :TEXT("적용 완료 · 원본 레이어 유지 · 적용 전 사본 저장됨. 맵 저장은 직접 실행하세요.");return true;
}

bool ACozyTerrainLayout::ApplyLive(FString& Result)
{
    // 바로 반영: 이미 만든 전용 레이어만 다시 쓴다 · 사본 파일·굳히기·되돌리기 기록 없음 (복구는 마지막 '지형에 적용' 전으로)
    if(!IsValid(Target)||Target->GetWorld()!=GetWorld()||!Target->GetLandscapeInfo()||!EditLayerGuid.IsValid()||!Target->GetEditLayer(EditLayerGuid))
    {Result=TEXT("먼저 '지형에 적용'으로 지형을 연결해 주세요");return false;}
    if(!GEditor||GEditor->PlayWorld){Result=TEXT("플레이 중에는 지형에 반영하지 않습니다.");return false;}
    if(!Recipe.Validate(Result)) return false;
    FIntPoint Min,Max;
    if(!Target->GetLandscapeInfo()->GetLandscapeExtent(Min.X,Min.Y,Max.X,Max.Y)||Min!=GridMin||Max!=GridMax)
    {Result=TEXT("지형의 구획 범위가 변경됐습니다. 새 설계에서 다시 연결하세요.");return false;}
    if(!GetActorRotation().IsNearlyZero()||!GetActorScale3D().Equals(FVector::OneVector,.001)||!Target->LandscapeActorToWorld().Equals(CapturedTransform,.01)||!GetActorLocation().Equals(BoundCenter,.01)||!Recipe.MapSize.Equals(BoundSize,.01))
    {Result=TEXT("연결 후 영역 위치·크기 또는 대상 지형의 변환이 바뀌었습니다. 새 설계에서 다시 연결하세요.");return false;}
    // 빠르게 자주 부르므로 다른 레이어 전체 비교(전체 읽기)는 '지형에 적용' 때만 한다
    TArray<uint16> Delta;if(!BuildDelta(Delta,Result)||!WriteLayer(Delta,Result)) return false;
    // 미리보기 기준만 지금 설계로 · 직전 적용분(PrevAppliedRecipe)과 사본은 그대로 두어 복구가 마지막 '지형에 적용' 전으로 돌아가게
    AppliedRecipe=Recipe;bHasApplied=true;bHasUnappliedChanges=false;MarkPackageDirty();RebuildPreview();
    Result=TEXT("지형에 바로 반영했습니다. 되돌리려면 '적용 전 높이 복구'(마지막 '지형에 적용' 전으로).");return true;
}

bool ACozyTerrainLayout::Restore(FString& Result)
{
    if(!GEditor||GEditor->PlayWorld||!Target||!EditLayerGuid.IsValid()||!Target->GetEditLayer(EditLayerGuid)){Result=TEXT("복구할 전용 레이어가 없습니다.");return false;}
    TArray<uint8> Bytes;if(!FFileHelper::LoadFileToArray(Bytes,*LastBackup)){Result=TEXT("적용 전 사본을 읽지 못했습니다.");return false;}
    FMemoryReader Reader(Bytes);int32 Version;FGuid Guid;FIntPoint Min,Max;bool Existed;TArray<uint16> Old;
    Reader<<Version<<Guid<<Min<<Max<<Existed;
    int32 Count=0;Reader<<Count;
    if(Reader.IsError()||Count!=(Existed?Baseline.Num():0)||Reader.TotalSize()-Reader.Tell()!=int64(Count)*sizeof(uint16))
    {Result=TEXT("사본 크기가 올바르지 않아 복구하지 않았습니다.");return false;}
    Old.SetNum(Count);if(Count)Reader.Serialize(Old.GetData(),Count*sizeof(uint16));
    if(Reader.IsError()||Version!=1||Guid!=Target->GetLandscapeGuid()||Min!=GridMin||Max!=GridMax||(Existed&&Old.Num()!=Baseline.Num())||!Target->LandscapeActorToWorld().Equals(CapturedTransform,.01))
    {Result=TEXT("사본의 대상·범위가 현재 지형과 달라 복구하지 않았습니다.");return false;}
    const FScopedTransaction Transaction(NSLOCTEXT("CozyTerrain","Restore","적용 전 지형으로 복구"));Modify();Target->Modify();
    if(Existed){if(!WriteLayer(Old,Result))return false;}
    else {Target->DeleteLayer(Target->GetLayerIndex(EditLayerGuid));EditLayerGuid.Invalidate();Target->RequestLayersContentUpdateForceAll();}
    // 미리보기 기준도 직전 적용분으로 (복구는 한 단계만)
    AppliedRecipe=PrevAppliedRecipe;bHasApplied=bHasPrevApplied;bHasPrevApplied=false;
    // 굳혔던 적용이면 기준 레이어와 원형 목록도 굳히기 전으로
    const bool bWasBaked=bCanUndoBake;
    if(bCanUndoBake){Baked=BakeUndoLayer;Recipe=BakeUndoRecipe;bCanUndoBake=false;bShowPreview=true;}
    bHasUnappliedChanges=true;Target->MarkPackageDirty();RebuildPreview();
    Result=bWasBaked?TEXT("마지막 적용 전 높이로 복구하고 굳혔던 원형을 다시 불러왔습니다."):TEXT("마지막 적용 전 높이로 복구했습니다. 설계 수치는 유지됩니다.");return true;
}

FString ACozyTerrainLayout::PresetPath() const{return FPaths::ProjectSavedDir()/TEXT("CozyTerrainEditor/Presets")/(FPaths::MakeValidFileName(PresetName)+TEXT(".json"));}
bool ACozyTerrainLayout::SavePreset(FString& Result) const
{
    if(!Recipe.Validate(Result)||PresetName.TrimStartAndEnd().IsEmpty())return false;
    FString Json;FJsonObjectConverter::UStructToJsonObjectString(Recipe,Json);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(PresetPath()),true);
    if(!FFileHelper::SaveStringToFile(Json,*PresetPath(),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)){Result=TEXT("설정 저장 실패");return false;}
    Result=TEXT("설정 저장: ")+PresetPath();return true;
}
bool ACozyTerrainLayout::LoadPreset(FString& Result)
{
    FString Json;FCozyTerrainRecipe Candidate;
    if(!FFileHelper::LoadFileToString(Json,*PresetPath())||!FJsonObjectConverter::JsonObjectStringToUStruct(Json,&Candidate,0,0)||!Candidate.Validate(Result))
    {Result=TEXT("설정 파일이 없거나 지원하는 형식이 아닙니다.");return false;}
    const FScopedTransaction Transaction(NSLOCTEXT("CozyTerrain","Load","지형 설정 불러오기"));Modify();Recipe=MoveTemp(Candidate);bHasUnappliedChanges=true;bShowPreview=true;RebuildPreview();MarkPackageDirty();
    Result=TEXT("설정을 불러왔습니다. 실제 지형은 적용 버튼을 누르기 전까지 바뀌지 않습니다.");return true;
}
FBox2D ACozyTerrainLayout::EstateBounds() const
{
    const FVector2D Half=FVector2D(EstateCells)*CellSize*.5;
    return FBox2D(EstateCenter-Half,EstateCenter+Half);
}
bool ACozyTerrainLayout::SnapLandform(FCozyLandform& F) const
{
    const FBox2D Estate=EstateBounds();
    if(!bSnapToEstateGrid||F.Type!=ECozyLandform::Plateau||CellSize<=0||!Estate.IsInside(F.Center))return false;
    // 영지 층은 시설을 놓는 곳 → 테두리는 칸에 맞춰 각지게, 바닥은 평평하게 (들쭉날쭉·울퉁불퉁을 끔)
    const bool bHadRandom=F.EdgeJitter!=0||F.NoiseAmount!=0;
    if(F.Shape==ECozyLandformShape::Free)
    {
        // 자유 모양: 점마다 칸 선 교차점에 맞추고 방향은 점 위치에 넣어 0도로 · 영지 층은 모서리가 각지게 모두 뾰족
        if(F.Points.Num()<3)return false;
        TArray<FVector2D> Corners;Corners.Reserve(F.Points.Num());
        for(const auto& Pt:F.Points)
        {
            const FVector2D Rel=F.Center+Pt.Position.GetRotated(F.Rotation)-Estate.Min;
            const FVector2D Snapped=Estate.Min+FVector2D(FMath::RoundToDouble(Rel.X/CellSize),FMath::RoundToDouble(Rel.Y/CellSize))*CellSize;
            // 같은 교차점으로 모인 이웃 점은 하나로 (첫 점과 마지막 점도)
            if(Corners.IsEmpty()||!Corners.Last().Equals(Snapped,1e-6))Corners.Add(Snapped);
        }
        if(Corners.Num()>1&&Corners[0].Equals(Corners.Last(),1e-6))Corners.Pop();
        if(Corners.Num()<3)return false;
        float Tier=F.Height;
        for(int I=0;I<TierHeights.Num();++I)if(I==0||FMath::Abs(TierHeights[I]-F.Height)<FMath::Abs(Tier-F.Height))Tier=TierHeights[I];
        const FVector2D NewCenter=CozyTerrainShape::Centroid(Corners);
        bool bChanged=bHadRandom||F.Rotation!=0||Tier!=F.Height||Corners.Num()!=F.Points.Num();
        for(int I=0;!bChanged&&I<Corners.Num();++I)bChanged=!F.Points[I].bSharp||!(F.Center+F.Points[I].Position).Equals(Corners[I],1e-4);
        TArray<FCozyShapePoint> NewPoints;NewPoints.SetNum(Corners.Num());
        for(int I=0;I<Corners.Num();++I){NewPoints[I].Position=Corners[I]-NewCenter;NewPoints[I].bSharp=true;}
        F.Points=MoveTemp(NewPoints);F.Center=NewCenter;F.Rotation=0;F.Height=Tier;F.EdgeJitter=0;F.NoiseAmount=0;
        return bChanged;
    }
    // 90·270도는 가로·세로를 바꿔 0도로 (타원은 180도 돌려도 같음)
    const int Quarter=(FMath::RoundToInt(F.Rotation/90.f)%4+4)%4;
    if(Quarter%2)F.Size=FVector2D(F.Size.Y,F.Size.X);
    F.Rotation=0;F.EdgeJitter=0;F.NoiseAmount=0;
    F.Size.X=FMath::Max(1.,FMath::RoundToDouble(F.Size.X/CellSize))*CellSize;
    F.Size.Y=FMath::Max(1.,FMath::RoundToDouble(F.Size.Y/CellSize))*CellSize;
    // 왼쪽 아래 모서리를 칸 선에 맞춘 뒤 중심을 다시 계산
    const FVector2D Corner=F.Center-F.Size*.5-Estate.Min;
    F.Center=Estate.Min+FVector2D(FMath::RoundToDouble(Corner.X/CellSize),FMath::RoundToDouble(Corner.Y/CellSize))*CellSize+F.Size*.5;
    if(TierHeights.Num())
    {
        float Best=TierHeights[0];
        for(float Tier:TierHeights)if(FMath::Abs(Tier-F.Height)<FMath::Abs(Best-F.Height))Best=Tier;
        F.Height=Best;
    }
    return true;
}
void ACozyTerrainLayout::AddDemo()
{
    Recipe.Landforms.Reset();Recipe.Routes.Reset();
    FCozyLandform Hill;Hill.Type=ECozyLandform::Mountain;Hill.Name=TEXT("뒤쪽 능선");Hill.Center=FVector2D(0,45);Hill.Size=FVector2D(150,100);Hill.Height=45;Recipe.Landforms.Add(Hill);
    FCozyLandform A;A.Name=TEXT("시작 평지");A.Center=FVector2D(-35,-30);A.Size=FVector2D(54,42);A.Height=0;Recipe.Landforms.Add(A);
    FCozyLandform B;B.Name=TEXT("확장 평지");B.Center=FVector2D(40,30);B.Size=FVector2D(50,40);B.Height=12;Recipe.Landforms.Add(B);
    FCozyTerrainRoute R;R.Name=TEXT("시작에서 확장");R.Points={FVector(-20,-25,0),FVector(0,-20,0),FVector(25,0,12),FVector(35,20,12)};Recipe.Routes.Add(R);RebuildPreview();
}
