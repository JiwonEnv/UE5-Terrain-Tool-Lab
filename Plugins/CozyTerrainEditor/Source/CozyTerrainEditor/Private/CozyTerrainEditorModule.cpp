#include "Modules/ModuleManager.h"
#include "CozyTerrainLayout.h"
#include "CozyTerrainShape.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "ProceduralMeshComponent.h"
#include "ToolMenus.h"
#include "ScopedTransaction.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"
#include "Rendering/DrawElements.h"
#include "HAL/IConsoleManager.h"
#include "Framework/Docking/TabManager.h"
#include "UObject/UnrealType.h"
#include "LevelEditorViewport.h"

namespace CozyTerrainUI
{
static const FName TabName(TEXT("CozyTerrainEditor"));
static FText T(const TCHAR* S){return FText::FromString(S);}
// 0.1m 단위로 맞춤
static double R1(double V){return FMath::RoundToDouble(V*10)/10;}
static FVector2D R1(FVector2D V){return FVector2D(R1(V.X),R1(V.Y));}
static bool IsFree(const FCozyLandform& F){return F.Shape==ECozyLandformShape::Free;}
// 자유 모양 점은 중심 기준·회전 전 좌표
static FVector2D ToLocal(const FCozyLandform& F,FVector2D W){return (W-F.Center).GetRotated(-F.Rotation);}
static FVector2D ToWorld(const FCozyLandform& F,FVector2D V){return F.Center+V.GetRotated(F.Rotation);}
static FCozyPolyShape FreePoly(const FCozyLandform& F){FCozyPolyShape S;if(F.Points.Num()>=3){CozyTerrainShape::BuildPolyline(F.Points,12,S);CozyTerrainShape::Prepare(S);}return S;}
// 자유 모양의 가로·세로는 점들을 감싼 크기 (표시용)
static void FitFreeSize(FCozyLandform& F){if(!IsFree(F)||F.Points.IsEmpty())return;FBox2D B(ForceInit);for(const auto& S:F.Points)B+=S.Position;const FVector2D E=B.GetSize();F.Size=FVector2D(FMath::Clamp(R1(E.X),2.,2000.),FMath::Clamp(R1(E.Y),2.,2000.));}
// 안쪽 띠는 자유 모양 산에만
static bool HasBands(const FCozyLandform& F){return IsFree(F)&&F.Type==ECozyLandform::Mountain;}
// 지도에 그리는 선: 테두리 들쭉날쭉까지 넣어 지형 계산과 같게 (씨앗: 바깥 모양 +0 · 띠 i는 +i+1) · Band가 없으면 바깥 모양
// 선택 판정(HitLandform)은 들쭉날쭉 없는 FreePoly로 · 선 가까이 누르기·점 끼워 넣기는 보이는 선(이것)으로
static FCozyPolyShape LinePoly(const FCozyLandform& F,int Band)
{
    const bool bBand=F.Bands.IsValidIndex(Band);const TArray<FCozyShapePoint>& Pts=bBand?F.Bands[Band].Points:F.Points;FCozyPolyShape S;
    if(Pts.Num()>=3){CozyTerrainShape::BuildPolyline(Pts,12,S);CozyTerrainShape::ApplyEdgeJitter(S,F.EdgeJitter,F.EdgeJitterSize,F.RandomSeed+(bBand?Band+1:0));}
    return S;
}
// 높이 글자: 정수면 "10m", 아니면 "12.5m"
static FString MetersText(float V){return FMath::IsNearlyEqual(V,FMath::RoundToFloat(V),.05f)?FString::Printf(TEXT("%.0fm"),V):FString::Printf(TEXT("%.1fm"),V);}
class SPanel;
class SMap : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SMap){} SLATE_END_ARGS()
    TWeakPtr<SPanel> Panel;
    void Construct(const FArguments&,TSharedRef<SPanel> InPanel){Panel=InPanel;SetClipping(EWidgetClipping::ClipToBounds);}
    // 작게 시작해 창이 화면 밖으로 나가지 않게 (창을 키우면 지도도 같이 커짐)
    virtual FVector2D ComputeDesiredSize(float) const override{return FVector2D(460,380);}
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry&,const FSlateRect&,FSlateWindowElementList&,int32,const FWidgetStyle&,bool) const override;
    virtual FReply OnMouseButtonDown(const FGeometry&,const FPointerEvent&) override;
    virtual FReply OnMouseMove(const FGeometry&,const FPointerEvent&) override;
    virtual FReply OnMouseButtonUp(const FGeometry&,const FPointerEvent&) override;
    virtual FReply OnMouseButtonDoubleClick(const FGeometry&,const FPointerEvent&) override;
    virtual void OnMouseLeave(const FPointerEvent&) override{bHover=false;Invalidate(EInvalidateWidgetReason::Paint);}
    virtual void OnMouseCaptureLost(const FCaptureLostEvent&) override;
    virtual bool SupportsKeyboardFocus() const override{return true;}
    virtual FReply OnKeyDown(const FGeometry&,const FKeyEvent&) override;
    TUniquePtr<FScopedTransaction> Drag;
    bool bResize=false;
    // 평지·산 추가: 누른 곳에서 끈 범위로 만든다
    bool bCreating=false;FVector2D CreateStart=FVector2D::ZeroVector,CreateStartPixel=FVector2D::ZeroVector;
    // 자유 모양 그리기: 찍은 점(지도 m) · 마우스 위치(미리보기 선)
    TArray<FVector2D> DrawPts;FVector2D HoverPixel=FVector2D::ZeroVector;bool bHover=false;
    // 자유 모양 점 끌기 · 통째로 옮길 때 누른 곳과 중심의 차이
    bool bPointDrag=false;FVector2D DragOffset=FVector2D::ZeroVector;
    /** 지도 픽셀 위치에 있는 점 번호 (Pts = 바깥 모양 또는 띠의 점 · 없으면 INDEX_NONE) */
    int HitPoint(const FVector2D& Size,const ACozyTerrainLayout& P,const FCozyLandform& F,const TArray<FCozyShapePoint>& Pts,FVector2D Mouse) const;
    /** 자유 모양 산의 띠 선(8픽셀 안)이나 띠 점을 눌렀는지 · 눌렀으면 띠 번호와 점 번호(없으면 INDEX_NONE) */
    bool HitBand(const FVector2D& Size,const ACozyTerrainLayout& P,const FCozyLandform& F,FVector2D Mouse,int& OutBand,int& OutPoint) const;
    /** 찍은 점으로 자유 모양을 만든다 (3개 이상) */
    void FinishFree(SPanel& O,ACozyTerrainLayout& P);
    /** 찍은 점으로 고른 산에 안쪽 띠를 만든다 (3개 이상) */
    void FinishBand(SPanel& O,ACozyTerrainLayout& P);
    /** 그리기 도구(4 자유 모양 · 5 띠)에 맞게 닫기 */
    void FinishDraw(SPanel& O,ACozyTerrainLayout& P);
};

class SPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPanel){} SLATE_END_ARGS()
    TWeakObjectPtr<ACozyTerrainLayout> Plan;
    TWeakObjectPtr<UWorld> LastWorld;
    TSharedPtr<IDetailsView> Details;
    TSharedPtr<SMap> Map;
    // 오른쪽 요소 목록 (0 이상 = 평지·산 번호 · 음수 = -1-길 번호)
    TArray<TSharedPtr<int32>> Items;
    TSharedPtr<SListView<TSharedPtr<int32>>> List;
    // Mode: 0 선택 · 1 평지 · 2 산 · 3 길 · 4 자유 모양 · 5 안쪽 띠 그리기 · Point = 선택한 자유 모양(또는 띠)의 점 · Band = 고른 안쪽 띠 (없으면 바깥 모양)
    int Mode=0,Selected=INDEX_NONE,Route=INDEX_NONE,Node=INDEX_NONE,Point=INDEX_NONE,Band=INDEX_NONE;
    // '띠 자동 만들기'로 만들 띠 수
    int BandCount=3;
    bool IsDrawing() const{return Mode==4||Mode==5;}
    /** 고른 띠 번호 (자유 모양 산이고 번호가 맞을 때만 · 아니면 INDEX_NONE) */
    int BandOf(const FCozyLandform& F) const{return HasBands(F)&&F.Bands.IsValidIndex(Band)?Band:INDEX_NONE;}
    const FCozyContourBand* SelBand() const{const FCozyLandform* F=Sel();const int B=F?BandOf(*F):INDEX_NONE;return B!=INDEX_NONE?&F->Bands[B]:nullptr;}
    /** 점 편집 대상: 고른 띠가 있으면 그 띠, 없으면 바깥 모양 */
    const TArray<FCozyShapePoint>& EditPoints(const FCozyLandform& F) const{const int B=BandOf(F);return B!=INDEX_NONE?F.Bands[B].Points:F.Points;}
    TArray<FCozyShapePoint>& EditPoints(FCozyLandform& F) const{const int B=BandOf(F);return B!=INDEX_NONE?F.Bands[B].Points:F.Points;}
    bool bContours=true,bSlope=true;
    // 바로 반영: 마지막 변경 0.35초 뒤 (끄는 중이면 0.35초마다) 지형에 반영
    double LiveDue=0,LastLive=0;FString LiveNote;
    void ScheduleLive(){if(auto* P=GetPlan())if(P->bLiveApply)LiveDue=FPlatformTime::Seconds()+.35;}
    const FCozyLandform* Sel() const{auto* P=GetPlan();return P&&P->Recipe.Landforms.IsValidIndex(Selected)?&P->Recipe.Landforms[Selected]:nullptr;}
    static EVisibility Show(bool b){return b?EVisibility::Visible:EVisibility::Collapsed;}
    FString Status=TEXT("현재 레벨에서 새 설계를 만들거나, Landscape를 선택한 뒤 연결하세요. 실제 지형은 적용 전까지 바뀌지 않습니다.");
    ACozyTerrainLayout* GetPlan() const
    {
        auto* P=Plan.Get();return P&&GEditor&&P->GetWorld()==GEditor->GetEditorWorldContext().World()?P:nullptr;
    }
    void Refresh()
    {
        // 막대를 끄는 중에는 세부 정보 칸을 다시 만들지 않음 (느려지고 스크롤이 튐)
        if(auto* P=GetPlan()){P->RebuildPreview();if(Details&&!SliderTx)Details->SetObject(P,true);}
        if(Map)Map->Invalidate(EInvalidateWidgetReason::Paint);
        RebuildList();
    }
    void RebuildList()
    {
        Items.Reset();TSharedPtr<int32> Current;
        if(auto* P=GetPlan())
        {
            for(int I=0;I<P->Recipe.Landforms.Num();++I){Items.Add(MakeShared<int32>(I));if(I==Selected)Current=Items.Last();}
            for(int R=0;R<P->Recipe.Routes.Num();++R){Items.Add(MakeShared<int32>(-1-R));if(R==Route&&Selected==INDEX_NONE)Current=Items.Last();}
        }
        if(!List)return;
        List->RequestListRefresh();
        if(Current){List->SetSelection(Current,ESelectInfo::Direct);List->RequestScrollIntoView(Current);}else List->ClearSelection();
    }
    FText ItemText(int32 Id) const
    {
        auto* P=GetPlan();if(!P)return FText::GetEmpty();
        if(P->Recipe.Landforms.IsValidIndex(Id)){const auto& F=P->Recipe.Landforms[Id];return FText::FromString(FString::Printf(TEXT("%s · %s%s · %.1fm"),F.Type==ECozyLandform::Plateau?TEXT("평지"):TEXT("산"),*F.Name,IsFree(F)&&!F.Name.Contains(TEXT("자유 모양"))?TEXT(" (자유 모양)"):TEXT(""),F.Height));}
        const int R=-1-Id;if(P->Recipe.Routes.IsValidIndex(R)){const auto& W=P->Recipe.Routes[R];return FText::FromString(FString::Printf(TEXT("길 · %s · 점 %d개"),*W.Name,W.Points.Num()));}
        return FText::GetEmpty();
    }
    /** 지도 좌표에 있는 평지·산 (위에 그려진 것부터) */
    int HitLandform(const ACozyTerrainLayout& P,FVector2D Pos) const
    {
        for(int I=P.Recipe.Landforms.Num()-1;I>=0;--I)
        {
            const auto& F=P.Recipe.Landforms[I];FVector2D D=ToLocal(F,Pos);
            // 자유 모양은 곡선 안쪽인지로
            if(IsFree(F)){const FCozyPolyShape S=FreePoly(F);if(S.IsValid()&&CozyTerrainShape::Contains(S,D))return I;continue;}
            if(FMath::Square(D.X/F.Size.X*2)+FMath::Square(D.Y/F.Size.Y*2)<=1)return I;
        }
        return INDEX_NONE;
    }
    void DeleteSelected()
    {
        // 자유 모양(또는 고른 띠)의 점을 골랐으면 그 점만 지움 (3개 아래로는 안 됨)
        if(const FCozyLandform* F=Sel())if(IsFree(*F)&&EditPoints(*F).IsValidIndex(Point))
        {
            if(EditPoints(*F).Num()<=3){Message(TEXT("점은 3개 이상이어야 합니다"));return;}
            const bool bBand=BandOf(*F)!=INDEX_NONE;
            Mutate([this](ACozyTerrainLayout& P){auto& L=P.Recipe.Landforms[Selected];EditPoints(L).RemoveAt(Point);FitFreeSize(L);Point=INDEX_NONE;});
            Message(bBand?TEXT("띠의 점을 지웠습니다. 띠 전체를 지우려면 점을 고르지 않은 채 Delete (또는 '띠 지우기')."):TEXT("점을 지웠습니다. 요소 전체를 지우려면 모양 안쪽을 눌러 고른 뒤 Delete."));return;
        }
        // 띠를 골랐으면 (점 없이) 그 띠만 지움 · 산은 그대로
        if(SelBand()){DeleteBand();return;}
        Mutate([this](auto& P){if(P.Recipe.Landforms.IsValidIndex(Selected))P.Recipe.Landforms.RemoveAt(Selected);else if(P.Recipe.Routes.IsValidIndex(Route)){if(P.Recipe.Routes[Route].Points.IsValidIndex(Node))P.Recipe.Routes[Route].Points.RemoveAt(Node);else P.Recipe.Routes.RemoveAt(Route);}SelectNone();});
    }
    void DeleteBand()
    {
        if(!SelBand())return;
        Mutate([this](ACozyTerrainLayout& P){auto& L=P.Recipe.Landforms[Selected];if(L.Bands.IsValidIndex(Band))L.Bands.RemoveAt(Band);Band=Point=INDEX_NONE;});
        Message(TEXT("띠를 지웠습니다 (산은 그대로 · 되돌리기로 복구)."));
    }
    /** 고른 자유 모양 산에 띠를 그리기 시작 */
    void StartBandDraw()
    {
        const FCozyLandform* F=Sel();if(!F||!HasBands(*F)){Message(TEXT("먼저 자유 모양 산을 고르세요."));return;}
        Mode=5;Band=Point=INDEX_NONE;if(Map)Map->DrawPts.Reset();
        Message(TEXT("띠 그리기 · 산 안쪽에 점을 차례로 클릭하세요. 첫 점을 다시 누르거나 Enter·더블클릭으로 닫습니다 (3개 이상) · Backspace 마지막 점 지우기 · Esc 취소."));
    }
    /** 바깥 모양 안에 띠를 자동으로 (있던 띠는 바꿈 · 되돌리기로 복구) */
    void AutoBands()
    {
        const FCozyLandform* F=Sel();if(!F||!HasBands(*F)||F->Points.Num()<3){Message(TEXT("먼저 자유 모양 산을 고르세요."));return;}
        const bool bHad=F->Bands.Num()>0;const int Count=FMath::Clamp(BandCount,1,8);
        Mutate([this,Count](ACozyTerrainLayout& P){auto& L=P.Recipe.Landforms[Selected];CozyTerrainShape::GenerateBands(L.Points,Count,L.Height,L.RandomSeed,L.Bands);Band=Point=INDEX_NONE;});
        if(const FCozyLandform* G=Sel())Message(FString::Printf(TEXT("안쪽 띠 %d개를 만들었습니다%s. 띠 선을 눌러 고르면 점·높이를 고칠 수 있습니다."),G->Bands.Num(),bHad?TEXT(" (있던 띠는 바뀜 · 되돌리기로 복구)"):TEXT("")));
    }
    void Message(FString S){Status=MoveTemp(S);Refresh();}
    /** 고른 평지·산 하나를 고침 (되돌리기·바로 반영 포함) */
    void EditSel(TFunction<void(FCozyLandform&)> Fn){Mutate([this,Fn](ACozyTerrainLayout& P){if(P.Recipe.Landforms.IsValidIndex(Selected))Fn(P.Recipe.Landforms[Selected]);});}
    void Mutate(TFunction<void(ACozyTerrainLayout&)> Fn)
    {
        if(auto* P=GetPlan()) {const FScopedTransaction Tx(T(TEXT("지형 설계 편집")));P->Modify();Fn(*P);SnapSelected(*P);P->bHasUnappliedChanges=true;P->bShowPreview=true;P->MarkPackageDirty();Refresh();ScheduleLive();}
    }
    // 영지 안 평지는 고칠 때마다 칸·층 높이에 맞춤
    void SnapSelected(ACozyTerrainLayout& P)
    {
        if(P.Recipe.Landforms.IsValidIndex(Selected)&&P.SnapLandform(P.Recipe.Landforms[Selected]))
            Status=FString::Printf(TEXT("영지 안 평지라 %.0fm 칸과 층 높이에 맞췄습니다. 끄려면 왼쪽 '영지 칸 맞춤' 체크를 해제하세요."),P.CellSize);
    }
    TSharedRef<SWidget> Button(const TCHAR* Label,TFunction<void()> Action)
    {return SNew(SButton).Text(T(Label)).OnClicked_Lambda([Action](){Action();return FReply::Handled();});}
    // 숫자 막대를 끄는 동안 한 번의 되돌리기로 묶음
    TUniquePtr<FScopedTransaction> SliderTx;
    /** 세부 정보 칸처럼 끌어서 바꾸는 막대 + 직접 입력 · 소수 한 자리 */
    TSharedRef<SWidget> Number(const TCHAR* Label,float SliderMin,float SliderMax,TFunction<TOptional<float>()> Getter,TFunction<void(float)> Setter)
    {
        auto Set=[Setter](float V){if(FMath::IsFinite(V))Setter(FMath::RoundToFloat(V*10)/10);};
        return SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).Padding(2)[SNew(STextBlock).Text(T(Label))]
        +SHorizontalBox::Slot().FillWidth(1.4f).Padding(2)[SNew(SNumericEntryBox<float>).AllowSpin(true).MinSliderValue(SliderMin).MaxSliderValue(SliderMax).Delta(.1f).MaxFractionalDigits(1)
            .Value_Lambda([Getter](){return Getter();})
            .OnBeginSliderMovement_Lambda([this](){SliderTx=MakeUnique<FScopedTransaction>(T(TEXT("숫자 막대로 조절")));})
            .OnValueChanged_Lambda([this,Set](float V){if(SliderTx)Set(V);})
            // 막대를 놓으면 바로 반영을 한 번 더 (끄는 동안은 0.35초마다)
            .OnEndSliderMovement_Lambda([this](float){SliderTx.Reset();Refresh();if(LiveDue>0)LiveDue=FPlatformTime::Seconds();})
            .OnValueCommitted_Lambda([Set](float V,ETextCommit::Type){Set(V);})];
    }
    /** 둘 중 하나 고르기 (켜진 쪽 파랗게) · Get이 -1이면 아무것도 안 켜짐 */
    TSharedRef<SWidget> Choice(const TCHAR* Label,const TCHAR* A,const TCHAR* B,TFunction<int()> Get,TFunction<void(int)> Set,const TCHAR* Tip=TEXT(""))
    {
        auto Opt=[Get,Set](const TCHAR* Text,int V)->TSharedRef<SWidget>
        {return SNew(SButton).HAlign(HAlign_Center).Text(T(Text)).ButtonColorAndOpacity_Lambda([Get,V](){return Get()==V?FLinearColor(.25,.65,1):FLinearColor::White;}).OnClicked_Lambda([Get,Set,V](){if(Get()!=V)Set(V);return FReply::Handled();});};
        return SNew(SHorizontalBox).ToolTipText(T(Tip))
        +SHorizontalBox::Slot().FillWidth(1).Padding(2).VAlign(VAlign_Center)[SNew(STextBlock).Text(T(Label))]
        +SHorizontalBox::Slot().FillWidth(.7f).Padding(2)[Opt(A,0)]
        +SHorizontalBox::Slot().FillWidth(.7f).Padding(2)[Opt(B,1)];
    }
    void SelectNone(){Selected=Route=Node=Point=Band=INDEX_NONE;}
    void FitView()
    {
        auto* P=GetPlan();
        if(!P){Message(TEXT("먼저 설계를 열거나 만드세요."));return;}
        // 연결된 Landscape가 있으면 그 범위, 없으면 설계 영역 중심 (미리보기를 꺼 두면 설계 액터의 범위가 비어 원점을 보던 문제)
        const FBox Box=P->Target?P->Target->GetComponentsBoundingBox(true):FBox();
        const FVector Center=Box.IsValid?Box.GetCenter():P->GetActorLocation();
        const double Span=FMath::Max(P->Recipe.MapSize.X,P->Recipe.MapSize.Y)*100;
        const FVector Eye=Center+FVector(-.55*Span,-.7*Span,.6*Span);
        // 지금 쓰는 레벨 뷰포트 → 없으면 첫 원근 뷰포트 · 자동 밝기 보정은 그대로 둠 (끄면 화면이 검게 됐음)
        FLevelEditorViewportClient* Client=GCurrentLevelEditingViewportClient&&GCurrentLevelEditingViewportClient->IsPerspective()?GCurrentLevelEditingViewportClient:nullptr;
        if(!Client)for(auto* Each:GEditor->GetLevelViewportClients())if(Each&&Each->IsPerspective()){Client=Each;break;}
        if(!Client){Message(TEXT("원근 레벨 뷰포트를 찾지 못했습니다."));return;}
        Client->SetViewLocation(Eye);Client->SetViewRotation((Center-Eye).Rotation());Client->Invalidate();
        Message(TEXT("레벨 뷰포트를 설계 영역에 맞췄습니다."));
    }
    void LoadExisting()
    {
        auto* W=GEditor->GetEditorWorldContext().World();
        for(TActorIterator<ACozyTerrainLayout> It(W);It;++It){Plan=*It;SelectNone();Message(TEXT("현재 레벨의 저장된 설계를 열었습니다."));return;}
        Message(TEXT("저장된 설계가 없습니다. 새 설계를 눌러 시작하세요."));
    }
    void NewPlan()
    {
        if(GEditor->PlayWorld){Message(TEXT("플레이를 종료한 뒤 편집하세요."));return;}
        const FScopedTransaction Tx(T(TEXT("새 지형 설계")));
        auto* P=GEditor->GetEditorWorldContext().World()->SpawnActor<ACozyTerrainLayout>();P->SetFlags(RF_Transactional);P->SetActorLabel(TEXT("지형 설계"));P->SetFolderPath(TEXT("지형 편집기/설계"));Plan=P;SelectNone();Message(TEXT("새 설계 · 왼쪽 도구를 고른 뒤 지도에 클릭하세요."));
    }
    void BindSelected()
    {
        ALandscape* L=nullptr;
        for(FSelectionIterator I(*GEditor->GetSelectedActors());I;++I)if(auto* Proxy=Cast<ALandscapeProxy>(*I))if(Proxy->GetLandscapeInfo())L=Proxy->GetLandscapeInfo()->LandscapeActor.Get();
        if(!L){Message(TEXT("먼저 아웃라이너에서 적용할 Landscape를 선택하세요."));return;}
        if(!GetPlan())NewPlan();
        const FScopedTransaction Tx(T(TEXT("현재 Landscape 연결")));GetPlan()->Modify();FString R;GetPlan()->BindLandscape(L,R);Message(R);
    }
    void Construct(const FArguments&)
    {
        FDetailsViewArgs Args;Args.bHideSelectionTip=true;Args.bLockable=false;Args.bAllowSearch=true;
        Details=FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).CreateDetailView(Args);
        Details->SetDisableCustomDetailLayouts(true);
        Details->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda([](const FPropertyAndParent& P)
        {
            const FProperty* Root=P.ParentProperties.IsEmpty()?&P.Property:P.ParentProperties.Last();
            if(Root->GetOwnerStruct()!=ACozyTerrainLayout::StaticClass()||!Root->HasMetaData(TEXT("Category"))) return false;
            // 지도·위 숫자 칸과 겹치는 값은 숨김: 미리보기 체크(왼쪽 체크로) · 평지·산·길 원본 목록 · 설정 버전
            static const TSet<FName> Hidden={GET_MEMBER_NAME_CHECKED(ACozyTerrainLayout,Preview),GET_MEMBER_NAME_CHECKED(ACozyTerrainLayout,bShowPreview),
                GET_MEMBER_NAME_CHECKED(FCozyTerrainRecipe,Landforms),GET_MEMBER_NAME_CHECKED(FCozyTerrainRecipe,Routes),GET_MEMBER_NAME_CHECKED(FCozyTerrainRecipe,Version),
                GET_MEMBER_NAME_CHECKED(ACozyTerrainLayout,ContourInterval),GET_MEMBER_NAME_CHECKED(ACozyTerrainLayout,bBakeOnApply),GET_MEMBER_NAME_CHECKED(ACozyTerrainLayout,bSnapToEstateGrid)};
            if(Hidden.Contains(Root->GetFName())||Hidden.Contains(P.Property.GetFName())) return false;
            for(const FProperty* Parent:P.ParentProperties) if(Hidden.Contains(Parent->GetFName())) return false;
            return true;
        }));
        Details->OnFinishedChangingProperties().AddLambda([this](const FPropertyChangedEvent&){if(auto* P=GetPlan())P->RebuildPreview();if(Map)Map->Invalidate(EInvalidateWidgetReason::Paint);ScheduleLive();});
        auto Tool=[this](const TCHAR* Label,int M)->TSharedRef<SWidget>
        {
            static const TCHAR* Help[]={TEXT("요소를 클릭해 고르고 끌어 옮기세요. 사각 손잡이로 크기 · Delete 키로 삭제. 자유 모양은 점을 끌어 모양 변경 · 테두리 더블클릭으로 점 추가 · 점 오른쪽 클릭으로 뾰족/둥글게."),TEXT("지도에서 끌어 평지 범위를 그리세요 (그냥 클릭하면 기본 크기). 다 그리면 선택 도구로 돌아갑니다."),TEXT("지도에서 끌어 산·능선 범위를 그리세요 (그냥 클릭하면 기본 크기). 다 그리면 선택 도구로 돌아갑니다."),TEXT("지도에 경유점을 차례로 클릭하세요. 끝내려면 Esc 또는 선택 도구."),
                TEXT("지도에 점을 차례로 클릭하세요. 첫 점을 다시 누르거나 Enter·더블클릭으로 닫습니다 (3개 이상) · Backspace 마지막 점 지우기 · Esc 취소.")};
            // 지금 켜진 도구는 파랗게
            return SNew(SButton).Text(T(Label)).ButtonColorAndOpacity_Lambda([this,M](){return Mode==M?FLinearColor(.25,.65,1):FLinearColor::White;})
                .OnClicked_Lambda([this,M](){Mode=M;if(Map)Map->DrawPts.Reset();if(M==3||M==4)SelectNone();Message(Help[M]);return FReply::Handled();});
        };
        ChildSlot[SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight().Padding(8)[SNew(STextBlock).Text(T(TEXT("지형 편집기  ·  현재 레벨의 평지·산·길 설계"))).Font(FAppStyle::GetFontStyle("HeadingExtraSmall"))]
        +SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("새 설계"),[this](){NewPlan();})]
            +SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("저장된 설계 열기"),[this](){LoadExisting();})]
            +SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("선택 Landscape 연결"),[this](){BindSelected();})]
            +SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("3D 화면 맞추기"),[this](){FitView();})]
            +SHorizontalBox::Slot().FillWidth(1).Padding(6)[SNew(STextBlock).Text_Lambda([](){auto* W=GEditor->GetEditorWorldContext().World();return FText::FromString(W?W->GetName():TEXT("레벨 없음"));})]]
        +SVerticalBox::Slot().AutoHeight().Padding(6)[SNew(STextBlock).Text_Lambda([this](){auto* P=GetPlan();if(!P)return T(TEXT("① 새 설계 → ② 평지·산·길 → ③ 3D 확인 → ④ 적용 → ⑤ 맵 저장"));return FText::FromString(FString::Printf(TEXT("%.0f × %.0fm  |  %d개 지형 요소 · %d개 길  |  %s"),P->Recipe.MapSize.X,P->Recipe.MapSize.Y,P->Recipe.Landforms.Num(),P->Recipe.Routes.Num(),P->bHasUnappliedChanges?TEXT("미적용 변경 있음 · 적용 버튼 필요"):TEXT("Landscape 적용됨")));})]
        +SVerticalBox::Slot().FillHeight(1).Padding(4)[SNew(SSplitter)
            +SSplitter::Slot().Value(.15f)[SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight().Padding(3)[Tool(TEXT("선택·이동"),0)]
                +SVerticalBox::Slot().AutoHeight().Padding(3)[Tool(TEXT("평지 추가"),1)]
                +SVerticalBox::Slot().AutoHeight().Padding(3)[Tool(TEXT("산·능선 추가"),2)]
                +SVerticalBox::Slot().AutoHeight().Padding(3)[Tool(TEXT("자유 모양 추가"),4)]
                +SVerticalBox::Slot().AutoHeight().Padding(3)[Tool(TEXT("길 그리기"),3)]
                +SVerticalBox::Slot().AutoHeight().Padding(3)[Button(TEXT("선택 요소 삭제"),[this](){DeleteSelected();})]
                +SVerticalBox::Slot().AutoHeight().Padding(3)[Button(TEXT("모두 지우기"),[this](){Mutate([this](auto& P){P.Recipe.Landforms.Reset();P.Recipe.Routes.Reset();SelectNone();});Message(TEXT("평지·산·길을 모두 지웠습니다. 실제 지형은 적용 전까지 그대로입니다. 되돌리기로 복구할 수 있습니다."));})]
                // 등고선: 같은 높이를 잇는 선 · 간격은 옆 숫자로
                +SVerticalBox::Slot().AutoHeight().Padding(5)[SNew(SHorizontalBox)
                    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SCheckBox).IsChecked(ECheckBoxState::Checked).OnCheckStateChanged_Lambda([this](ECheckBoxState S){bContours=S==ECheckBoxState::Checked;Refresh();})[SNew(STextBlock).Text(T(TEXT("등고선"))).ToolTipText(T(TEXT("지도에 같은 높이를 잇는 흰 선을 정해진 간격(m)마다 긋습니다. 선이 촘촘하면 가파르고 넓으면 완만합니다. 실제 지형에는 영향이 없습니다.")))]]
                    +SHorizontalBox::Slot().FillWidth(1).Padding(6,0,0,0)[SNew(SNumericEntryBox<float>).AllowSpin(true).MinSliderValue(1.f).MaxSliderValue(20.f).MinValue(.5f).MaxValue(50.f).Delta(.5f).MaxFractionalDigits(1)
                        .Value_Lambda([this]()->TOptional<float>{auto* P=GetPlan();return P?TOptional<float>(P->ContourInterval):TOptional<float>();})
                        .OnValueChanged_Lambda([this](float V){if(auto* P=GetPlan()){P->ContourInterval=FMath::Clamp(V,.5f,50.f);if(Map)Map->Invalidate(EInvalidateWidgetReason::Paint);}})
                        .OnValueCommitted_Lambda([this](float V,ETextCommit::Type){if(auto* P=GetPlan()){P->Modify();P->ContourInterval=FMath::Clamp(V,.5f,50.f);P->MarkPackageDirty();if(Map)Map->Invalidate(EInvalidateWidgetReason::Paint);}})]
                    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(3,0)[SNew(STextBlock).Text(T(TEXT("m")))]]
                +SVerticalBox::Slot().AutoHeight().Padding(5)[SNew(SCheckBox).IsChecked(ECheckBoxState::Checked).OnCheckStateChanged_Lambda([this](ECheckBoxState S){bSlope=S==ECheckBoxState::Checked;Refresh();})[SNew(STextBlock).Text(T(TEXT("급경사 표시")))]]
                // 3D 미리보기 켜기·끄기 (아래 따로 있던 '미리보기' 버튼과 세부 정보 칸의 같은 체크를 여기 하나로)
                +SVerticalBox::Slot().AutoHeight().Padding(5)[SNew(SCheckBox).IsChecked_Lambda([this](){auto* P=GetPlan();return P&&P->bShowPreview?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}).OnCheckStateChanged_Lambda([this](ECheckBoxState S){if(auto* P=GetPlan()){P->bShowPreview=S==ECheckBoxState::Checked;P->RebuildPreview();}})[SNew(STextBlock).Text(T(TEXT("3D 미리보기"))).ToolTipText(T(TEXT("설계를 바꾼 곳만 3D 화면에 색으로 표시합니다 (주황 올라감 · 파랑 내려감 · 빨강 급경사)")))]]
                +SVerticalBox::Slot().AutoHeight().Padding(5)[SNew(SCheckBox).IsChecked_Lambda([this](){auto* P=GetPlan();return P&&P->bSnapToEstateGrid?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}).OnCheckStateChanged_Lambda([this](ECheckBoxState S){if(auto* P=GetPlan()){const FScopedTransaction Tx(T(TEXT("영지 칸 맞춤 전환")));P->Modify();P->bSnapToEstateGrid=S==ECheckBoxState::Checked;P->MarkPackageDirty();Refresh();}})[SNew(STextBlock).Text(T(TEXT("영지 칸 맞춤"))).ToolTipText(T(TEXT("초록 격자(영지) 안의 평지를 칸 크기와 층 높이에 맞춥니다. 영지 위치·칸 수·층 높이는 아래 '06 영지 칸'에서 바꿉니다.")))]]
                +SVerticalBox::Slot().AutoHeight().Padding(5)[SNew(SCheckBox).IsChecked_Lambda([this](){auto* P=GetPlan();return P&&P->bBakeOnApply?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}).OnCheckStateChanged_Lambda([this](ECheckBoxState S){if(auto* P=GetPlan()){P->Modify();P->bBakeOnApply=S==ECheckBoxState::Checked;P->MarkPackageDirty();}})[SNew(STextBlock).Text(T(TEXT("적용하면 원형 정리"))).ToolTipText(T(TEXT("'지형에 적용'을 누르면 평지·산·길 원형을 지형에 굳히고 목록을 비웁니다. 다음 설계는 바뀐 지형 위에서 시작합니다. '적용 전 높이 복구'를 누르면 지형과 원형이 함께 돌아옵니다.")))]]
                // 바로 반영: 값을 바꾸고 잠시 뒤 지형에 반영 (원형 정리·사본 없이)
                +SVerticalBox::Slot().AutoHeight().Padding(5)[SNew(SCheckBox).IsChecked_Lambda([this](){auto* P=GetPlan();return P&&P->bLiveApply?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}).OnCheckStateChanged_Lambda([this](ECheckBoxState S){if(auto* P=GetPlan()){P->Modify();P->bLiveApply=S==ECheckBoxState::Checked;P->MarkPackageDirty();LiveNote.Reset();if(P->bLiveApply)LiveDue=FPlatformTime::Seconds();else LiveDue=0;}})
                    .ToolTipText(T(TEXT("값을 바꾸면 지형에 바로 반영 · 먼저 한 번 '지형에 적용'이 필요")))[SNew(STextBlock).Text_Lambda([this](){return FText::FromString(LiveNote.IsEmpty()?FString(TEXT("바로 반영")):FString::Printf(TEXT("바로 반영  (%s)"),*LiveNote));})]]
                +SVerticalBox::Slot().AutoHeight().Padding(5)[SNew(STextBlock).AutoWrapText(true).Text(T(TEXT("청록 낮음\n노랑 높음\n빨강 급경사\n사각 손잡이 = 크기\n자유 모양 점: 네모 = 뾰족 · 동그라미 = 둥글게 (오른쪽 클릭으로 바꿈) · 테두리 더블클릭 = 점 추가\n주황 선 = 산 안쪽 띠 (눌러 고름)")))]]
            +SSplitter::Slot().Value(.53f)[SAssignNew(Map,SMap,SharedThis(this))]
            +SSplitter::Slot().Value(.32f)[SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(STextBlock).Text_Lambda([this](){auto* P=GetPlan();if(P&&P->Recipe.Landforms.IsValidIndex(Selected))return FText::FromString(P->Recipe.Landforms[Selected].Name);if(P&&P->Recipe.Routes.IsValidIndex(Route))return FText::FromString(P->Recipe.Routes[Route].Name);return T(TEXT("선택 요소 / 전체 설계"));})]
                // 종류·가장자리 방식 (평지·산 모두 · 바꿔도 다른 값은 그대로)
                +SVerticalBox::Slot().AutoHeight()[SNew(SBox).Visibility_Lambda([this](){return Show(Sel()!=nullptr);})[Choice(TEXT("종류"),TEXT("평지"),TEXT("산"),[this](){const auto* F=Sel();return F?(F->Type==ECozyLandform::Mountain?1:0):-1;},[this](int V){Mutate([this,V](ACozyTerrainLayout& P){if(P.Recipe.Landforms.IsValidIndex(Selected))P.Recipe.Landforms[Selected].Type=V?ECozyLandform::Mountain:ECozyLandform::Plateau;});},TEXT("평지: 정한 높이로 평평하게 · 산: 가운데가 가장 높은 언덕·산"))]]
                +SVerticalBox::Slot().AutoHeight()[SNew(SBox).Visibility_Lambda([this](){return Show(Sel()!=nullptr);})[Choice(TEXT("가장자리"),TEXT("연결 폭"),TEXT("경사 각도"),[this](){const auto* F=Sel();return F?(F->EdgeMode==ECozyEdgeMode::Angle?1:0):-1;},[this](int V){Mutate([this,V](ACozyTerrainLayout& P){if(P.Recipe.Landforms.IsValidIndex(Selected))P.Recipe.Landforms[Selected].EdgeMode=V?ECozyEdgeMode::Angle:ECozyEdgeMode::Width;});},TEXT("연결 폭: 주변 땅과 정한 폭(m) 안에서 부드럽게 이어짐 · 경사 각도: 정한 각도의 곧은 비탈"))]]
                +SVerticalBox::Slot().AutoHeight()[Number(TEXT("선택 높이 (m)"),-20,60,[this]()->TOptional<float>{auto* P=GetPlan();if(P&&P->Recipe.Landforms.IsValidIndex(Selected))return P->Recipe.Landforms[Selected].Height;if(P&&P->Recipe.Routes.IsValidIndex(Route)&&P->Recipe.Routes[Route].Points.IsValidIndex(Node))return P->Recipe.Routes[Route].Points[Node].Z;return {};},[this](float V){Mutate([this,V](auto& P){if(P.Recipe.Landforms.IsValidIndex(Selected))P.Recipe.Landforms[Selected].Height=FMath::Clamp(V,-200.f,200.f);else if(P.Recipe.Routes.IsValidIndex(Route)&&P.Recipe.Routes[Route].Points.IsValidIndex(Node))P.Recipe.Routes[Route].Points[Node].Z=FMath::Clamp(V,-200.f,200.f);});})]
                // 자유 모양은 점으로 모양을 정하므로 가로·세로 대신 감싼 크기만 보여 줌
                +SVerticalBox::Slot().AutoHeight().Padding(4,2)[SNew(STextBlock).Visibility_Lambda([this](){const auto* F=Sel();return Show(F&&IsFree(*F));}).Text_Lambda([this](){const auto* F=Sel();return F?FText::FromString(FString::Printf(TEXT("자유 모양 · 점 %d개 · 크기 %.1f × %.1fm (점을 끌어 바꿈)"),F->Points.Num(),F->Size.X,F->Size.Y)):FText::GetEmpty();})]
                +SVerticalBox::Slot().AutoHeight()[SNew(SBox).Visibility_Lambda([this](){const auto* F=Sel();return Show(!F||!IsFree(*F));})[Number(TEXT("가로 / 길 폭 (m)"),2,200,[this]()->TOptional<float>{auto* P=GetPlan();if(P&&P->Recipe.Landforms.IsValidIndex(Selected))return P->Recipe.Landforms[Selected].Size.X;if(P&&P->Recipe.Routes.IsValidIndex(Route))return P->Recipe.Routes[Route].Width;return {};},[this](float V){Mutate([this,V](auto& P){if(P.Recipe.Landforms.IsValidIndex(Selected))P.Recipe.Landforms[Selected].Size.X=FMath::Clamp(V,2.f,2000.f);else if(P.Recipe.Routes.IsValidIndex(Route))P.Recipe.Routes[Route].Width=FMath::Clamp(V,2.f,80.f);});})]]
                +SVerticalBox::Slot().AutoHeight()[SNew(SBox).Visibility_Lambda([this](){const auto* F=Sel();return Show(!F||!IsFree(*F));})[Number(TEXT("세로 (m)"),2,200,[this]()->TOptional<float>{auto* P=GetPlan();if(P&&P->Recipe.Landforms.IsValidIndex(Selected))return P->Recipe.Landforms[Selected].Size.Y;return {};},[this](float V){Mutate([this,V](auto& P){if(P.Recipe.Landforms.IsValidIndex(Selected))P.Recipe.Landforms[Selected].Size.Y=FMath::Clamp(V,2.f,2000.f);});})]]
                // 세부 정보 칸에서 원본 목록을 숨긴 대신 거기서만 고칠 수 있던 값을 여기로
                +SVerticalBox::Slot().AutoHeight()[Number(TEXT("방향 (도)"),0,360,[this]()->TOptional<float>{auto* P=GetPlan();if(P&&P->Recipe.Landforms.IsValidIndex(Selected))return P->Recipe.Landforms[Selected].Rotation;return {};},[this](float V){Mutate([this,V](auto& P){if(P.Recipe.Landforms.IsValidIndex(Selected))P.Recipe.Landforms[Selected].Rotation=FMath::Fmod(V,360.f);});})]
                +SVerticalBox::Slot().AutoHeight()[SNew(SBox).Visibility_Lambda([this](){const auto* F=Sel();return Show(!F||F->EdgeMode!=ECozyEdgeMode::Angle);})[Number(TEXT("주변 연결 폭 (m)"),1,60,[this]()->TOptional<float>{auto* P=GetPlan();if(P&&P->Recipe.Landforms.IsValidIndex(Selected))return P->Recipe.Landforms[Selected].Blend;if(P&&P->Recipe.Routes.IsValidIndex(Route))return P->Recipe.Routes[Route].Blend;return {};},[this](float V){Mutate([this,V](auto& P){if(P.Recipe.Landforms.IsValidIndex(Selected))P.Recipe.Landforms[Selected].Blend=FMath::Clamp(V,1.f,200.f);else if(P.Recipe.Routes.IsValidIndex(Route))P.Recipe.Routes[Route].Blend=FMath::Clamp(V,1.f,80.f);});})]]
                // 경사 각도 방식: 가장자리를 이 각도의 곧은 비탈로
                +SVerticalBox::Slot().AutoHeight()[SNew(SBox).Visibility_Lambda([this](){const auto* F=Sel();return Show(F&&F->EdgeMode==ECozyEdgeMode::Angle);})[Number(TEXT("경사 각도 (도)"),5,85,[this]()->TOptional<float>{const auto* F=Sel();if(F)return F->SlopeAngle;return {};},[this](float V){Mutate([this,V](ACozyTerrainLayout& P){if(P.Recipe.Landforms.IsValidIndex(Selected))P.Recipe.Landforms[Selected].SlopeAngle=FMath::Clamp(V,5.f,85.f);});})]]
                // 산만: 0%는 뾰족한 봉우리, 클수록 꼭대기가 넓고 옆면이 가파름
                +SVerticalBox::Slot().AutoHeight()[SNew(SBox).Visibility_Lambda([this](){const auto* F=Sel();return Show(F&&F->Type==ECozyLandform::Mountain&&F->EdgeMode==ECozyEdgeMode::Width);})[Number(TEXT("꼭대기 넓이 (%)"),0,90,[this]()->TOptional<float>{auto* P=GetPlan();if(P&&P->Recipe.Landforms.IsValidIndex(Selected)&&P->Recipe.Landforms[Selected].Type==ECozyLandform::Mountain)return P->Recipe.Landforms[Selected].PeakFlat;return {};},[this](float V){Mutate([this,V](auto& P){if(P.Recipe.Landforms.IsValidIndex(Selected)&&P.Recipe.Landforms[Selected].Type==ECozyLandform::Mountain)P.Recipe.Landforms[Selected].PeakFlat=FMath::Clamp(V,0.f,90.f);});})]]
                // 안쪽 띠 (자유 모양 산만): 띠와 띠 사이를 부드럽게 잇고 가장 안쪽 띠 안은 꼭대기 높이까지
                +SVerticalBox::Slot().AutoHeight().Padding(4,6,4,2)[SNew(STextBlock).AutoWrapText(true).Visibility_Lambda([this](){const auto* F=Sel();return Show(F&&HasBands(*F));}).Text_Lambda([this]()
                {
                    const auto* F=Sel();if(!F)return FText::GetEmpty();const int B=BandOf(*F);
                    if(B!=INDEX_NONE)return FText::FromString(FString::Printf(TEXT("안쪽 띠 %d / %d 고름 · 점 %d개 · %s  (점 끌기 · 선 더블클릭 점 추가 · Esc 바깥 모양으로)"),B+1,F->Bands.Num(),F->Bands[B].Points.Num(),*MetersText(F->Bands[B].Height)));
                    return FText::FromString(FString::Printf(TEXT("안쪽 띠 %d개  (주황 선을 눌러 고르기)"),F->Bands.Num()));
                })]
                +SVerticalBox::Slot().AutoHeight()[SNew(SBox).Visibility_Lambda([this](){const auto* F=Sel();return Show(F&&HasBands(*F));})[SNew(SHorizontalBox)
                    +SHorizontalBox::Slot().FillWidth(1).Padding(2)[SNew(SBox).ToolTipText(T(TEXT("산 안쪽에 같은 높이를 잇는 띠(등고선)를 직접 그립니다. 자유 모양 그리기와 같은 방법입니다.")))[Button(TEXT("띠 그리기"),[this](){StartBandDraw();})]]
                    +SHorizontalBox::Slot().FillWidth(1).Padding(2)[SNew(SBox).ToolTipText(T(TEXT("아래 '띠 수'만큼 띠를 무작위로 만듭니다. 있던 띠는 바뀝니다 (되돌리기로 복구).")))[Button(TEXT("띠 자동 만들기"),[this](){AutoBands();})]]]]
                +SVerticalBox::Slot().AutoHeight()[SNew(SBox).Visibility_Lambda([this](){const auto* F=Sel();return Show(F&&HasBands(*F));})[Number(TEXT("띠 수"),1,8,[this]()->TOptional<float>{return float(BandCount);},[this](float V){BandCount=FMath::Clamp(FMath::RoundToInt(V),1,8);})]]
                +SVerticalBox::Slot().AutoHeight()[SNew(SBox).Visibility_Lambda([this](){return Show(SelBand()!=nullptr);})[Number(TEXT("띠 높이 (m)"),-20,120,[this]()->TOptional<float>{if(const auto* B=SelBand())return B->Height;return {};},[this](float V){EditSel([this,V](FCozyLandform& L){if(L.Bands.IsValidIndex(Band))L.Bands[Band].Height=FMath::Clamp(V,-200.f,200.f);});})]]
                +SVerticalBox::Slot().AutoHeight().Padding(2)[SNew(SBox).Visibility_Lambda([this](){return Show(SelBand()!=nullptr);})[Button(TEXT("띠 지우기"),[this](){DeleteBand();})]]
                // 랜덤: 표면 울퉁불퉁 · 테두리 들쭉날쭉 (자유 모양만) · 씨앗을 바꾸면 같은 세기로 다른 모양
                +SVerticalBox::Slot().AutoHeight().Padding(4,6,4,2)[SNew(STextBlock).Visibility_Lambda([this](){return Show(Sel()!=nullptr);}).Text(T(TEXT("랜덤")))]
                +SVerticalBox::Slot().AutoHeight()[SNew(SBox).Visibility_Lambda([this](){return Show(Sel()!=nullptr);}).ToolTipText(T(TEXT("표면을 울퉁불퉁하게 하는 높이(m) · 0이면 매끈")))[Number(TEXT("울퉁불퉁 세기 (m)"),0,30,[this]()->TOptional<float>{if(const auto* F=Sel())return F->NoiseAmount;return {};},[this](float V){EditSel([V](FCozyLandform& L){L.NoiseAmount=FMath::Clamp(V,0.f,30.f);});})]]
                +SVerticalBox::Slot().AutoHeight()[SNew(SBox).Visibility_Lambda([this](){return Show(Sel()!=nullptr);}).ToolTipText(T(TEXT("울퉁불퉁 한 덩어리의 크기(m) · 작으면 자글자글, 크면 큰 언덕")))[Number(TEXT("울퉁불퉁 크기 (m)"),2,300,[this]()->TOptional<float>{if(const auto* F=Sel())return F->NoiseSize;return {};},[this](float V){EditSel([V](FCozyLandform& L){L.NoiseSize=FMath::Clamp(V,2.f,300.f);});})]]
                +SVerticalBox::Slot().AutoHeight()[SNew(SBox).Visibility_Lambda([this](){const auto* F=Sel();return Show(F&&IsFree(*F));}).ToolTipText(T(TEXT("테두리와 띠 선을 구불구불하게 미는 정도(m) · 0이면 그린 그대로")))[Number(TEXT("테두리 들쭉날쭉 (m)"),0,30,[this]()->TOptional<float>{if(const auto* F=Sel())return F->EdgeJitter;return {};},[this](float V){EditSel([V](FCozyLandform& L){L.EdgeJitter=FMath::Clamp(V,0.f,30.f);});})]]
                +SVerticalBox::Slot().AutoHeight()[SNew(SBox).Visibility_Lambda([this](){const auto* F=Sel();return Show(F&&IsFree(*F));}).ToolTipText(T(TEXT("구불거림 한 번의 길이(m) · 작으면 잘게, 크면 크게 구불거림")))[Number(TEXT("들쭉날쭉 간격 (m)"),2,120,[this]()->TOptional<float>{if(const auto* F=Sel())return F->EdgeJitterSize;return {};},[this](float V){EditSel([V](FCozyLandform& L){L.EdgeJitterSize=FMath::Clamp(V,2.f,200.f);});})]]
                +SVerticalBox::Slot().AutoHeight()[SNew(SBox).Visibility_Lambda([this](){return Show(Sel()!=nullptr);})[SNew(SHorizontalBox)
                    +SHorizontalBox::Slot().AutoWidth().Padding(2)[SNew(SBox).ToolTipText(T(TEXT("같은 세기로 다른 모양이 나오게 랜덤 씨앗을 바꿉니다 (되돌리기로 복구)")))[Button(TEXT("다시 섞기"),[this](){EditSel([](FCozyLandform& L){int32 S=L.RandomSeed;while(S==L.RandomSeed)S=FMath::Rand();L.RandomSeed=S;});if(const auto* F=Sel())Message(FString::Printf(TEXT("랜덤을 다시 섞었습니다 (씨앗 %d)."),F->RandomSeed));})]]
                    +SHorizontalBox::Slot().FillWidth(1).Padding(6,2).VAlign(VAlign_Center)[SNew(STextBlock).Text_Lambda([this](){const auto* F=Sel();return F?FText::FromString(FString::Printf(TEXT("씨앗 %d"),F->RandomSeed)):FText::GetEmpty();})]]]
                +SVerticalBox::Slot().AutoHeight().Padding(4,8,4,2)[SNew(STextBlock).Text_Lambda([this](){return FText::FromString(FString::Printf(TEXT("요소 목록 (%d개)"),Items.Num()));})]
                +SVerticalBox::Slot().AutoHeight().Padding(2)[SNew(SBox).HeightOverride(150)[SAssignNew(List,SListView<TSharedPtr<int32>>).ListItemsSource(&Items).SelectionMode(ESelectionMode::Single)
                    .OnGenerateRow_Lambda([this](TSharedPtr<int32> Item,const TSharedRef<STableViewBase>& Owner){const int32 Id=*Item;return SNew(STableRow<TSharedPtr<int32>>,Owner)[SNew(STextBlock).Text_Lambda([this,Id](){return ItemText(Id);})];})
                    .OnSelectionChanged_Lambda([this](TSharedPtr<int32> Item,ESelectInfo::Type How){if(How==ESelectInfo::Direct||!Item)return;SelectNone();if(*Item>=0)Selected=*Item;else Route=-1-*Item;Mode=0;if(Map)Map->Invalidate(EInvalidateWidgetReason::Paint);})]]
                +SVerticalBox::Slot().AutoHeight().Padding(4,8,4,2)[SNew(STextBlock).Text(T(TEXT("전체 설계 · 재질 · 설정 이름")))]
                +SVerticalBox::Slot().FillHeight(1)[Details.ToSharedRef()]]]
        +SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("되돌리기"),[this](){GEditor->UndoTransaction();SelectNone();Refresh();ScheduleLive();})]
            +SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("다시 실행"),[this](){GEditor->RedoTransaction();SelectNone();Refresh();ScheduleLive();})]
            +SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("설정 저장"),[this](){if(auto* P=GetPlan()){FString R;P->SavePreset(R);Message(R);}})]
            +SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("설정 불러오기"),[this](){if(auto* P=GetPlan()){FString R;P->LoadPreset(R);SelectNone();Message(R);}})]
            +SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("지형에 적용"),[this](){if(auto* P=GetPlan()){FString R;if(P->Apply(R))SelectNone();Message(R);}})]
            +SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("적용 전 높이 복구"),[this](){if(auto* P=GetPlan()){FString R;if(P->Restore(R))SelectNone();Message(R);}})]]
        +SVerticalBox::Slot().AutoHeight().Padding(8)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this](){return FText::FromString(Status);})]];
        LastWorld=GEditor->GetEditorWorldContext().World();LoadExisting();
    }
    virtual void Tick(const FGeometry& G,double Tm,float Dt) override
    {
        SCompoundWidget::Tick(G,Tm,Dt);
        auto* W=GEditor->GetEditorWorldContext().World();if(W!=LastWorld.Get()){LastWorld=W;Plan=nullptr;SelectNone();Details->SetObject(nullptr);Status=TEXT("레벨이 변경됐습니다. 이 레벨의 설계를 열거나 새로 만드세요.");LiveDue=0;}
        // 바로 반영: 마지막 변경 0.35초 뒤 · 막대나 지도에서 끄는 중이면 0.35초마다 한 번
        if(LiveDue>0)
        {
            auto* P=GetPlan();const double Now=FPlatformTime::Seconds();const bool bDragging=SliderTx.IsValid()||(Map&&Map->Drag.IsValid());
            if(!P||!P->bLiveApply)LiveDue=0;
            else if(Now>=LiveDue||(bDragging&&Now-LastLive>=.35))
            {
                LiveDue=0;LastLive=Now;FString R;
                if(P->ApplyLive(R))LiveNote=FString::Printf(TEXT("반영됨 %s"),*FDateTime::Now().ToString(TEXT("%H:%M:%S")));
                else{LiveNote=TEXT("반영 실패");Status=FString::Printf(TEXT("바로 반영 실패: %s"),*R);}
                if(Map)Map->Invalidate(EInvalidateWidgetReason::Paint);
            }
        }
    }
};

// 지도는 실제 지형 비율 그대로 창 가운데에 그린다 (창을 늘리고 줄여도 찌그러지지 않게 · 남는 곳은 빈 여백)
static void FitMap(const FVector2D& Size,const FVector2D& Span,FVector2D& Origin,FVector2D& Drawn){const double S=FMath::Min(Size.X/FMath::Max(1.,Span.X),Size.Y/FMath::Max(1.,Span.Y));Drawn=Span*S;Origin=(Size-Drawn)*.5;}
static FVector2D ToMap(FVector2D P,const FVector2D& Size,const FVector2D& Span){FVector2D O,D;FitMap(Size,Span,O,D);return O+FVector2D((P.X/Span.X+.5)*D.X,(.5-P.Y/Span.Y)*D.Y);}
static FVector2D FromMap(FVector2D P,const FVector2D& Size,const FVector2D& Span){FVector2D O,D;FitMap(Size,Span,O,D);P-=O;return FVector2D((P.X/D.X-.5)*Span.X,(.5-P.Y/D.Y)*Span.Y);}
// 지도 1픽셀이 몇 m인지 (점 끼워 넣기 거리 판정용)
static double PixelMeters(const FVector2D& Size,const FVector2D& Span){FVector2D O,D;FitMap(Size,Span,O,D);return Span.X/FMath::Max(1.,D.X);}
int SMap::HitPoint(const FVector2D& Size,const ACozyTerrainLayout& P,const FCozyLandform& F,const TArray<FCozyShapePoint>& Pts,FVector2D Mouse) const
{
    int Best=INDEX_NONE;double Near=9;
    for(int K=0;K<Pts.Num();++K){const double D=FVector2D::Distance(Mouse,ToMap(ToWorld(F,Pts[K].Position),Size,P.Recipe.MapSize));if(D<=Near){Near=D;Best=K;}}
    return Best;
}
bool SMap::HitBand(const FVector2D& Size,const ACozyTerrainLayout& P,const FCozyLandform& F,FVector2D Mouse,int& OutBand,int& OutPoint) const
{
    OutBand=OutPoint=INDEX_NONE;if(!HasBands(F))return false;
    const FVector2D Local=ToLocal(F,FromMap(Mouse,Size,P.Recipe.MapSize));const float Reach=float(8*PixelMeters(Size,P.Recipe.MapSize));
    // 위(안쪽·나중에 그린 띠)부터 · 점 손잡이가 선보다 먼저
    for(int B=F.Bands.Num()-1;B>=0;--B){const int H=HitPoint(Size,P,F,F.Bands[B].Points,Mouse);if(H!=INDEX_NONE){OutBand=B;OutPoint=H;return true;}}
    for(int B=F.Bands.Num()-1;B>=0;--B){const FCozyPolyShape S=LinePoly(F,B);if(S.IsValid()&&CozyTerrainShape::FindSpan(S,Local,Reach)!=INDEX_NONE){OutBand=B;return true;}}
    return false;
}
void SMap::FinishDraw(SPanel& O,ACozyTerrainLayout& P){if(O.Mode==5)FinishBand(O,P);else FinishFree(O,P);}
void SMap::FinishBand(SPanel& O,ACozyTerrainLayout& P)
{
    if(DrawPts.Num()<3){O.Message(TEXT("점을 3개 이상 찍어야 띠를 닫을 수 있습니다."));return;}
    const FCozyLandform* F=O.Sel();
    if(!F||!HasBands(*F)){DrawPts.Reset();bHover=false;O.Mode=0;O.Message(TEXT("띠는 자유 모양 산을 고른 뒤 그릴 수 있습니다."));return;}
    // 산 중심 기준·회전 전 좌표로 (바깥 모양 점과 같은 기준)
    TArray<FCozyShapePoint> Pts;TArray<FVector2D> Local;
    for(const FVector2D& V:DrawPts){FCozyShapePoint S;S.Position=R1(ToLocal(*F,V));Pts.Add(S);Local.Add(S.Position);}
    // 기본 높이: 이 띠를 감싼 띠(없으면 산기슭 0m)와 꼭대기의 가운데
    const FVector2D Mid=CozyTerrainShape::Centroid(Local);float Below=0;
    for(const FCozyContourBand& B:F->Bands)if(B.Points.Num()>=3&&B.Height>Below){FCozyPolyShape S;CozyTerrainShape::BuildPolyline(B.Points,12,S);if(CozyTerrainShape::Contains(S,Mid))Below=B.Height;}
    const float Height=FMath::RoundToFloat((Below+F->Height)*5)/10;
    DrawPts.Reset();bHover=false;O.Mode=0;
    O.Mutate([&O,&Pts,Height](ACozyTerrainLayout& A)
    {
        if(!A.Recipe.Landforms.IsValidIndex(O.Selected))return;
        FCozyContourBand B;B.Points=Pts;B.Height=Height;O.Band=A.Recipe.Landforms[O.Selected].Bands.Add(B);O.Point=INDEX_NONE;
    });
    if(const FCozyContourBand* B=O.SelBand())O.Message(FString::Printf(TEXT("안쪽 띠를 만들었습니다 (점 %d개 · %s). 오른쪽 '띠 높이'로 높이를 바꾸세요. 선택 도구로 돌아갔습니다."),B->Points.Num(),*MetersText(B->Height)));
}
void SMap::FinishFree(SPanel& O,ACozyTerrainLayout& P)
{
    if(DrawPts.Num()<3){O.Message(TEXT("점을 3개 이상 찍어야 모양을 닫을 수 있습니다."));return;}
    const FVector2D Center=R1(CozyTerrainShape::Centroid(DrawPts));const TArray<FVector2D> Pts=DrawPts;
    DrawPts.Reset();bHover=false;O.Mode=0;
    O.Mutate([&O,Center,&Pts](ACozyTerrainLayout& A)
    {
        FCozyLandform F;F.Shape=ECozyLandformShape::Free;F.Type=ECozyLandform::Plateau;F.EdgeMode=ECozyEdgeMode::Width;F.Blend=8;F.Center=Center;
        for(const FVector2D& V:Pts){FCozyShapePoint S;S.Position=R1(V-Center);F.Points.Add(S);}
        // 중심의 지금 땅 높이 (영지 안이면 맞춤에서 층 높이로)
        F.Height=FMath::RoundToFloat(A.Sample(Center)*10)/10;
        F.Name=FString::Printf(TEXT("자유 모양 %d"),A.Recipe.Landforms.Num()+1);FitFreeSize(F);
        O.SelectNone();O.Selected=A.Recipe.Landforms.Add(F);
    });
    if(const FCozyLandform* F=O.Sel())O.Message(FString::Printf(TEXT("'%s'을 만들었습니다 (점 %d개 · %.1fm). 오른쪽에서 평지/산 · 가장자리 방식을 고르세요. 선택 도구로 돌아갔습니다."),*F->Name,F->Points.Num(),F->Height));
}
int32 SMap::OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 L,const FWidgetStyle&,bool) const
{
    const auto Owner=Panel.Pin();const auto* P=Owner?Owner->GetPlan():nullptr;const FVector2D Size=G.GetLocalSize();
    const FSlateBrush* Brush=FAppStyle::GetBrush("WhiteBrush");
    FSlateDrawElement::MakeBox(Out,L,G.ToPaintGeometry(),Brush,ESlateDrawEffect::None,FLinearColor(.025,.04,.05));
    if(!P){FSlateDrawElement::MakeText(Out,L+1,G.ToPaintGeometry(),TEXT("새 설계 또는 선택 Landscape 연결로 시작"),FAppStyle::GetFontStyle("NormalFont"),ESlateDrawEffect::None,FLinearColor::White);return L+1;}
    FVector2D MO,MD;FitMap(Size,P->Recipe.MapSize,MO,MD);
    constexpr int N=48;float Heights[N+1][N+1];float Low=FLT_MAX,High=-FLT_MAX;
    for(int Y=0;Y<=N;++Y)for(int X=0;X<=N;++X){float Z=P->Sample(FromMap(MO+FVector2D(double(X)/N*MD.X,double(Y)/N*MD.Y),Size,P->Recipe.MapSize));Heights[Y][X]=Z;Low=FMath::Min(Low,Z);High=FMath::Max(High,Z);}
    auto Line=[&](FVector2D A,FVector2D B,FLinearColor C,float Width=1.f){TArray<FVector2D> V={A,B};FSlateDrawElement::MakeLines(Out,L+2,G.ToPaintGeometry(),V,ESlateDrawEffect::None,C,true,Width);};
    for(int Y=0;Y<N;++Y)for(int X=0;X<N;++X)
    {
        const float Z=Heights[Y][X],T=(Z-Low)/FMath::Max(1.f,High-Low);
        FLinearColor C=FMath::Lerp(FLinearColor(.04,.25,.25),FLinearColor(.88,.65,.25),T);
        const float DX=(Heights[Y][X+1]-Z)/(P->Recipe.MapSize.X/N),DY=(Heights[Y+1][X]-Z)/(P->Recipe.MapSize.Y/N);
        if(Owner->bSlope&&FMath::RadiansToDegrees(FMath::Atan(FMath::Sqrt(DX*DX+DY*DY)))>P->SlopeLimit)C=FLinearColor(.75,.12,.07);
        const FVector2D A=MO+FVector2D(double(X)/N*MD.X,double(Y)/N*MD.Y),Cell(MD.X/N+1,MD.Y/N+1);
        FSlateDrawElement::MakeBox(Out,L+1,G.ToPaintGeometry(FVector2f(Cell),FSlateLayoutTransform(FVector2f(A))),Brush,ESlateDrawEffect::None,C);
        if(Owner->bContours)
        {
            // 정사각 셀 변의 등고선 교점을 선형 보간한다.
            const FVector2D Corners[]={A,A+FVector2D(MD.X/N,0),A+FVector2D(MD.X/N,MD.Y/N),A+FVector2D(0,MD.Y/N)};
            const float Values[]={Z,Heights[Y][X+1],Heights[Y+1][X+1],Heights[Y+1][X]};
            float Min=Z,Max=Z;for(float V:Values){Min=FMath::Min(Min,V);Max=FMath::Max(Max,V);}
            const float Gap=FMath::Max(.5f,P->ContourInterval);
            for(int Band=FMath::CeilToInt(Min/Gap);Band<=FMath::FloorToInt(Max/Gap)&&Band<FMath::CeilToInt(Min/Gap)+50;++Band)
            {
                TArray<FVector2D> Hits;float Level=Band*Gap;
                for(int E=0;E<4;++E){int B=(E+1)%4;if((Values[E]<Level)!=(Values[B]<Level))Hits.Add(FMath::Lerp(Corners[E],Corners[B],double((Level-Values[E])/(Values[B]-Values[E]))));}
                for(int I=1;I<Hits.Num();I+=2)Line(Hits[I-1],Hits[I],FLinearColor(1,1,1,.25));
            }
        }
    }
    if(P->bSnapToEstateGrid&&P->CellSize>0)
    {
        // 영지 칸 격자: 칸 선은 옅게, 테두리는 진하게
        const FBox2D Estate=P->EstateBounds();const FLinearColor Grid(.55,1,.55,.22),Edge(.55,1,.55,.9);
        for(int X=0;X<=P->EstateCells.X;++X){const double W=Estate.Min.X+X*P->CellSize;Line(ToMap(FVector2D(W,Estate.Min.Y),Size,P->Recipe.MapSize),ToMap(FVector2D(W,Estate.Max.Y),Size,P->Recipe.MapSize),X%P->EstateCells.X?Grid:Edge,X%P->EstateCells.X?1.f:2.f);}
        for(int Y=0;Y<=P->EstateCells.Y;++Y){const double H=Estate.Min.Y+Y*P->CellSize;Line(ToMap(FVector2D(Estate.Min.X,H),Size,P->Recipe.MapSize),ToMap(FVector2D(Estate.Max.X,H),Size,P->Recipe.MapSize),Y%P->EstateCells.Y?Grid:Edge,Y%P->EstateCells.Y?1.f:2.f);}
        FSlateDrawElement::MakeText(Out,L+3,G.ToPaintGeometry(FVector2f(Size),FSlateLayoutTransform(FVector2f(ToMap(FVector2D(Estate.Min.X,Estate.Max.Y),Size,P->Recipe.MapSize)+FVector2D(2,-16)))),FString::Printf(TEXT("영지 %d×%d칸"),P->EstateCells.X,P->EstateCells.Y),FAppStyle::GetFontStyle("SmallFont"),ESlateDrawEffect::None,Edge);
    }
    for(int I=0;I<P->Recipe.Landforms.Num();++I)
    {
        const auto& F=P->Recipe.Landforms[I];const FLinearColor C=Owner->Selected==I?FLinearColor(.1,.9,1):FLinearColor(.95,.95,.85);
        if(IsFree(F))
        {
            // 자유 모양: 곡선을 꺾은선으로 펼쳐 그림 (테두리 들쭉날쭉까지 · 지형과 같게)
            const int SelBand=Owner->Selected==I?Owner->BandOf(F):INDEX_NONE;
            const FCozyPolyShape S=LinePoly(F,INDEX_NONE);
            if(S.IsValid()){TArray<FVector2D> V;for(const FVector2D& Q:S.Poly)V.Add(ToMap(ToWorld(F,Q),Size,P->Recipe.MapSize));V.Add(FVector2D(V[0]));FSlateDrawElement::MakeLines(Out,L+2,G.ToPaintGeometry(),V,ESlateDrawEffect::None,C,true,Owner->Selected==I&&SelBand==INDEX_NONE?3.f:Owner->Selected==I?2.f:1.f);}
            // 안쪽 띠: 주황 선 + 높이 글자 (고른 띠는 굵게)
            if(HasBands(F))for(int B=0;B<F.Bands.Num();++B)
            {
                const FCozyPolyShape BS=LinePoly(F,B);if(!BS.IsValid())continue;
                const FLinearColor BC=SelBand==B?FLinearColor(1,.75,.2):FLinearColor(1,.55,.15,Owner->Selected==I?.95f:.6f);
                TArray<FVector2D> V;int Top=0;for(const FVector2D& Q:BS.Poly){V.Add(ToMap(ToWorld(F,Q),Size,P->Recipe.MapSize));if(V.Last().Y<V[Top].Y)Top=V.Num()-1;}
                const FVector2D LabelAt=V[Top]+FVector2D(3,-15);V.Add(FVector2D(V[0]));
                FSlateDrawElement::MakeLines(Out,L+2,G.ToPaintGeometry(),V,ESlateDrawEffect::None,BC,true,SelBand==B?3.f:1.5f);
                FSlateDrawElement::MakeText(Out,L+3,G.ToPaintGeometry(FVector2f(Size),FSlateLayoutTransform(FVector2f(LabelAt))),MetersText(F.Bands[B].Height),FAppStyle::GetFontStyle("SmallFont"),ESlateDrawEffect::None,BC);
            }
            // 점 손잡이는 고른 대상(띠를 골랐으면 그 띠, 아니면 바깥 모양)만
            const TArray<FCozyShapePoint>& Pts=Owner->Selected==I?Owner->EditPoints(F):F.Points;const FLinearColor HC=SelBand!=INDEX_NONE?FLinearColor(1,.75,.2):C;
            if(Owner->Selected==I)for(int K=0;K<Pts.Num();++K)
            {
                // 점 손잡이: 네모 = 뾰족 · 동그라미 = 둥글게 · 고른 점은 노랑
                const FVector2D H=ToMap(ToWorld(F,Pts[K].Position),Size,P->Recipe.MapSize);const FLinearColor PC=Owner->Point==K?(SelBand!=INDEX_NONE?FLinearColor::White:FLinearColor(1,.85,.1)):HC;
                if(Pts[K].bSharp)FSlateDrawElement::MakeBox(Out,L+4,G.ToPaintGeometry(FVector2f(10,10),FSlateLayoutTransform(FVector2f(H-FVector2D(5,5)))),Brush,ESlateDrawEffect::None,PC);
                else{TArray<FVector2D> Ring;for(int A=0;A<=12;++A){const double R=A*2*PI/12;Ring.Add(H+FVector2D(FMath::Cos(R),FMath::Sin(R))*5.5);}FSlateDrawElement::MakeLines(Out,L+4,G.ToPaintGeometry(),Ring,ESlateDrawEffect::None,PC,true,2.5f);}
            }
        }
        else for(int K=0;K<48;++K){auto Pt=[&](int A){double R=A*2*PI/48;return ToMap(F.Center+FVector2D(FMath::Cos(R)*F.Size.X*.5,FMath::Sin(R)*F.Size.Y*.5).GetRotated(F.Rotation),Size,P->Recipe.MapSize);};Line(Pt(K),Pt(K+1),C,Owner->Selected==I?3:1);}
        const FVector2D Center=ToMap(F.Center,Size,P->Recipe.MapSize);
        // 이름·높이 글자는 요소가 적을 때만 전부, 많으면 선택한 것만 (지도가 글자로 덮이지 않게)
        if(Owner->Selected==I||P->Recipe.Landforms.Num()<=12)FSlateDrawElement::MakeText(Out,L+3,G.ToPaintGeometry(FVector2f(Size),FSlateLayoutTransform(FVector2f(Center+FVector2D(4,4)))),FString::Printf(TEXT("%s\n%.1fm"),*F.Name,F.Height),FAppStyle::GetFontStyle("NormalFont"),ESlateDrawEffect::None,FLinearColor::White);
        if(Owner->Selected==I&&!IsFree(F)){FVector2D H=ToMap(F.Center+F.Size*.5,Size,P->Recipe.MapSize);FSlateDrawElement::MakeBox(Out,L+4,G.ToPaintGeometry(FVector2f(12,12),FSlateLayoutTransform(FVector2f(H-FVector2D(6,6)))),Brush,ESlateDrawEffect::None,C);}
    }
    for(int R=0;R<P->Recipe.Routes.Num();++R)
    {
        const auto& Path=P->Recipe.Routes[R];for(int I=0;I<Path.Points.Num();++I)
        {
            FVector2D B=ToMap(FVector2D(Path.Points[I]),Size,P->Recipe.MapSize);
            if(I){FVector2D A=ToMap(FVector2D(Path.Points[I-1]),Size,P->Recipe.MapSize);double Slope=FMath::RadiansToDegrees(FMath::Atan2(FMath::Abs(Path.Points[I].Z-Path.Points[I-1].Z),FMath::Max(.001,FVector::Dist2D(Path.Points[I],Path.Points[I-1]))));Line(A,B,Slope>P->SlopeLimit?FLinearColor(1,.1,.1):FLinearColor(.1,.65,1),4);}
            FSlateDrawElement::MakeBox(Out,L+4,G.ToPaintGeometry(FVector2f(9,9),FSlateLayoutTransform(FVector2f(B-FVector2D(4.5,4.5)))),Brush,ESlateDrawEffect::None,FLinearColor(.1,.8,1));
        }
    }
    if(Owner->IsDrawing()&&DrawPts.Num())
    {
        // 그리는 중인 자유 모양(노랑)·띠(주황): 찍은 점을 곧게 잇고, 마우스까지·첫 점으로 돌아가는 선은 점선
        const bool bBandDraw=Owner->Mode==5;
        const FLinearColor Draw=bBandDraw?FLinearColor(1,.55,.15):FLinearColor(1,.85,.1),Faint=bBandDraw?FLinearColor(1,.55,.15,.45):FLinearColor(1,.85,.1,.45);
        TArray<FVector2D> V;for(const FVector2D& Q:DrawPts)V.Add(ToMap(Q,Size,P->Recipe.MapSize));
        if(V.Num()>1)FSlateDrawElement::MakeLines(Out,L+3,G.ToPaintGeometry(),V,ESlateDrawEffect::None,Draw,true,2.f);
        auto Dashed=[&](FVector2D A,FVector2D B){const double Len=FVector2D::Distance(A,B);const FVector2D Dir=(B-A)/FMath::Max(Len,.001);for(double D=0;D<Len;D+=10)Line(A+Dir*D,A+Dir*FMath::Min(D+6,Len),Faint,1.5f);};
        const FVector2D End=bHover?HoverPixel:V.Last();
        if(bHover)Dashed(V.Last(),HoverPixel);
        if(V.Num()>=2)Dashed(End,V[0]);
        // 첫 점은 크게 (가까이 가면 흰색 = 누르면 닫힘)
        const bool bNearFirst=bHover&&V.Num()>=3&&FVector2D::Distance(HoverPixel,V[0])<=10;
        for(int K=0;K<V.Num();++K){const float S=K?8.f:12.f;FSlateDrawElement::MakeBox(Out,L+4,G.ToPaintGeometry(FVector2f(S,S),FSlateLayoutTransform(FVector2f(V[K]-FVector2D(S*.5,S*.5)))),Brush,ESlateDrawEffect::None,K==0&&bNearFirst?FLinearColor::White:Draw);}
    }
    return L+4;
}

FReply SMap::OnMouseButtonDown(const FGeometry& G,const FPointerEvent& E)
{
    const auto O=Panel.Pin();auto* P=O?O->GetPlan():nullptr;if(!P||GEditor->PlayWorld)return FReply::Unhandled();
    const FVector2D Size=G.GetLocalSize(),Mouse=G.AbsoluteToLocal(E.GetScreenSpacePosition()),Pos=FromMap(Mouse,Size,P->Recipe.MapSize);
    // 고른 자유 모양의 점을 오른쪽 클릭 → 뾰족하게/둥글게 전환
    if(E.GetEffectingButton()==EKeys::RightMouseButton)
    {
        // 고른 대상(띠 또는 바깥 모양)의 점 · 고른 띠가 없으면 띠 점을 눌러도 그 띠를 고르며 바꿈
        const FCozyLandform* F=O->Mode==0?O->Sel():nullptr;int H=F&&IsFree(*F)?HitPoint(Size,*P,*F,O->EditPoints(*F),Mouse):INDEX_NONE;
        if(H==INDEX_NONE&&F&&O->BandOf(*F)==INDEX_NONE){int B,K;if(HitBand(Size,*P,*F,Mouse,B,K)&&K!=INDEX_NONE){O->Band=B;H=K;}}
        if(H==INDEX_NONE)return FReply::Unhandled();
        O->Point=H;O->Mutate([O,H](ACozyTerrainLayout& A){if(A.Recipe.Landforms.IsValidIndex(O->Selected)){auto& Pts=O->EditPoints(A.Recipe.Landforms[O->Selected]);if(Pts.IsValidIndex(H))Pts[H].bSharp=!Pts[H].bSharp;}});
        F=O->Sel();if(F&&O->EditPoints(*F).IsValidIndex(H))O->Message(O->EditPoints(*F)[H].bSharp?TEXT("점을 뾰족하게 바꿨습니다 (다시 오른쪽 클릭하면 둥글게)"):TEXT("점을 둥글게 바꿨습니다 (다시 오른쪽 클릭하면 뾰족하게)"));
        return FReply::Handled().SetUserFocus(SharedThis(this));
    }
    if(E.GetEffectingButton()!=EKeys::LeftMouseButton)return FReply::Unhandled();
    // 지도 밖 여백을 누르면 아무것도 하지 않음
    if(FMath::Abs(Pos.X)>P->Recipe.MapSize.X*.5+1||FMath::Abs(Pos.Y)>P->Recipe.MapSize.Y*.5+1)return FReply::Handled();
    if(O->IsDrawing())
    {
        // 첫 점 가까이(10픽셀) 누르면 닫기 · 아니면 점 추가 (0.1m 단위)
        if(DrawPts.Num()>=3&&FVector2D::Distance(Mouse,ToMap(DrawPts[0],Size,P->Recipe.MapSize))<=10){FinishDraw(*O,*P);return FReply::Handled().SetUserFocus(SharedThis(this));}
        const FVector2D Q=R1(FVector2D(FMath::Clamp(Pos.X,-P->Recipe.MapSize.X*.5,P->Recipe.MapSize.X*.5),FMath::Clamp(Pos.Y,-P->Recipe.MapSize.Y*.5,P->Recipe.MapSize.Y*.5)));
        if(DrawPts.Num()<128&&(DrawPts.IsEmpty()||FVector2D::Distance(DrawPts.Last(),Q)>.05))DrawPts.Add(Q);
        O->Status=FString::Printf(TEXT("점 %d개 · 첫 점을 다시 누르거나 Enter·더블클릭으로 닫기 (3개 이상) · Backspace 마지막 점 지우기 · Esc 취소"),DrawPts.Num());
        HoverPixel=Mouse;bHover=true;Invalidate(EInvalidateWidgetReason::Paint);return FReply::Handled().SetUserFocus(SharedThis(this));
    }
    if(O->Mode==3)
    {
        O->Mutate([&](auto& A){if(!A.Recipe.Routes.IsValidIndex(O->Route)){O->Route=A.Recipe.Routes.AddDefaulted();O->Selected=INDEX_NONE;}auto& R=A.Recipe.Routes[O->Route];if(R.Points.Num()<256){R.Points.Add(FVector(Pos.X,Pos.Y,A.Sample(Pos)));O->Node=R.Points.Num()-1;}});return FReply::Handled();
    }
    if(O->Mode==1||O->Mode==2)
    {
        // 기존 요소 위를 누르면 새로 만들지 않고 그 요소를 고른다 (아래 선택 처리)
        if(O->HitLandform(*P,Pos)==INDEX_NONE)
        {
            const bool bPlateau=O->Mode==1;
            Drag=MakeUnique<FScopedTransaction>(T(bPlateau?TEXT("평지 그리기"):TEXT("산·능선 그리기")));P->Modify();
            FCozyLandform F;F.Type=bPlateau?ECozyLandform::Plateau:ECozyLandform::Mountain;F.Center=Pos;F.Size=FVector2D(2,2);
            // 누른 곳 땅 높이를 0.1m 단위로 (영지 안이면 아래 맞춤에서 층 높이로)
            F.Height=bPlateau?FMath::RoundToFloat(P->Sample(Pos)*10)/10:25;
            F.Name=FString::Printf(TEXT("%s %d"),bPlateau?TEXT("평지"):TEXT("산·능선"),P->Recipe.Landforms.Num()+1);
            O->Selected=P->Recipe.Landforms.Add(F);O->Route=O->Node=O->Point=O->Band=INDEX_NONE;
            bCreating=true;bResize=bPointDrag=false;CreateStart=FVector2D(FMath::RoundToDouble(Pos.X*10)/10,FMath::RoundToDouble(Pos.Y*10)/10);CreateStartPixel=Mouse;
            P->bHasUnappliedChanges=true;P->bShowPreview=true;O->Refresh();
            return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this));
        }
        O->Mode=0;
    }
    bResize=bPointDrag=false;bool bKeep=false;
    if(const FCozyLandform* F=O->Sel())
    {
        if(IsFree(*F))
        {
            // 점 손잡이(고른 띠 또는 바깥 모양) → 그 점 끌기 · 다른 띠의 선·점 → 그 띠 고르기 · 테두리 가까이 → 바깥 모양 고른 채로 (밖이어도 · 더블클릭 점 추가가 되게)
            const int H=HitPoint(Size,*P,*F,O->EditPoints(*F),Mouse);int HB,HK;
            if(H!=INDEX_NONE){O->Point=H;bPointDrag=true;}
            else if(HitBand(Size,*P,*F,Mouse,HB,HK))
            {
                O->Band=HB;O->Point=HK;bPointDrag=HK!=INDEX_NONE;bKeep=true;
                O->Status=FString::Printf(TEXT("안쪽 띠 %d 고름 (%s) · 점 끌기 · 선 더블클릭 점 추가 · 점 오른쪽 클릭 뾰족/둥글게 · Delete 점(또는 띠) 지우기 · Esc 바깥 모양으로"),HB+1,*MetersText(F->Bands[HB].Height));
            }
            else
            {
                const FCozyPolyShape S=LinePoly(*F,INDEX_NONE);O->Point=INDEX_NONE;
                bKeep=S.IsValid()&&CozyTerrainShape::FindSpan(S,ToLocal(*F,Pos),float(8*PixelMeters(Size,P->Recipe.MapSize)))!=INDEX_NONE;
                if(bKeep)O->Band=INDEX_NONE;
            }
        }
        else bResize=FVector2D::Distance(Mouse,ToMap(F->Center+F->Size*.5,Size,P->Recipe.MapSize))<14;
    }
    if(!bResize&&!bPointDrag&&!bKeep)
    {
        O->SelectNone();
        for(int R=0;R<P->Recipe.Routes.Num();++R)for(int I=0;I<P->Recipe.Routes[R].Points.Num();++I)
            if(FVector2D::Distance(Mouse,ToMap(FVector2D(P->Recipe.Routes[R].Points[I]),Size,P->Recipe.MapSize))<13){O->Route=R;O->Node=I;}
        if(O->Route==INDEX_NONE)O->Selected=O->HitLandform(*P,Pos);
        // 안 고른 산의 띠 선을 눌렀으면 산과 그 띠를 함께 고름 (끌면 산 전체가 움직임)
        int HB,HK;if(const FCozyLandform* F=O->Sel())if(HitBand(Size,*P,*F,Mouse,HB,HK)){O->Band=HB;O->Status=FString::Printf(TEXT("안쪽 띠 %d 고름 (%s) · 점을 끌어 모양 · 오른쪽 '띠 높이'로 높이 · Esc 바깥 모양으로"),HB+1,*MetersText(F->Bands[HB].Height));}
    }
    // 통째로 옮길 때 누른 곳과 중심의 차이를 유지 (중심이 마우스로 튀지 않게)
    if(const FCozyLandform* F=O->Sel())DragOffset=F->Center-R1(Pos);
    if(O->Selected!=INDEX_NONE||O->Route!=INDEX_NONE){Drag=MakeUnique<FScopedTransaction>(T(TEXT("지도에서 지형 조절")));P->Modify();O->Refresh();return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this));}
    O->Refresh();return FReply::Handled().SetUserFocus(SharedThis(this));
}
FReply SMap::OnMouseMove(const FGeometry& G,const FPointerEvent& E)
{
    const auto O=Panel.Pin();auto* P=O?O->GetPlan():nullptr;
    // 자유 모양 그리는 중: 마우스까지 미리보기 선
    if(O&&P&&O->IsDrawing()){HoverPixel=G.AbsoluteToLocal(E.GetScreenSpacePosition());bHover=true;if(DrawPts.Num())Invalidate(EInvalidateWidgetReason::Paint);return FReply::Handled();}
    if(!Drag||!P||!HasMouseCapture())return FReply::Unhandled();
    FVector2D Pos=FromMap(G.AbsoluteToLocal(E.GetScreenSpacePosition()),G.GetLocalSize(),P->Recipe.MapSize);
    // 끌어서 바꾸는 위치·크기는 0.1m 단위 (31.103222 같은 값이 나오지 않게)
    Pos=R1(Pos);const FVector2D Half=P->Recipe.MapSize*.5;
    Pos.X=FMath::Clamp(Pos.X,-Half.X,Half.X);Pos.Y=FMath::Clamp(Pos.Y,-Half.Y,Half.Y);
    if(bCreating&&P->Recipe.Landforms.IsValidIndex(O->Selected)){auto& F=P->Recipe.Landforms[O->Selected];F.Center=(CreateStart+Pos)*.5;F.Size=FVector2D(FMath::Max(2.,FMath::Abs(Pos.X-CreateStart.X)),FMath::Max(2.,FMath::Abs(Pos.Y-CreateStart.Y)));O->SnapSelected(*P);}
    else if(P->Recipe.Landforms.IsValidIndex(O->Selected))
    {
        auto& F=P->Recipe.Landforms[O->Selected];
        // 자유 모양(또는 고른 띠) 점 끌기 (영지 칸 맞춤은 놓을 때 한 번)
        if(bPointDrag&&IsFree(F)&&O->EditPoints(F).IsValidIndex(O->Point)){O->EditPoints(F)[O->Point].Position=R1(ToLocal(F,Pos));FitFreeSize(F);}
        else
        {
            if(bResize&&!IsFree(F))F.Size=FVector2D(FMath::Clamp(FMath::Abs(Pos.X-F.Center.X)*2,2.,2000.),FMath::Clamp(FMath::Abs(Pos.Y-F.Center.Y)*2,2.,2000.));
            else{const FVector2D C=R1(Pos+DragOffset);F.Center=FVector2D(FMath::Clamp(C.X,-Half.X,Half.X),FMath::Clamp(C.Y,-Half.Y,Half.Y));}
            O->SnapSelected(*P);
        }
    }
    else if(P->Recipe.Routes.IsValidIndex(O->Route)&&P->Recipe.Routes[O->Route].Points.IsValidIndex(O->Node)){auto& V=P->Recipe.Routes[O->Route].Points[O->Node];V.X=Pos.X;V.Y=Pos.Y;}
    P->bHasUnappliedChanges=true;P->bShowPreview=true;P->RebuildPreview();Invalidate(EInvalidateWidgetReason::Paint);O->ScheduleLive();return FReply::Handled();
}
FReply SMap::OnMouseButtonUp(const FGeometry& G,const FPointerEvent& E)
{
    if(E.GetEffectingButton()!=EKeys::LeftMouseButton)return FReply::Unhandled();
    if(auto O=Panel.Pin())if(auto* P=O->GetPlan())
    {
        if(bCreating&&P->Recipe.Landforms.IsValidIndex(O->Selected))
        {
            // 거의 끌지 않았으면 기본 크기 (평지는 영지 안에 들어가는 12×10m)
            auto& F=P->Recipe.Landforms[O->Selected];
            if(FVector2D::Distance(G.AbsoluteToLocal(E.GetScreenSpacePosition()),CreateStartPixel)<6){F.Center=CreateStart;F.Size=F.Type==ECozyLandform::Plateau?FVector2D(12,10):FVector2D(90,70);}
            O->SnapSelected(*P);O->Mode=0;
            O->Status=FString::Printf(TEXT("'%s'을 만들었습니다 (%.0f×%.0fm · %.1fm). 선택 도구로 돌아갔습니다."),*F.Name,F.Size.X,F.Size.Y,F.Height);
        }
        if(bPointDrag&&Drag)O->SnapSelected(*P);
        // 끌기를 놓으면 바로 반영을 곧바로 한 번
        if(Drag&&O->LiveDue>0)O->LiveDue=FPlatformTime::Seconds();
        P->MarkPackageDirty();
    }
    bCreating=bPointDrag=false;Drag.Reset();
    if(auto O=Panel.Pin())O->Refresh();
    return FReply::Handled().ReleaseMouseCapture();
}
FReply SMap::OnMouseButtonDoubleClick(const FGeometry& G,const FPointerEvent& E)
{
    const auto O=Panel.Pin();auto* P=O?O->GetPlan():nullptr;if(!P||E.GetEffectingButton()!=EKeys::LeftMouseButton||GEditor->PlayWorld)return FReply::Unhandled();
    const FVector2D Size=G.GetLocalSize(),Mouse=G.AbsoluteToLocal(E.GetScreenSpacePosition()),Pos=FromMap(Mouse,Size,P->Recipe.MapSize);
    // 그리는 중 더블클릭 = 닫기 (첫 번째 클릭에서 이미 점이 찍힘)
    if(O->IsDrawing()){if(DrawPts.Num()>=3){FinishDraw(*O,*P);return FReply::Handled().SetUserFocus(SharedThis(this));}return OnMouseButtonDown(G,E);}
    // 고른 자유 모양(또는 고른 띠)의 선 더블클릭 = 그 자리에 점 끼워 넣기
    if(O->Mode==0)if(const FCozyLandform* F=O->Sel())if(IsFree(*F)&&O->EditPoints(*F).Num()<128&&HitPoint(Size,*P,*F,O->EditPoints(*F),Mouse)==INDEX_NONE)
    {
        const bool bBand=O->BandOf(*F)!=INDEX_NONE;
        const FCozyPolyShape S=LinePoly(*F,O->BandOf(*F));const FVector2D Local=ToLocal(*F,Pos),Loc=R1(Local);
        const int Span=S.IsValid()?CozyTerrainShape::FindSpan(S,Local,float(8*PixelMeters(Size,P->Recipe.MapSize))):INDEX_NONE;
        if(Span!=INDEX_NONE)
        {
            O->Mutate([O,Span,Loc](ACozyTerrainLayout& A)
            {
                if(!A.Recipe.Landforms.IsValidIndex(O->Selected))return;
                auto& L=A.Recipe.Landforms[O->Selected];auto& Pts=O->EditPoints(L);FCozyShapePoint New;New.Position=Loc;
                const int At=FMath::Min(Span+1,Pts.Num());Pts.Insert(New,At);FitFreeSize(L);O->Point=At;
            });
            O->Message(bBand?TEXT("띠에 점을 끼워 넣었습니다 · 끌어 옮기기 · 오른쪽 클릭 뾰족/둥글게 · Delete 지우기"):TEXT("점을 끼워 넣었습니다 · 끌어 옮기기 · 오른쪽 클릭 뾰족/둥글게 · Delete 지우기"));
            return FReply::Handled().SetUserFocus(SharedThis(this));
        }
    }
    return OnMouseButtonDown(G,E);
}
void SMap::OnMouseCaptureLost(const FCaptureLostEvent&){bCreating=bPointDrag=false;Drag.Reset();}
FReply SMap::OnKeyDown(const FGeometry&,const FKeyEvent& E)
{
    const auto O=Panel.Pin();auto* P=O?O->GetPlan():nullptr;if(!P)return FReply::Unhandled();
    if(O->IsDrawing())
    {
        // 자유 모양 그리는 중: Enter 닫기 · Backspace 마지막 점 지우기 · Esc 취소
        if(E.GetKey()==EKeys::Enter){FinishDraw(*O,*P);return FReply::Handled();}
        if(E.GetKey()==EKeys::BackSpace||E.GetKey()==EKeys::Delete){if(DrawPts.Num())DrawPts.RemoveAt(DrawPts.Num()-1);O->Status=FString::Printf(TEXT("점 %d개 · 마지막 점을 지웠습니다"),DrawPts.Num());Invalidate(EInvalidateWidgetReason::Paint);return FReply::Handled();}
        if(E.GetKey()==EKeys::Escape){const bool bBand=O->Mode==5;DrawPts.Reset();bHover=false;O->Mode=0;O->Message(bBand?TEXT("띠 그리기를 취소했습니다 · 선택 도구"):TEXT("자유 모양 그리기를 취소했습니다 · 선택 도구"));return FReply::Handled();}
    }
    if(E.GetKey()==EKeys::Delete||E.GetKey()==EKeys::BackSpace){O->DeleteSelected();return FReply::Handled();}
    // 띠를 골랐으면 Esc는 띠만 놓고 산은 고른 채로
    if(E.GetKey()==EKeys::Escape&&O->SelBand()){O->Mode=0;O->Band=O->Point=INDEX_NONE;O->Message(TEXT("띠 선택 해제 · 산(바깥 모양)을 고른 상태"));return FReply::Handled();}
    if(E.GetKey()==EKeys::Escape){O->Mode=0;O->SelectNone();O->Message(TEXT("선택 도구 · 선택 해제"));return FReply::Handled();}
    return FReply::Unhandled();
}
}

class FCozyTerrainEditorModule : public IModuleInterface
{
    TUniquePtr<FAutoConsoleCommand> OpenCommand;
public:
    virtual void StartupModule() override
    {
        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(CozyTerrainUI::TabName,FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&){return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNew(CozyTerrainUI::SPanel)];})).SetDisplayName(CozyTerrainUI::T(TEXT("지형 편집기"))).SetTooltipText(CozyTerrainUI::T(TEXT("현재 레벨에서 지형을 설계·미리보기·적용합니다")));
        // 창 메뉴 항목은 위 탭 등록이 자동으로 만든다 (따로 메뉴를 더하면 '지형 편집기'가 두 번 나왔음)
        OpenCommand=MakeUnique<FAutoConsoleCommand>(TEXT("Cozy.TerrainEditor"),TEXT("현재 레벨의 지형 편집기 열기"),FConsoleCommandDelegate::CreateLambda([](){FGlobalTabmanager::Get()->TryInvokeTab(CozyTerrainUI::TabName);}));
    }
    virtual void ShutdownModule() override
    {
        OpenCommand.Reset();FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(CozyTerrainUI::TabName);
    }
};
IMPLEMENT_MODULE(FCozyTerrainEditorModule,CozyTerrainEditor)
