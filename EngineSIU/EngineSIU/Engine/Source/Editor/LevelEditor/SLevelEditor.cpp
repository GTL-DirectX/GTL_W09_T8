#include "SLevelEditor.h"

#include <fstream>
#include "EngineLoop.h"
#include "UnrealClient.h"
#include "WindowsCursor.h"
#include "BaseGizmos/GizmoBaseComponent.h"
#include "Engine/EditorEngine.h"
#include "ImGUI/imgui.h"
#include "Slate/Widgets/Layout/SSplitter.h"
#include "SlateCore/Widgets/SWindow.h"
#include "UnrealEd/EditorConfigManager.h"
#include "UnrealEd/EditorViewportClient.h"
#include "UnrealEd/SkeletalMeshViewportClient.h"
#include "UObject/ObjectTypes.h"

#include "GameFramework/PlayerController.h"


extern FEngineLoop GEngineLoop;


SLevelEditor::SLevelEditor()
    : HSplitter(nullptr)
    , VSplitter(nullptr)
    , bMultiViewportMode(false)
{
}

void SLevelEditor::Initialize(uint32 InEditorWidth, uint32 InEditorHeight, UEngine* InEngine)
{
    ResizeEditor(InEditorWidth, InEditorHeight);
    
    VSplitter = new SSplitterV();
    VSplitter->Initialize(FRect(0.0f, 0.f, InEditorWidth, InEditorHeight));
    
    HSplitter = new SSplitterH();
    HSplitter->Initialize(FRect(0.f, 0.0f, InEditorWidth, InEditorHeight));
    
    FRect Top = VSplitter->SideLT->GetRect();
    FRect Bottom = VSplitter->SideRB->GetRect();
    FRect Left = HSplitter->SideLT->GetRect();
    FRect Right = HSplitter->SideRB->GetRect();

    for (size_t i = 0; i < 4; i++)
    {
        EViewScreenLocation Location = static_cast<EViewScreenLocation>(i);
        FRect Rect;
        switch (Location)
        {
        case EViewScreenLocation::EVL_TopLeft:
            Rect.TopLeftX = Left.TopLeftX;
            Rect.TopLeftY = Top.TopLeftY;
            Rect.Width = Left.Width;
            Rect.Height = Top.Height;
            break;
        case EViewScreenLocation::EVL_TopRight:
            Rect.TopLeftX = Right.TopLeftX;
            Rect.TopLeftY = Top.TopLeftY;
            Rect.Width = Right.Width;
            Rect.Height = Top.Height;
            break;
        case EViewScreenLocation::EVL_BottomLeft:
            Rect.TopLeftX = Left.TopLeftX;
            Rect.TopLeftY = Bottom.TopLeftY;
            Rect.Width = Left.Width;
            Rect.Height = Bottom.Height;
            break;
        case EViewScreenLocation::EVL_BottomRight:
            Rect.TopLeftX = Right.TopLeftX;
            Rect.TopLeftY = Bottom.TopLeftY;
            Rect.Width = Right.Width;
            Rect.Height = Bottom.Height;
            break;
        default:
            return;
        }
        ViewportClients[i] = std::make_shared<FEditorViewportClient>();
        ViewportClients[i]->Initialize(Location, Rect, InEngine->ActiveWorld);
    }

    if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(InEngine))
    {
        EditorEngine->OnStartPIE.AddDynamic(this, &SLevelEditor::OnWorldChanged);
        EditorEngine->OnEndPIE.AddDynamic(this, &SLevelEditor::OnWorldChanged);
    }
    
    ActiveViewportClient = ViewportClients[0];
    
    LoadConfig();

    FSlateAppMessageHandler* Handler = GEngineLoop.GetAppMessageHandler();

    Handler->OnMouseDownDelegate.AddLambda([this](const FPointerEvent& InMouseEvent)
    {
        if (GEngine->ActiveWorld->WorldType == EWorldType::PIE)
        {
            if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
            {
                FWindowsCursor::SetShowMouseCursor(false);
                MousePinPosition = InMouseEvent.GetScreenSpacePosition();
            }
        }

        if (GEngine->ActiveWorld->WorldType != EWorldType::Editor)
        {
            return;
        }
        
        if (ImGui::GetIO().WantCaptureMouse) return;

        switch (InMouseEvent.GetEffectingButton())  // NOLINT(clang-diagnostic-switch-enum)
        {
        case EKeys::LeftMouseButton:
        {
            if (const UEditorEngine* EdEngine = Cast<UEditorEngine>(GEngine))
            {
                USceneComponent* TargetComponent = nullptr;
                if (USceneComponent* SelectedComponent = EdEngine->GetSelectedComponent())
                {
                    TargetComponent = SelectedComponent;
                }
                else if (AActor* SelectedActor = EdEngine->GetSelectedActor())
                {
                    TargetComponent = SelectedActor->GetRootComponent();
                }
                else
                {
                    return;
                }

                // 초기 Actor와 Cursor의 거리차를 저장
                const FViewportCamera* ViewTransform = ActiveViewportClient->GetViewportType() == LVT_Perspective
                                                    ? &ActiveViewportClient->GetPerspectiveCamera()
                                                    : &ActiveViewportClient->GetOrthogonalCamera();

                FVector RayOrigin, RayDir;
                ActiveViewportClient->DeprojectFVector2D(FWindowsCursor::GetClientPosition(), RayOrigin, RayDir);

                const FVector TargetLocation = TargetComponent->GetWorldLocation();
                const float TargetDist = FVector::Distance(ViewTransform->GetLocation(), TargetLocation);
                const FVector TargetRayEnd = RayOrigin + RayDir * TargetDist;
                TargetDiff = TargetLocation - TargetRayEnd;     
            }
            break;
        }
        case EKeys::RightMouseButton:
        {
            if (!InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
            {
                FWindowsCursor::SetShowMouseCursor(false);
                MousePinPosition = InMouseEvent.GetScreenSpacePosition();
            }
            break;
        }
        default:
            break;
        }

        // 마우스 이벤트가 일어난 위치의 뷰포트를 선택
        if (bMultiViewportMode)
        {
            POINT Point;
            GetCursorPos(&Point);
            ScreenToClient(GEngineLoop.AppWnd, &Point);
            FVector2D ClientPos = FVector2D{static_cast<float>(Point.x), static_cast<float>(Point.y)};
            SelectViewport(ClientPos);
            VSplitter->OnPressed({ClientPos.X, ClientPos.Y});
            HSplitter->OnPressed({ClientPos.X, ClientPos.Y});
        }
    });

    Handler->OnMouseMoveDelegate.AddLambda([this](const FPointerEvent& InMouseEvent)
    {
        if (ImGui::GetIO().WantCaptureMouse) return;

        // Splitter 움직임 로직
        if (InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
        {
            const auto& [DeltaX, DeltaY] = InMouseEvent.GetCursorDelta();
            
            bool bSplitterDragging = false;
            if (VSplitter->IsSplitterPressed())
            {
                VSplitter->OnDrag(FPoint(DeltaX, DeltaY));
                bSplitterDragging = true;
            }
            if (HSplitter->IsSplitterPressed())
            {
                HSplitter->OnDrag(FPoint(DeltaX, DeltaY));
                bSplitterDragging = true;
            }

            if (bSplitterDragging)
            {
                ResizeViewports();
            }
        }

        // 멀티 뷰포트일 때, 커서 변경 로직
        if (
            bMultiViewportMode
            && !InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton)
            && !InMouseEvent.IsMouseButtonDown(EKeys::RightMouseButton)
        ) {
            // TODO: 나중에 커서가 Viewport 위에 있을때만 ECursorType::Crosshair로 바꾸게끔 하기
            // ECursorType CursorType = ECursorType::Crosshair;
            ECursorType CursorType = ECursorType::Arrow;
            POINT Point;

            GetCursorPos(&Point);
            ScreenToClient(GEngineLoop.AppWnd, &Point);
            FVector2D ClientPos = FVector2D{static_cast<float>(Point.x), static_cast<float>(Point.y)};
            const bool bIsVerticalHovered = VSplitter->IsSplitterHovered({ClientPos.X, ClientPos.Y});
            const bool bIsHorizontalHovered = HSplitter->IsSplitterHovered({ClientPos.X, ClientPos.Y});

            if (bIsHorizontalHovered && bIsVerticalHovered)
            {
                CursorType = ECursorType::ResizeAll;
            }
            else if (bIsHorizontalHovered)
            {
                CursorType = ECursorType::ResizeLeftRight;
            }
            else if (bIsVerticalHovered)
            {
                CursorType = ECursorType::ResizeUpDown;
            }
            FWindowsCursor::SetMouseCursor(CursorType);
        }
    });

    Handler->OnMouseUpDelegate.AddLambda([this](const FPointerEvent& InMouseEvent)
    {
        switch (InMouseEvent.GetEffectingButton())  // NOLINT(clang-diagnostic-switch-enum)
        {
        case EKeys::RightMouseButton:
        {
            FWindowsCursor::SetShowMouseCursor(true);
            FWindowsCursor::SetPosition(
                static_cast<int32>(MousePinPosition.X),
                static_cast<int32>(MousePinPosition.Y)
            );
            return;
        }

        // Viewport 선택 로직
        case EKeys::LeftMouseButton:
        {
            VSplitter->OnReleased();
            HSplitter->OnReleased();
            return;
        }

        default:
            return;
        }
    });

    Handler->OnRawMouseInputDelegate.AddLambda([this](const FPointerEvent& InMouseEvent)
    {

        if (GEngine->ActiveWorld->WorldType == EWorldType::PIE)
        {
            ActiveViewportClient->GetWorld()->GetFirstPlayerController()->MouseInput(InMouseEvent.GetCursorDelta().X, InMouseEvent.GetCursorDelta().Y);
        }

        if (GEngine->ActiveWorld->WorldType != EWorldType::Editor)
        {
            return;
        }
        
        // Mouse Move 이벤트 일때만 실행
        if (
            InMouseEvent.GetInputEvent() == IE_Axis
            && InMouseEvent.GetEffectingButton() == EKeys::Invalid
        )
        {
            // 에디터 카메라 이동 로직
            if (!InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) && InMouseEvent.IsMouseButtonDown(EKeys::RightMouseButton))
            {
                ActiveViewportClient->MouseMove(InMouseEvent);
            }
            else if (!InMouseEvent.IsMouseButtonDown(EKeys::RightMouseButton) && InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
            {
                // Gizmo control
                if (const UEditorEngine* EdEngine = Cast<UEditorEngine>(GEngine))
                {
                    const UGizmoBaseComponent* Gizmo = Cast<UGizmoBaseComponent>(ActiveViewportClient->GetPickedGizmoComponent());
                    if (!Gizmo)
                    {
                        return;
                    }
                    
                    USceneComponent* TargetComponent = EdEngine->GetSelectedComponent();
                    if (!TargetComponent)
                    {
                        if (AActor* SelectedActor = EdEngine->GetSelectedActor())
                        {
                            TargetComponent = SelectedActor->GetRootComponent();
                        }
                        else
                        {
                            return;
                        }
                    }
                    
                    const FViewportCamera* ViewTransform = ActiveViewportClient->GetViewportType() == LVT_Perspective
                                                            ? &ActiveViewportClient->GetPerspectiveCamera()
                                                            : &ActiveViewportClient->GetOrthogonalCamera();
                        
                    FVector RayOrigin, RayDir;
                    ActiveViewportClient->DeprojectFVector2D(FWindowsCursor::GetClientPosition(), RayOrigin, RayDir);

                    const float TargetDist = FVector::Distance(ViewTransform->GetLocation(), TargetComponent->GetWorldLocation());
                    const FVector TargetRayEnd = RayOrigin + RayDir * TargetDist;
                    const FVector Result = TargetRayEnd + TargetDiff;

                    FVector NewLocation = TargetComponent->GetWorldLocation();
                    if (EdEngine->GetEditorPlayer()->GetCoordMode() == CDM_WORLD)
                    {
                        // 월드 좌표계에서 카메라 방향을 고려한 이동
                        if (Gizmo->GetGizmoType() == UGizmoBaseComponent::ArrowX)
                        {
                            // 카메라의 오른쪽 방향을 X축 이동에 사용
                            NewLocation.X = Result.X;
                        }
                        else if (Gizmo->GetGizmoType() == UGizmoBaseComponent::ArrowY)
                        {
                            // 카메라의 오른쪽 방향을 Y축 이동에 사용
                            NewLocation.Y = Result.Y;
                        }
                        else if (Gizmo->GetGizmoType() == UGizmoBaseComponent::ArrowZ)
                        {
                            // 카메라의 위쪽 방향을 Z축 이동에 사용
                            NewLocation.Z = Result.Z;
                        }
                    }
                    else
                    {
                        // Result에서 현재 액터 위치를 빼서 이동 벡터를 구함
                        const FVector Delta = Result - TargetComponent->GetWorldLocation();
                        // 각 축에 대해 Local 방향 벡터에 투영하여 이동량 계산
                        if (Gizmo->GetGizmoType() == UGizmoBaseComponent::ArrowX)
                        {
                            const float MoveAmount = FVector::DotProduct(Delta, TargetComponent->GetForwardVector());
                            NewLocation += TargetComponent->GetForwardVector() * MoveAmount;
                        }
                        else if (Gizmo->GetGizmoType() == UGizmoBaseComponent::ArrowY)
                        {
                            const float MoveAmount = FVector::DotProduct(Delta, TargetComponent->GetRightVector());
                            NewLocation += TargetComponent->GetRightVector() * MoveAmount;
                            TargetComponent->SetWorldLocation(NewLocation);
                        }
                        else if (Gizmo->GetGizmoType() == UGizmoBaseComponent::ArrowZ)
                        {
                            const float MoveAmount = FVector::DotProduct(Delta, TargetComponent->GetUpVector());
                            NewLocation += TargetComponent->GetUpVector() * MoveAmount;
                        }
                    }
                    TargetComponent->SetWorldLocation(NewLocation);
                }
            }
        }
        // 마우스 휠 이벤트
        else if (InMouseEvent.GetEffectingButton() == EKeys::MouseWheelAxis)
        {
            // 카메라 속도 조절
            if (InMouseEvent.IsMouseButtonDown(EKeys::RightMouseButton) && ActiveViewportClient->IsPerspective())
            {
                const float CurrentSpeed = ActiveViewportClient->GetCameraSpeedScalar();
                const float Adjustment = FMath::Sign(InMouseEvent.GetWheelDelta()) * FMath::Loge(CurrentSpeed + 1.0f) * 0.5f;

                ActiveViewportClient->SetCameraSpeed(CurrentSpeed + Adjustment);
            }
        }
    });

    Handler->OnMouseWheelDelegate.AddLambda([this](const FPointerEvent& InMouseEvent)
    {
        if (ImGui::GetIO().WantCaptureMouse) return;

        // 뷰포트에서 앞뒤 방향으로 화면 이동
        if (ActiveViewportClient->IsPerspective())
        {
            if (!InMouseEvent.IsMouseButtonDown(EKeys::RightMouseButton))
            {
                const FVector CameraLoc = ActiveViewportClient->GetPerspectiveCamera().GetLocation();
                const FVector CameraForward = ActiveViewportClient->GetPerspectiveCamera().GetForwardVector();
                ActiveViewportClient->GetPerspectiveCamera().SetLocation(
                    CameraLoc + CameraForward * InMouseEvent.GetWheelDelta() * 50.0f
                );
            }
        }
        else
        {
            FEditorViewportClient::SetOrthoSize(FEditorViewportClient::GetOrthoSize() + (-InMouseEvent.GetWheelDelta()));
        }
    });

    Handler->OnKeyDownDelegate.AddLambda([this](const FKeyEvent& InKeyEvent)
    {   
        ActiveViewportClient->InputKey(InKeyEvent);
    });

    Handler->OnKeyUpDelegate.AddLambda([this](const FKeyEvent& InKeyEvent)
    {
        ActiveViewportClient->InputKey(InKeyEvent);
    });
}

void SLevelEditor::Tick(float DeltaTime)
{
    for (std::shared_ptr<FEditorViewportClient> Viewport : ViewportClients)
    {
        Viewport->Tick(DeltaTime);
    }

    for (auto& [Name, Viewport] : WindowViewportClients)
    {
        Viewport->Tick(DeltaTime);
    }
}

void SLevelEditor::Release()
{
    delete VSplitter;
    delete HSplitter;
}

void SLevelEditor::ResizeEditor(uint32 InEditorWidth, uint32 InEditorHeight)
{
    if (InEditorWidth == EditorWidth && InEditorHeight == EditorHeight)
    {
        return;
    }
    
    EditorWidth = InEditorWidth;
    EditorHeight = InEditorHeight;

    if (HSplitter && VSplitter)
    {
        HSplitter->OnResize(EditorWidth, EditorHeight);
        VSplitter->OnResize(EditorWidth, EditorHeight);
        ResizeViewports();
    }
}

void SLevelEditor::SelectViewport(const FVector2D& Point)
{
    for (int i = 0; i < 4; i++)
    {
        if (ViewportClients[i]->IsSelected(Point))
        {
            SetActiveViewportClient(i);
            return;
        }
    }
}

void SLevelEditor::ResizeViewports()
{
    if (bMultiViewportMode)
    {
        if (GetViewports()[0])
        {
            for (int i = 0; i < 4; ++i)
            {
                GetViewports()[i]->ResizeViewport(
                    VSplitter->SideLT->GetRect(),
                    VSplitter->SideRB->GetRect(),
                    HSplitter->SideLT->GetRect(),
                    HSplitter->SideRB->GetRect()
                );
            }
        }
    }
    else
    {
        ActiveViewportClient->GetViewport()->ResizeViewport(FRect(0.0f, 0.0f, EditorWidth, EditorHeight));
    }

    // NOTE : Window로 띄우는 ImGUI 창의 경우 메인 Window와 별개임
}

void SLevelEditor::SetEnableMultiViewport(bool bIsEnable)
{
    bMultiViewportMode = bIsEnable;
    ResizeViewports();
}

bool SLevelEditor::IsMultiViewport() const
{
    return bMultiViewportMode;
}

FViewportClient* SLevelEditor::AddWindowViewportClient(FName ViewportName, UWorld* PreviewWorld, const FRect& InRect, EEditorViewportType NewViewportType)
{
    if (WindowViewportClients.Contains(ViewportName))
    {
        UE_LOG(LogLevel::Warning, TEXT("Viewport with name %s already exists."), *ViewportName.ToString());
        return WindowViewportClients[ViewportName].get();
    }

    std::shared_ptr<FViewportClient> NewViewportClient = nullptr;
    switch (NewViewportType)
    {
    case EEditorViewportType::EditorViewport:
        NewViewportClient = std::make_shared<FEditorViewportClient>();
        NewViewportClient->Initialize(EViewScreenLocation::EVL_Window, InRect, PreviewWorld);
        NewViewportClient->SetViewportType(ELevelViewportType::LVT_Perspective);
        break;
    case EEditorViewportType::StaticMeshEditor:
        break;
    case EEditorViewportType::SkeletalMeshEditor:
        NewViewportClient = std::make_shared<FSkeletalMeshViewportClient>();
        NewViewportClient->Initialize(EViewScreenLocation::EVL_Window, InRect, PreviewWorld);
        NewViewportClient->SetViewportType(ELevelViewportType::LVT_Perspective);
        break;
    case EEditorViewportType::AnimationEditor:
        break;
    case EEditorViewportType::MaterialEditor:
        break;
    case EEditorViewportType::TextureEditor:
        break;
    case EEditorViewportType::ParticleEditor:
        break;
    case EEditorViewportType::MAX:
        break;
    default:
        break;
    }

    WindowViewportClients.Add(ViewportName, NewViewportClient);

    return NewViewportClient.get();
}

void SLevelEditor::RemoveWindowViewportClient(FName ViewportName)
{
    if (WindowViewportClients.Contains(ViewportName))
    {
        WindowViewportClients.Remove(ViewportName);
    }
    else
    {
        UE_LOG(LogLevel::Warning, TEXT("Viewport with name %s does not exist."), *ViewportName.ToString());
    }
}

void SLevelEditor::OnWorldChanged(UWorld* NewWorld)
{
    // 모든 뷰포트 클라이언트들의 참조 월드를 변경합니다
    for (size_t i = 0; i < 4; i++)
    {
        ViewportClients[i]->SetWorld(NewWorld);
    }
}

void SLevelEditor::LoadConfig()
{
    auto Config = FEditorConfigManager::GetInstance().Read();
    FVector Pivot;
    float OrthoSize;
    Pivot.X = FEditorConfigManager::GetValueFromConfig(Config, "OrthoPivotX", 0.0f);
    Pivot.Y = FEditorConfigManager::GetValueFromConfig(Config, "OrthoPivotY", 0.0f);
    Pivot.Z = FEditorConfigManager::GetValueFromConfig(Config, "OrthoPivotZ", 0.0f);
    OrthoSize = FEditorConfigManager::GetValueFromConfig(Config, "OrthoZoomSize", 10.0f);

    FEditorViewportClient::SetOrthoPivot(Pivot);
    FEditorViewportClient::SetOrthoSize(OrthoSize);

    SetActiveViewportClient(FEditorConfigManager::GetValueFromConfig(Config, "ActiveViewportIndex", 0));
    bMultiViewportMode = FEditorConfigManager::GetValueFromConfig(Config, "bMultiView", false);
    if (bMultiViewportMode)
    {
        SetEnableMultiViewport(true);
    }
    else
    {
        SetEnableMultiViewport(false);
    }
    
    for (size_t i = 0; i < 4; i++)
    {
        ViewportClients[i]->LoadConfig(Config);
    }
    
    if (HSplitter)
    {
        HSplitter->LoadConfig(Config);
    }
    if (VSplitter)
    {
        VSplitter->LoadConfig(Config);
    }
    
    ResizeViewports();
}

void SLevelEditor::SaveConfig()
{
    TMap<FString, FString> config;
    if (HSplitter)
    {
        HSplitter->SaveConfig(config);
    }
    if (VSplitter)
    {
        VSplitter->SaveConfig(config);
    }
    for (size_t i = 0; i < 4; i++)
    {
        ViewportClients[i]->SaveConfig(config);
    }
    ActiveViewportClient->SaveConfig(config);
    config["bMultiView"] = std::to_string(bMultiViewportMode);
    config["ActiveViewportIndex"] = std::to_string(ActiveViewportClient->GetViewportIndex());
    config["ScreenWidth"] = std::to_string(EditorWidth);
    config["ScreenHeight"] = std::to_string(EditorHeight);
    config["OrthoPivotX"] = std::to_string(FEditorViewportClient::GetOrthoPivot().X);
    config["OrthoPivotY"] = std::to_string(FEditorViewportClient::GetOrthoPivot().Y);
    config["OrthoPivotZ"] = std::to_string(FEditorViewportClient::GetOrthoPivot().Z);
    config["OrthoZoomSize"] = std::to_string(FEditorViewportClient::GetOrthoSize());
    FEditorConfigManager::GetInstance().AddConfig(config);
}
