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
