#include "CozyTerrainLayout.h"
#include "Misc/AutomationTest.h"
#include "JsonObjectConverter.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Tests/AutomationEditorCommon.h"
#include "Landscape.h"
#include "LandscapeEdit.h"
#include "LandscapeInfo.h"
#include "LandscapeEditLayer.h"
#include "FileHelpers.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerStart.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/SWindow.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyTerrainRecipeTest,"CozyRealm.TerrainEditor.RecipeAndReuse",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCozyTerrainRecipeTest::RunTest(const FString&)
{
    FCozyTerrainRecipe R;FString Error;
    TestTrue(TEXT("기본 설정 유효"),R.Validate(Error));
    FCozyLandform Hill;Hill.Type=ECozyLandform::Mountain;Hill.Size=FVector2D(100,60);Hill.Height=40;R.Landforms.Add(Hill);
    FCozyLandform Flat;Flat.Height=12;Flat.Size=FVector2D(30,20);R.Landforms.Add(Flat);
    TestEqual(TEXT("산 위 평지의 목표 높이"),R.Evaluate(FVector2D(0,0),0),12.f);
    TestEqual(TEXT("평지 안 높이 균일"),R.Evaluate(FVector2D(5,3),0),12.f);
    FCozyTerrainRoute Path;Path.Points={FVector(-50,0,0),FVector(0,0,10),FVector(0,50,20)};R.Routes.Add(Path);
    TestTrue(TEXT("꺾인 길 다음 구간 높이 연속"),FMath::Abs(R.Evaluate(FVector2D(0,25),0)-15)<.01);
    FString Json;TestTrue(TEXT("재사용 설정 저장 형식"),FJsonObjectConverter::UStructToJsonObjectString(R,Json));
    FCozyTerrainRecipe Loaded;TestTrue(TEXT("다른 맵용 설정 복원"),FJsonObjectConverter::JsonObjectStringToUStruct(Json,&Loaded,0,0));
    TestEqual(TEXT("복원 후 형상 일치"),Loaded.Evaluate(FVector2D(0,25),0),R.Evaluate(FVector2D(0,25),0));
    FCozyTerrainRecipe Peaks;FCozyLandform Peak;Peak.Type=ECozyLandform::Mountain;Peak.Size=FVector2D(40,40);Peak.Height=25;Peaks.Landforms.Add(Peak);Peaks.Landforms.Add(Peak);
    TestTrue(TEXT("겹친 산이 두 배로 솟지 않음"),Peaks.Evaluate(FVector2D(0,0),0)<=26.1f);
    TestTrue(TEXT("산에서 먼 바닥은 그대로"),FMath::Abs(Peaks.Evaluate(FVector2D(200,200),0))<.01f);
    FCozyTerrainRecipe Mesa;FCozyLandform Top;Top.Type=ECozyLandform::Mountain;Top.Size=FVector2D(40,40);Top.Height=20;Mesa.Landforms.Add(Top);
    TestTrue(TEXT("뾰족한 산은 중심에서 조금만 벗어나도 낮아짐"),Mesa.Evaluate(FVector2D(8,0),0)<19);
    Mesa.Landforms[0].PeakFlat=50;
    TestTrue(TEXT("꼭대기 넓이 50%면 반지름 40% 지점도 꼭대기 높이"),FMath::Abs(Mesa.Evaluate(FVector2D(8,0),0)-20)<.01f);
    Loaded.Landforms[0].Size.X=0;TestFalse(TEXT("영 크기 거부"),Loaded.Validate(Error));
    Loaded=R;Loaded.Version=99;TestFalse(TEXT("미지원 설정 버전 거부"),Loaded.Validate(Error));
    Loaded=R;Loaded.Routes[0].Width=-1;TestFalse(TEXT("음수 길 폭 거부"),Loaded.Validate(Error));
    Loaded=R;Loaded.MapSize.X=100000;TestFalse(TEXT("과도한 편집 영역 거부"),Loaded.Validate(Error));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyTerrainSnapTest,"CozyRealm.TerrainEditor.EstateGridSnap",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCozyTerrainSnapTest::RunTest(const FString&)
{
    // 기본값: 영지 중심 0,0 · 12×12칸 · 2m · 층 0/2.4/4.8m
    const ACozyTerrainLayout* Plan=GetDefault<ACozyTerrainLayout>();
    FCozyLandform Flat;Flat.Center=FVector2D(3.3,-1.2);Flat.Size=FVector2D(7,5.1);Flat.Height=3;
    TestTrue(TEXT("영지 안 평지 맞춤"),Plan->SnapLandform(Flat));
    TestEqual(TEXT("크기는 2m 배수"),Flat.Size,FVector2D(8,6));
    TestEqual(TEXT("모서리가 칸 선 위"),Flat.Center,FVector2D(4,-1));
    TestEqual(TEXT("가장 가까운 층 높이"),Flat.Height,2.4f);
    FCozyLandform Turned;Turned.Center=FVector2D(0,0);Turned.Size=FVector2D(4,10);Turned.Rotation=85;
    Plan->SnapLandform(Turned);
    TestTrue(TEXT("90도 가까우면 가로·세로 바꿔 0도"),Turned.Rotation==0&&Turned.Size==FVector2D(10,4));
    FCozyLandform Hill=Flat;Hill.Type=ECozyLandform::Mountain;Hill.Height=3;
    TestFalse(TEXT("산은 맞추지 않음"),Plan->SnapLandform(Hill));
    FCozyLandform Outside=Flat;Outside.Center=FVector2D(50,50);Outside.Height=3;
    TestFalse(TEXT("영지 밖 평지는 자유"),Plan->SnapLandform(Outside));
    TestEqual(TEXT("영지 밖 높이 유지"),Outside.Height,3.f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyTerrainFreeShapeTest,"CozyRealm.TerrainEditor.FreeShapeAndSlope",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCozyTerrainFreeShapeTest::RunTest(const FString&)
{
    // 한 변 20m 정사각형 (모두 뾰족 → 꺾은선이 조절점 다각형과 같음)
    auto Square=[](ECozyLandform Type,float Height){FCozyLandform F;F.Type=Type;F.Shape=ECozyLandformShape::Free;F.Height=Height;F.Blend=4;
        for(const FVector2D& P:{FVector2D(-10,-10),FVector2D(10,-10),FVector2D(10,10),FVector2D(-10,10)}){FCozyShapePoint Pt;Pt.Position=P;Pt.bSharp=true;F.Points.Add(Pt);}return F;};
    auto Near=[](float A,float B,float Tol){return FMath::Abs(A-B)<=Tol;};
    FString Error;
    // 자유 모양 평지 · 연결 폭
    FCozyTerrainRecipe Flat;Flat.Landforms.Add(Square(ECozyLandform::Plateau,6));
    TestTrue(TEXT("자유 모양 설정 유효"),Flat.Validate(Error));
    TestTrue(TEXT("자유 평지 안 목표 높이"),Near(Flat.Evaluate(FVector2D(0,0),0),6,1e-4f)&&Near(Flat.Evaluate(FVector2D(7,-9),0),6,1e-4f));
    TestTrue(TEXT("자유 평지 연결 폭 사이"),Flat.Evaluate(FVector2D(12,0),0)>0&&Flat.Evaluate(FVector2D(12,0),0)<6);
    TestEqual(TEXT("자유 평지 멀리 밖은 그대로"),Flat.Evaluate(FVector2D(30,0),0),0.f);
    // 경사 각도 45도 평지: 올릴 때·내릴 때
    Flat.Landforms[0].EdgeMode=ECozyEdgeMode::Angle;Flat.Landforms[0].SlopeAngle=45;
    TestTrue(TEXT("45도 비탈 2m 밖은 6-2"),Near(Flat.Evaluate(FVector2D(12,0),0),4,.05f));
    TestTrue(TEXT("45도 비탈 안은 목표 높이"),Near(Flat.Evaluate(FVector2D(3,3),0),6,1e-4f));
    TestTrue(TEXT("45도 비탈이 주변 높이에서 멈춤"),Near(Flat.Evaluate(FVector2D(20,0),0),0,1e-4f));
    TestTrue(TEXT("내리는 평지: 2m 밖은 6+2"),Near(Flat.Evaluate(FVector2D(12,0),10),8,.05f));
    TestTrue(TEXT("내리는 평지: 멀면 주변 높이, 안은 목표"),Near(Flat.Evaluate(FVector2D(20,0),10),10,1e-4f)&&Near(Flat.Evaluate(FVector2D(0,0),10),6,1e-4f));
    // 타원 평지 경사 각도 (짧은 반지름으로 근사 · 원이면 정확)
    FCozyTerrainRecipe Round;FCozyLandform Disc;Disc.Size=FVector2D(20,20);Disc.Height=6;Disc.EdgeMode=ECozyEdgeMode::Angle;Disc.SlopeAngle=45;Round.Landforms.Add(Disc);
    TestTrue(TEXT("원 평지 45도 2m 밖은 4"),Near(Round.Evaluate(FVector2D(12,0),0),4,.05f));
    // 자유 모양 산 · 경사 각도: 테두리가 산기슭, 안쪽 깊이×tan, 높이에 닿으면 평평
    FCozyTerrainRecipe Hill;Hill.Landforms.Add(Square(ECozyLandform::Mountain,5));Hill.Landforms[0].EdgeMode=ECozyEdgeMode::Angle;Hill.Landforms[0].SlopeAngle=45;
    TestTrue(TEXT("각도 산 깊이 2m는 2m"),Near(Hill.Evaluate(FVector2D(-8,0),0),2,.05f));
    TestTrue(TEXT("각도 산 깊이 3m는 3m"),Near(Hill.Evaluate(FVector2D(7,7),0),3,.05f));
    TestTrue(TEXT("각도 산 평평한 꼭대기"),Near(Hill.Evaluate(FVector2D(0,0),0),5,1e-3f)&&Near(Hill.Evaluate(FVector2D(2,-3),0),5,1e-3f));
    TestTrue(TEXT("각도 산 밖은 0"),Near(Hill.Evaluate(FVector2D(15,0),0),0,1e-4f));
    Hill.Landforms[0].SlopeAngle=30;
    TestTrue(TEXT("각도 산 30도 깊이 2m"),Near(Hill.Evaluate(FVector2D(-8,0),0),2*FMath::Tan(FMath::DegreesToRadians(30.f)),.05f));
    // 자유 모양 산 · 연결 폭: 가장 깊은 곳이 가장 높고 바깥으로 낮아짐
    FCozyTerrainRecipe Peak;Peak.Landforms.Add(Square(ECozyLandform::Mountain,20));
    const float H0=Peak.Evaluate(FVector2D(0,0),0),H5=Peak.Evaluate(FVector2D(5,0),0),H10=Peak.Evaluate(FVector2D(10,0),0),H20=Peak.Evaluate(FVector2D(20,0),0);
    TestTrue(TEXT("폭 산 가장 깊은 곳은 거의 높이"),Near(H0,20,.5f));
    TestTrue(TEXT("폭 산 바깥으로 낮아짐"),H0>H5&&H5>H10&&H10>H20&&H20>=0);
    TestTrue(TEXT("폭 산 멀리는 0"),Near(Peak.Evaluate(FVector2D(80,0),0),0,1e-3f));
    // 검사
    FCozyTerrainRecipe Bad=Flat;Bad.Landforms[0].Points.SetNum(2);
    TestFalse(TEXT("점 2개 자유 모양 거부"),Bad.Validate(Error));
    Bad=Flat;Bad.Landforms[0].Points[1].Position.X=1e10;
    TestFalse(TEXT("범위 밖 점 위치 거부"),Bad.Validate(Error));
    Bad=Flat;Bad.Landforms[0].SlopeAngle=89;
    TestFalse(TEXT("경사 각도 범위 밖 거부"),Bad.Validate(Error));
    // 옛 설정 파일(새 항목 없음)도 그대로 열림
    const FString OldJson=TEXT("{\"version\":1,\"mapSize\":{\"x\":252,\"y\":252},\"components\":4,\"baseHeight\":0,\"landforms\":[{\"name\":\"a\",\"type\":\"Plateau\",\"center\":{\"x\":0,\"y\":0},\"size\":{\"x\":40,\"y\":30},\"height\":5,\"blend\":8,\"rotation\":0,\"peakFlat\":0}],\"routes\":[]}");
    FCozyTerrainRecipe Old;
    TestTrue(TEXT("옛 설정 파일 읽기"),FJsonObjectConverter::JsonObjectStringToUStruct(OldJson,&Old,0,0));
    TestTrue(TEXT("옛 설정은 타원·연결 폭"),Old.Validate(Error)&&Old.Landforms.Num()==1&&Old.Landforms[0].Shape==ECozyLandformShape::Ellipse&&Old.Landforms[0].EdgeMode==ECozyEdgeMode::Width);
    TestEqual(TEXT("옛 설정 높이"),Old.Evaluate(FVector2D(0,0),0),5.f);
    // 자유 모양 저장·복원
    FString Json;FJsonObjectConverter::UStructToJsonObjectString(Hill,Json);FCozyTerrainRecipe Back;
    TestTrue(TEXT("자유 모양 저장·복원"),FJsonObjectConverter::JsonObjectStringToUStruct(Json,&Back,0,0)&&Back.Landforms.Num()==1&&Back.Landforms[0].Points.Num()==4&&Back.Evaluate(FVector2D(-8,0),0)==Hill.Evaluate(FVector2D(-8,0),0));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyTerrainLegacyEllipseTest,"CozyRealm.TerrainEditor.LegacyEllipseUnchanged",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCozyTerrainLegacyEllipseTest::RunTest(const FString&)
{
    // 자유 모양 이전의 타원 계산식 (길 제외) · 타원+연결 폭 결과가 그대로인지 비교
    auto Legacy=[](const FCozyTerrainRecipe& R,FVector2D P,float Base)
    {
        auto SmoothMax=[](float A,float B){const float K=FMath::Min(4.f,.25f*FMath::Max(A,B));if(K<=KINDA_SMALL_NUMBER)return FMath::Max(A,B);const float W=FMath::Max(K-FMath::Abs(A-B),0.f)/K;return FMath::Max(A,B)+W*W*K*.25f;};
        float Up=0,Down=0;
        for(const auto& F:R.Landforms)
        {
            if(F.Type!=ECozyLandform::Mountain) continue;
            const FVector2D Q=(P-F.Center).GetRotated(-F.Rotation);
            const double D=FMath::Sqrt(FMath::Square(Q.X/FMath::Max(1.,F.Size.X*.5))+FMath::Square(Q.Y/FMath::Max(1.,F.Size.Y*.5)));
            const double T=FMath::Clamp(F.PeakFlat,0.f,90.f)/100.,Rr=FMath::Max(0.,D-T)/(1-T);
            const float Bump=F.Height*FMath::Exp(-2*Rr*Rr);
            if(Bump>=0)Up=SmoothMax(Up,Bump);else Down=-SmoothMax(-Down,-Bump);
        }
        float H=Base+Up+Down;
        for(const auto& F:R.Landforms)
        {
            if(F.Type==ECozyLandform::Mountain) continue;
            const FVector2D Q=(P-F.Center).GetRotated(-F.Rotation);
            const double D=FMath::Sqrt(FMath::Square(Q.X/FMath::Max(1.,F.Size.X*.5))+FMath::Square(Q.Y/FMath::Max(1.,F.Size.Y*.5)));
            H=FMath::Lerp(H,F.Height,float(1-FMath::SmoothStep(1.,1.+F.Blend/FMath::Max(1.,FMath::Min(F.Size.X,F.Size.Y)*.5),D)));
        }
        return H;
    };
    FCozyTerrainRecipe R;
    FCozyLandform A;A.Type=ECozyLandform::Mountain;A.Center=FVector2D(10,20);A.Size=FVector2D(120,70);A.Height=40;A.Rotation=30;A.PeakFlat=25;R.Landforms.Add(A);
    FCozyLandform B;B.Type=ECozyLandform::Mountain;B.Center=FVector2D(-30,-10);B.Size=FVector2D(50,50);B.Height=-12;R.Landforms.Add(B);
    FCozyLandform C;C.Center=FVector2D(-20,30);C.Size=FVector2D(40,24);C.Height=8;C.Blend=10;C.Rotation=-20;R.Landforms.Add(C);
    float Worst=0;
    for(int Y=-6;Y<=6;++Y)for(int X=-6;X<=6;++X){const FVector2D P(X*13.7,Y*11.3);Worst=FMath::Max(Worst,FMath::Abs(R.Evaluate(P,3)-Legacy(R,P,3)));}
    TestTrue(TEXT("타원·연결 폭 결과가 이전과 같음"),Worst<1e-4f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyTerrainFreeSnapTest,"CozyRealm.TerrainEditor.FreeShapeEstateSnap",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCozyTerrainFreeSnapTest::RunTest(const FString&)
{
    // 기본값: 영지 중심 0,0 · 12×12칸 · 2m · 층 0/2.4/4.8m
    const ACozyTerrainLayout* Plan=GetDefault<ACozyTerrainLayout>();
    FCozyLandform F;F.Shape=ECozyLandformShape::Free;F.Center=FVector2D(1,1);F.Rotation=90;F.Height=3;
    for(const FVector2D& P:{FVector2D(-3.1,-2.9),FVector2D(3.2,-3),FVector2D(2.9,3.1),FVector2D(-3,2.8)}){FCozyShapePoint Pt;Pt.Position=P;F.Points.Add(Pt);}
    TestTrue(TEXT("영지 안 자유 평지 맞춤"),Plan->SnapLandform(F));
    TestEqual(TEXT("방향 0도"),F.Rotation,0.f);
    TestEqual(TEXT("가장 가까운 층 높이"),F.Height,2.4f);
    TestEqual(TEXT("점 수 유지"),F.Points.Num(),4);
    bool bGrid=true,bSharp=true;
    for(const auto& Pt:F.Points){const FVector2D W=F.Center+Pt.Position;bGrid&=FMath::Abs(W.X/2-FMath::RoundToDouble(W.X/2))<1e-4&&FMath::Abs(W.Y/2-FMath::RoundToDouble(W.Y/2))<1e-4;bSharp&=Pt.bSharp;}
    TestTrue(TEXT("점이 2m 칸 교차점 위"),bGrid);
    TestTrue(TEXT("모서리 모두 뾰족"),bSharp);
    // 90도 돌린 첫 점 (-3.1,-2.9) → (2.9,-3.1)+중심(1,1) = (3.9,-2.1) → (4,-2)
    TestTrue(TEXT("돌린 방향이 점에 들어감"),(F.Center+F.Points[0].Position).Equals(FVector2D(4,-2),1e-4));
    TestFalse(TEXT("이미 맞춘 모양은 다시 바뀌지 않음"),Plan->SnapLandform(F));
    FCozyLandform Far=F;Far.Center=FVector2D(50,50);Far.Height=3;
    TestFalse(TEXT("영지 밖 자유 평지는 자유"),Plan->SnapLandform(Far));
    return true;
}
// 별도 검증 프로세스에서만 새 임시 맵을 만든다. 일반 전체 검사에서는 현재 맵을 건드리지 않는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyTerrainLandscapeTest,"CozyRealm.TerrainEditor.LandscapeApplyRestore",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCozyTerrainLandscapeTest::RunTest(const FString&)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("CozyTerrainValidation")))
    {AddInfo(TEXT("별도 검증 실행 인자가 없어 임시 맵 검사는 건너뜁니다."));return true;}
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();
    auto* Plan=World->SpawnActor<ACozyTerrainLayout>();Plan->bBakeOnApply=false;
    Plan->Recipe.MapSize=FVector2D(63,63);Plan->Recipe.Components=1;
    FCozyLandform Flat;Flat.Size=FVector2D(40,40);Flat.Height=12;Plan->Recipe.Landforms.Add(Flat);
    FString Result;
    if(!TestTrue(TEXT("실제 Landscape 최초 적용"),Plan->Apply(Result))){AddError(Result);return false;}
    auto* Target=Plan->Target.Get();Target->ForceLayersFullUpdate();
    const int LayerCount=Target->GetLayersConst().Num();
    TestTrue(TEXT("원본과 전용 레이어가 분리됨"),LayerCount>=2);
    auto Height=[&](){Target->ForceLayersFullUpdate();FHeightmapAccessor<false> A(Target->GetLandscapeInfo());A.SetEditLayer(FGuid());uint16 H=32768;A.GetDataFast(31,31,31,31,&H);return (int(H)-32768)/128.f*Target->GetActorScale3D().Z;};
    TestTrue(TEXT("높이 12m 실제 반영"),FMath::Abs(Height()-1200)<3);
    Target->RecreateCollisionComponents();
    const auto Collision=Target->GetHeightAtLocation(FVector::ZeroVector);
    TestTrue(TEXT("실제 지형 충돌 높이"),Collision.IsSet()&&FMath::Abs(Collision.GetValue()-1200)<5);
    Plan->Recipe.Landforms[0].Height=18;
    if(!TestTrue(TEXT("반복 적용"),Plan->Apply(Result))){AddError(Result);return false;}
    TestEqual(TEXT("반복 적용이 레이어를 계속 만들지 않음"),Target->GetLayersConst().Num(),LayerCount);
    TestTrue(TEXT("높이 18m 실제 반영"),FMath::Abs(Height()-1800)<3);
    TestTrue(TEXT("마지막 적용 전 높이 복구"),Plan->Restore(Result));
    TestTrue(TEXT("복구된 높이 12m"),FMath::Abs(Height()-1200)<3);
    // 바로 반영: 연결 전에는 거부 · 연결 뒤에는 사본 없이 레이어만 다시 쓰고, 복구는 마지막 '지형에 적용' 전으로
    {
        auto* Fresh=World->SpawnActor<ACozyTerrainLayout>();
        TestFalse(TEXT("연결 전 바로 반영 거부"),Fresh->ApplyLive(Result));World->DestroyActor(Fresh);
        const FString Backup=Plan->LastBackup;
        Plan->Recipe.Landforms[0].Height=15;
        if(!TestTrue(TEXT("바로 반영"),Plan->ApplyLive(Result))){AddError(Result);return false;}
        TestTrue(TEXT("바로 반영 높이 15m"),FMath::Abs(Height()-1500)<3);
        TestEqual(TEXT("바로 반영은 레이어를 늘리지 않음"),Target->GetLayersConst().Num(),LayerCount);
        TestEqual(TEXT("바로 반영은 사본을 만들지 않음"),Plan->LastBackup,Backup);
        TestEqual(TEXT("바로 반영은 원형을 비우지 않음"),Plan->Recipe.Landforms.Num(),1);
        TestTrue(TEXT("바로 반영 뒤 미리보기 기준도 지금 설계"),FMath::Abs(Plan->SampleApplied(FVector2D::ZeroVector)-15)<.05);
        TestTrue(TEXT("바로 반영 뒤 복구"),Plan->Restore(Result));
        TestTrue(TEXT("복구하면 마지막 적용 전 12m"),FMath::Abs(Height()-1200)<3);
        Plan->Recipe.Landforms[0].Height=18;
    }
    auto* Connected=World->SpawnActor<ACozyTerrainLayout>();Connected->bBakeOnApply=false;
    TestTrue(TEXT("기존 Landscape 높이를 기준으로 연결"),Connected->BindLandscape(Target,Result));
    TestTrue(TEXT("연결된 원래 높이 읽기"),FMath::Abs(Connected->Sample(FVector2D::ZeroVector)-12)<.03);
    Flat.Height=20;Connected->Recipe.Landforms.Add(Flat);
    TestTrue(TEXT("기존 Landscape 위 새 전용 레이어 적용"),Connected->Apply(Result));
    TestTrue(TEXT("기존 높이에서 목표 20m로 보정"),FMath::Abs(Height()-2000)<3);
    TestFalse(TEXT("외부 높이 변경 감지 후 기존 기준 적용 거부"),Plan->Apply(Result));
    TestTrue(TEXT("연결 지형 원래 높이 복원"),Connected->Restore(Result));
    TestTrue(TEXT("원래 높이 12m와 레이어 수 보존"),FMath::Abs(Height()-1200)<3&&Target->GetLayersConst().Num()==LayerCount);
    Plan->PresetName=TEXT("자동검증_맵간재사용");TestTrue(TEXT("설정 파일 저장"),Plan->SavePreset(Result));
    // 다른 설계가 같은 지형 위에 새 Landscape를 중복 생성하면 안 된다.
    auto* Overlap=World->SpawnActor<ACozyTerrainLayout>();Overlap->Recipe.MapSize=FVector2D(63,63);Overlap->Recipe.Components=1;
    TestFalse(TEXT("기존 지형과 겹치는 새 생성 거부"),Overlap->Apply(Result));World->DestroyActor(Overlap);
    // 굳히기: 적용하면 원형이 비고, 다음 설계는 굳힌 높이 위에서 시작, 복구하면 지형과 원형이 함께 돌아옴
    World=FAutomationEditorCommonUtils::CreateNewMap();
    auto* Bake=World->SpawnActor<ACozyTerrainLayout>();Bake->Recipe.MapSize=FVector2D(63,63);Bake->Recipe.Components=1;
    Flat.Height=10;Bake->Recipe.Landforms.Add(Flat);
    if(!TestTrue(TEXT("굳히며 적용"),Bake->Apply(Result))){AddError(Result);return false;}
    Target=Bake->Target.Get();
    TestEqual(TEXT("적용 뒤 원형 목록 비움"),Bake->Recipe.Landforms.Num(),0);
    TestTrue(TEXT("굳힌 높이 10m 실제 반영"),FMath::Abs(Height()-1000)<3);
    TestTrue(TEXT("다음 설계는 굳힌 높이 위에서 시작"),FMath::Abs(Bake->Sample(FVector2D::ZeroVector)-10)<.05);
    FCozyLandform Bump;Bump.Type=ECozyLandform::Mountain;Bump.Size=FVector2D(30,30);Bump.Height=5;Bake->Recipe.Landforms.Add(Bump);
    if(!TestTrue(TEXT("굳힌 뒤 두 번째 적용"),Bake->Apply(Result))){AddError(Result);return false;}
    TestTrue(TEXT("굳힌 지형 위에 산 5m 더함"),FMath::Abs(Height()-1500)<5);
    TestTrue(TEXT("두 번째 적용 복구"),Bake->Restore(Result));
    TestTrue(TEXT("복구하면 첫 굳힌 높이 10m"),FMath::Abs(Height()-1000)<3);
    TestEqual(TEXT("복구하면 원형이 돌아옴"),Bake->Recipe.Landforms.Num(),1);
    // 마지막에 별도 맵을 남겨 GUI 검증을 이어간다. 원본 프로젝트 맵은 저장하지 않는다.
    World=FAutomationEditorCommonUtils::CreateNewMap();Plan=World->SpawnActor<ACozyTerrainLayout>();Plan->bBakeOnApply=false;
    Plan->PresetName=TEXT("자동검증_맵간재사용");TestTrue(TEXT("다른 맵에서 설정 불러오기"),Plan->LoadPreset(Result));
    TestTrue(TEXT("다른 맵에서도 같은 높이"),FMath::Abs(Plan->Sample(FVector2D::ZeroVector)-18)<.01);
    TestTrue(TEXT("다른 맵에 실제 지형 생성"),Plan->Apply(Result));
    if(Plan->Target){Plan->Target->ForceLayersFullUpdate();TestTrue(TEXT("최초 적용 전 복구"),Plan->Restore(Result));Plan->Target->ForceLayersFullUpdate();TestFalse(TEXT("복구 후 전용 레이어 제거"),Plan->EditLayerGuid.IsValid());}
    World=FAutomationEditorCommonUtils::CreateNewMap();Plan=World->SpawnActor<ACozyTerrainLayout>();Plan->AddDemo();
    World->GetWorldSettings()->DefaultGameMode=AGameModeBase::StaticClass();
    auto* Sun=World->SpawnActor<ADirectionalLight>();Sun->SetActorRotation(FRotator(-45,-45,0));World->SpawnActor<ASkyLight>();
    auto* Start=World->SpawnActor<APlayerStart>();Start->SetActorLocation(FVector(-11000,-14000,12500));Start->SetActorRotation(FRotator(-30,55,0));
    if(auto Window=FGlobalTabmanager::Get()->GetRootWindow())Window->SetTitle(FText::FromString(TEXT("CozyRealm · 지형 편집기 검증")));
    FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("CozyTerrainEditor")));
    return true;
}
#endif
