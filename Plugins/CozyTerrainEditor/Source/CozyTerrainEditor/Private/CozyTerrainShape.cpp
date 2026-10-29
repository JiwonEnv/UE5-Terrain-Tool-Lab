#include "CozyTerrainShape.h"

namespace CozyTerrainShapeImpl
{
    // 같은 자리로 보는 거리(제곱) · 1mm
    constexpr double SameSq=1e-6;

    struct FCtrl { FVector2D P; bool bSharp; int32 Index; };

    // 구심 Catmull-Rom(alpha 0.5) · 매듭 간격 = 거리의 제곱근 · T는 0..1 (P1→P2)
    FVector2D CatmullRom(const FVector2D& P0,const FVector2D& P1,const FVector2D& P2,const FVector2D& P3,double T)
    {
        auto Knot=[](const FVector2D& A,const FVector2D& B){ return FMath::Max(FMath::Sqrt(FVector2D::Distance(A,B)),1e-4); };
        const double T0=0, T1=T0+Knot(P0,P1), T2=T1+Knot(P1,P2), T3=T2+Knot(P2,P3);
        const double t=T1+(T2-T1)*T;
        auto Lerp=[t](const FVector2D& A,const FVector2D& B,double Ta,double Tb){ return A*((Tb-t)/(Tb-Ta))+B*((t-Ta)/(Tb-Ta)); };
        const FVector2D A1=Lerp(P0,P1,T0,T1), A2=Lerp(P1,P2,T1,T2), A3=Lerp(P2,P3,T2,T3);
        const FVector2D B1=Lerp(A1,A2,T0,T2), B2=Lerp(A2,A3,T1,T3);
        return Lerp(B1,B2,T1,T2);
    }

    // 점 P와 선분 AB 사이 거리의 제곱
    FORCEINLINE double SegDistSq(const FVector2D& P,const FVector2D& A,const FVector2D& B)
    {
        const double Dx=B.X-A.X, Dy=B.Y-A.Y, Px=P.X-A.X, Py=P.Y-A.Y;
        const double Len=Dx*Dx+Dy*Dy;
        double U=Len>0?(Px*Dx+Py*Dy)/Len:0;
        U=U<0?0:(U>1?1:U);
        const double Ex=Px-Dx*U, Ey=Py-Dy*U;
        return Ex*Ex+Ey*Ey;
    }
}

namespace CozyTerrainShape
{
    namespace Impl=CozyTerrainShapeImpl;

    void BuildPolyline(const TArray<FCozyShapePoint>& Points, int32 Subdivisions, FCozyPolyShape& Out)
    {
        Out=FCozyPolyShape();
        const int32 Sub=FMath::Clamp(Subdivisions,1,64);

        // 연달아 겹친 점은 하나로 합친다(길이 0 구간 건너뜀). 구간 번호는 겹친 무리의 마지막 점 번호를 쓴다 → 실제로 길이가 있는 구간 번호
        TArray<Impl::FCtrl> C; C.Reserve(Points.Num());
        for(int32 i=0;i<Points.Num();++i)
        {
            const FCozyShapePoint& Pt=Points[i];
            if(C.Num()>0&&FVector2D::DistSquared(C.Last().P,Pt.Position)<=Impl::SameSq){ C.Last().Index=i; C.Last().bSharp|=Pt.bSharp; continue; }
            C.Add({Pt.Position,Pt.bSharp,i});
        }
        while(C.Num()>1&&FVector2D::DistSquared(C.Last().P,C[0].P)<=Impl::SameSq){ C[0].bSharp|=C.Last().bSharp; C.Pop(); }

        const int32 N=C.Num();
        if(N<3)
        {
            // 점이 모자라면 그대로 둔다 (IsValid()가 거짓)
            for(const Impl::FCtrl& X:C){ Out.Poly.Add(X.P); Out.Span.Add(X.Index); }
            return;
        }

        Out.Poly.Reserve(N*Sub); Out.Span.Reserve(N*Sub);
        for(int32 i=0;i<N;++i)
        {
            const Impl::FCtrl& C1=C[i]; const Impl::FCtrl& C2=C[(i+1)%N];
            Out.Poly.Add(C1.P); Out.Span.Add(C1.Index); // 조절점은 그대로 지난다
            if(C1.bSharp&&C2.bSharp) continue;          // 양 끝이 뾰족하면 직선
            // 뾰족한 끝의 바깥 이웃은 맞은편 점을 그 점 기준으로 뒤집은 가상 점 (P0'=2·P1−P2, P3'=2·P2−P1)
            // → 접선이 이 구간 방향과 같아져 뾰족한 점에서 둥글게 휘지 않고 반대편 구간과 꺾여 만난다
            const FVector2D P0=C1.bSharp?C1.P*2.0-C2.P:C[(i-1+N)%N].P;
            const FVector2D P3=C2.bSharp?C2.P*2.0-C1.P:C[(i+2)%N].P;
            for(int32 s=1;s<Sub;++s){ Out.Poly.Add(Impl::CatmullRom(P0,C1.P,C2.P,P3,double(s)/Sub)); Out.Span.Add(C1.Index); }
        }
    }

    float SignedDistance(const FCozyPolyShape& Shape, FVector2D P)
    {
        if(!Shape.IsValid()) return 1e9f;
        const FVector2D* V=Shape.Poly.GetData(); const int32 N=Shape.Poly.Num();
        double Best=TNumericLimits<double>::Max(); bool bInside=false;
        for(int32 i=0,j=N-1;i<N;j=i++)
        {
            const FVector2D& A=V[j]; const FVector2D& B=V[i];
            const double D=Impl::SegDistSq(P,A,B); if(D<Best) Best=D;
            // 짝수-홀수 교차 규칙
            if((A.Y>P.Y)!=(B.Y>P.Y)&&P.X<(B.X-A.X)*(P.Y-A.Y)/(B.Y-A.Y)+A.X) bInside=!bInside;
        }
        const double Dist=FMath::Sqrt(Best);
        return float(bInside?-Dist:Dist);
    }

    bool Contains(const FCozyPolyShape& Shape, FVector2D P)
    {
        return Shape.IsValid()&&SignedDistance(Shape,P)<=0;
    }

    void Prepare(FCozyPolyShape& Shape)
    {
        Shape.Bounds=FBox2D(ForceInit); Shape.MaxInset=0;
        for(const FVector2D& V:Shape.Poly) Shape.Bounds+=V;
        if(!Shape.IsValid()) return;
        // 1) 32×32 칸 중심을 훑어 가장 깊은 안쪽 점을 찾고 2) 그 둘레를 9×9로 여러 번 좁혀 다시 훑는다
        const FVector2D Size=Shape.Bounds.GetSize();
        constexpr int32 G=32;
        double Best=0; FVector2D BestP=Shape.Bounds.GetCenter(); bool bFound=false;
        for(int32 y=0;y<G;++y) for(int32 x=0;x<G;++x)
        {
            const FVector2D P=Shape.Bounds.Min+FVector2D(Size.X*(x+0.5)/G,Size.Y*(y+0.5)/G);
            const double D=-double(SignedDistance(Shape,P));
            if(D>Best){ Best=D; BestP=P; bFound=true; }
        }
        if(!bFound) return;
        FVector2D Half=Size/double(G);
        for(int32 Round=0;Round<5;++Round)
        {
            const FVector2D Center=BestP; constexpr int32 R=4;
            for(int32 y=-R;y<=R;++y) for(int32 x=-R;x<=R;++x)
            {
                const FVector2D P=Center+FVector2D(Half.X*x/R,Half.Y*y/R);
                const double D=-double(SignedDistance(Shape,P));
                if(D>Best){ Best=D; BestP=P; }
            }
            Half*=0.35;
        }
        Shape.MaxInset=float(FMath::Max(Best,0.0));
    }

    int32 FindSpan(const FCozyPolyShape& Shape, FVector2D P, float MaxDistance)
    {
        const int32 N=Shape.Poly.Num();
        if(N<2||MaxDistance<0) return INDEX_NONE;
        double Best=TNumericLimits<double>::Max(); int32 BestI=INDEX_NONE;
        for(int32 i=0;i<N;++i)
        {
            const double D=Impl::SegDistSq(P,Shape.Poly[i],Shape.Poly[(i+1)%N]);
            if(D<Best){ Best=D; BestI=i; }
        }
        if(BestI==INDEX_NONE||Best>double(MaxDistance)*double(MaxDistance)) return INDEX_NONE;
        return Shape.Span.IsValidIndex(BestI)?Shape.Span[BestI]:BestI;
    }

    FVector2D Centroid(const TArray<FVector2D>& Points)
    {
        const int32 N=Points.Num();
        if(N==0) return FVector2D::ZeroVector;
        FVector2D Avg=FVector2D::ZeroVector;
        for(const FVector2D& V:Points) Avg+=V;
        Avg/=double(N);
        if(N<3) return Avg;
        // 신발끈 공식 · 원점 대신 평균을 기준으로 계산해 큰 좌표에서도 정밀도 유지
        double A2=0, Cx=0, Cy=0;
        for(int32 i=0,j=N-1;i<N;j=i++)
        {
            const FVector2D Pj=Points[j]-Avg, Pi=Points[i]-Avg;
            const double Cr=Pj.X*Pi.Y-Pi.X*Pj.Y;
            A2+=Cr; Cx+=(Pj.X+Pi.X)*Cr; Cy+=(Pj.Y+Pi.Y)*Cr;
        }
        if(FMath::Abs(A2)<1e-9) return Avg; // 넓이가 거의 0이면 평균
        return Avg+FVector2D(Cx/(3.0*A2),Cy/(3.0*A2));
    }
}

// ---- 랜덤 (작업 3) ----
namespace CozyTerrainShapeRandomImpl
{
    // 정수 섞기(씨앗 → 겹마다 다른 출발점)
    FORCEINLINE uint32 Hash32(uint32 X){ X^=X>>16; X*=0x7feb352dU; X^=X>>15; X*=0x846ca68bU; X^=X>>16; return X; }
    // 펄린 잡음은 256칸마다 반복 → 미리 0..256으로 접어 float 정밀도 손실을 막는다
    FORCEINLINE double Wrap256(double V){ return V-256.0*FMath::FloorToDouble(V/256.0); }

    // 닫힌 꺾은선을 둘레 길이 기준 M개 고른 점으로
    void ResampleClosed(const TArray<FVector2D>& Poly,int32 M,TArray<FVector2D>& Res)
    {
        Res.Reset(); const int32 N=Poly.Num(); if(N<2||M<1) return;
        double L=0; for(int32 i=0;i<N;++i) L+=FVector2D::Distance(Poly[i],Poly[(i+1)%N]);
        if(L<=0){ Res.Add(Poly[0]); return; }
        int32 Seg=0; double SegStart=0, SegLen=FVector2D::Distance(Poly[0],Poly[1%N]);
        for(int32 m=0;m<M;++m)
        {
            const double S=L*m/M;
            while(Seg<N-1&&SegStart+SegLen<S){ SegStart+=SegLen; ++Seg; SegLen=FVector2D::Distance(Poly[Seg],Poly[(Seg+1)%N]); }
            const double T=SegLen>0?FMath::Clamp((S-SegStart)/SegLen,0.0,1.0):0.0;
            const FVector2D A=Poly[Seg], B=Poly[(Seg+1)%N];
            Res.Add(A+(B-A)*T);
        }
    }
}

namespace CozyTerrainShape
{
    namespace RImpl=CozyTerrainShapeRandomImpl;

    float Noise2D(FVector2D P, float Size, int32 Seed)
    {
        // 4겹 펄린(겹마다 크기 ½ · 세기 ½) · 겹마다 씨앗으로 정한 다른 출발점
        const double Inv=1.0/FMath::Max(double(Size),0.5);
        double X=P.X*Inv, Y=P.Y*Inv, Amp=1, Sum=0, Norm=0;
        for(int32 o=0;o<4;++o)
        {
            const uint32 H=RImpl::Hash32(uint32(Seed)*0x9E3779B9u+uint32(o)*0x85EBCA6Bu+0x632BE5ABu);
            const double Ox=double(H&0xFFFFu)*(256.0/65536.0), Oy=double(H>>16)*(256.0/65536.0);
            Sum+=Amp*double(FMath::PerlinNoise2D(FVector2D(RImpl::Wrap256(X+Ox),RImpl::Wrap256(Y+Oy))));
            Norm+=Amp; Amp*=0.5; X*=2.0; Y*=2.0;
        }
        // 겹을 합치면 값이 가운데로 몰리므로 키워서 대략 -1~1로 맞춘다
        return float(FMath::Clamp(Sum/Norm*1.6,-1.2,1.2));
    }

    void ApplyEdgeJitter(FCozyPolyShape& Shape, float Amount, float Size, int32 Seed)
    {
        if(Amount<=0||!Shape.IsValid()) return;
        const double Sz=FMath::Max(double(Size),0.5);
        const double MaxSeg=FMath::Max(Sz/6.0,0.5);
        const int32 N=Shape.Poly.Num();

        // 1) 다시 나누기 · 끼운 점은 출발 꼭짓점의 구간 번호를 이어받음
        TArray<FVector2D> P; TArray<int32> S; P.Reserve(N*2); S.Reserve(N*2);
        for(int32 i=0;i<N;++i)
        {
            const FVector2D A=Shape.Poly[i], B=Shape.Poly[(i+1)%N];
            const int32 Sp=Shape.Span.IsValidIndex(i)?Shape.Span[i]:i;
            const int32 Cuts=FMath::Clamp(FMath::CeilToInt32(FVector2D::Distance(A,B)/MaxSeg),1,4096);
            P.Add(A); S.Add(Sp);
            for(int32 c=1;c<Cuts;++c){ P.Add(A+(B-A)*(double(c)/Cuts)); S.Add(Sp); }
        }
        const int32 M=P.Num();

        // 2) 둘레 길이 · 방향(넓이 부호)
        TArray<double> Arc; Arc.SetNumUninitialized(M);
        double L=0, A2=0;
        for(int32 i=0;i<M;++i)
        {
            Arc[i]=L; const FVector2D& A=P[i]; const FVector2D& B=P[(i+1)%M];
            L+=FVector2D::Distance(A,B); A2+=A.X*B.Y-B.X*A.Y;
        }
        if(L<=0) return;
        const double Out=A2>=0?1.0:-1.0; // 반시계면 진행 방향의 오른쪽이 바깥

        // 3) 꼭짓점 바깥 방향 = 양쪽 선분 바깥 방향의 평균
        auto EdgeNormal=[&](const FVector2D& A,const FVector2D& B){ const FVector2D D=(B-A).GetSafeNormal(); return FVector2D(D.Y,-D.X)*Out; };
        TArray<FVector2D> R; R.SetNumUninitialized(M);
        const double TwoPi=2.0*UE_DOUBLE_PI, Rad=L/(TwoPi*Sz);
        for(int32 i=0;i<M;++i)
        {
            const FVector2D& Pv=P[(i-1+M)%M]; const FVector2D& Pc=P[i]; const FVector2D& Pn=P[(i+1)%M];
            FVector2D Nrm=(EdgeNormal(Pv,Pc)+EdgeNormal(Pc,Pn)).GetSafeNormal();
            if(Nrm.IsNearlyZero()) Nrm=EdgeNormal(Pc,Pn);
            // 둘레를 원으로 감아 잡음을 읽는다 → 끝과 처음이 이어짐 · 원 둘레 = L/Size 덩어리
            const double Th=TwoPi*Arc[i]/L;
            const double Nz=FMath::Clamp(double(Noise2D(FVector2D(FMath::Cos(Th),FMath::Sin(Th))*Rad,1.f,Seed)),-1.0,1.0);
            R[i]=Pc+Nrm*(double(Amount)*Nz);
        }
        Shape.Poly=MoveTemp(R); Shape.Span=MoveTemp(S);
    }

    void GenerateBands(const TArray<FCozyShapePoint>& Outline, int32 Count, float PeakHeight, int32 Seed, TArray<FCozyContourBand>& OutBands)
    {
        OutBands.Reset();
        if(Outline.Num()<3) return;
        const int32 K=FMath::Clamp(Count,1,8);
        constexpr int32 Sub=8, NumPts=16;
        FCozyPolyShape Outer; BuildPolyline(Outline,Sub,Outer); Prepare(Outer);
        if(!Outer.IsValid()||Outer.MaxInset<=0) return;
        FRandomStream Rng(Seed);

        // 1) 꼭대기 C: 범위 안 후보 40개(못 찾으면 더) 중 '무작위 + 깊이(가장 깊은 곳의 60% 이상이면 만점)' 점수가 가장 높은 곳
        //    테두리 가까이(가장 깊은 곳의 30% 미만)는 처음 200번은 후보에서 뺀다 (꼭대기가 가장자리에 붙어 한쪽 비탈이 급해지던 문제)
        const double Deep=0.6*Outer.MaxInset, Shallow=0.3*Outer.MaxInset;
        FVector2D C=FVector2D::ZeroVector; double BestScore=-1;
        for(int32 Try=0;Try<400&&(Try<40||BestScore<0);++Try)
        {
            const FVector2D P(Rng.FRandRange(Outer.Bounds.Min.X,Outer.Bounds.Max.X),Rng.FRandRange(Outer.Bounds.Min.Y,Outer.Bounds.Max.Y));
            const double Luck=Rng.FRand();
            const double D=-double(SignedDistance(Outer,P));
            if(D<=0||(D<Shallow&&Try<200)) continue;
            const double Score=0.6*Luck+3.0*FMath::Min(D/Deep,1.0);
            if(Score>BestScore){ BestScore=Score; C=P; }
        }
        if(BestScore<0) return;

        // 2) 바깥 모양을 고른 간격 32점으로 → C에서 곧게 보이는(선분이 넉넉히 안쪽으로만 지나는) 점만 남겨 최대 16점
        //    오목한 모양에서 C 쪽으로 줄일 때 테두리 밖으로 나가거나 오목한 모서리에 끼지 않게 하기 위함
        TArray<FVector2D> All; RImpl::ResampleClosed(Outer.Poly,2*NumPts,All);
        TArray<FVector2D> Seen;
        for(const FVector2D& Oi:All)
        {
            const double Dist=FVector2D::Distance(Oi,C); bool bSee=true;
            for(int32 s=1;bSee&&s<=12;++s)
            {
                const double T=s/12.0;
                bSee=SignedDistance(Outer,Oi+(C-Oi)*T)<=-FMath::Min(0.3*Outer.MaxInset,0.5*T*Dist);
            }
            if(bSee) Seen.Add(Oi);
        }
        if(Seen.Num()<3) Seen=All;
        TArray<FVector2D> O;
        if(Seen.Num()>NumPts){ for(int32 i=0;i<NumPts;++i) O.Add(Seen[(i*Seen.Num())/NumPts]); }
        else O=MoveTemp(Seen);
        if(O.Num()<3) return;
        const double Margin=FMath::Max(0.01*Outer.MaxInset,0.02);
        FCozyPolyShape Prev=Outer;
        for(int32 k=1;k<=K;++k)
        {
            const double F=double(k)/(K+1);
            TArray<FVector2D> Pts; Pts.Reserve(O.Num());
            for(const FVector2D& Oi:O)
            {
                const FVector2D Dir=(Oi-C)*(1.0-F); // C에서 이 띠 점까지 (지역 반지름)
                const double Wob=Rng.FRandRange(-1.0,1.0)*0.08*(1.0-0.5*F);
                FVector2D P=C+Dir*(1.0+Wob);
                // 앞 띠(처음은 바깥 모양) 안쪽에 들 때까지 C 쪽으로 당김
                for(int32 It=0;It<24&&SignedDistance(Prev,P)>-Margin;++It) P+=(C-P)*0.25;
                Pts.Add(P);
            }
            // 곡선으로 이었을 때 앞 띠를 벗어나는 구간이 있으면 그 구간 양 끝 점을 더 당긴다 · 여러 번 안 되면 띠 전체를 C 쪽으로 줄임
            FCozyContourBand Band; Band.Height=PeakHeight*float(k)/float(K+1);
            FCozyPolyShape Shape;
            for(int32 Round=0;Round<16;++Round)
            {
                Band.Points.Reset();
                for(const FVector2D& P:Pts){ FCozyShapePoint X; X.Position=P; X.bSharp=false; Band.Points.Add(X); }
                BuildPolyline(Band.Points,Sub,Shape);
                TArray<bool> Bad; Bad.Init(false,Pts.Num()); bool bAny=false;
                for(int32 j=0;j<Shape.Poly.Num();++j)
                {
                    if(SignedDistance(Prev,Shape.Poly[j])<=-Margin*0.5) continue;
                    const int32 Sp=Shape.Span.IsValidIndex(j)?Shape.Span[j]:0;
                    if(Pts.IsValidIndex(Sp)){ Bad[Sp]=true; Bad[(Sp+1)%Pts.Num()]=true; bAny=true; }
                }
                if(!bAny) break;
                if(Round>=6) Bad.Init(true,Pts.Num());
                for(int32 i=0;i<Pts.Num();++i) if(Bad[i]) Pts[i]+=(C-Pts[i])*0.2;
            }
            OutBands.Add(Band);
            Prev=MoveTemp(Shape);
        }
    }
}
