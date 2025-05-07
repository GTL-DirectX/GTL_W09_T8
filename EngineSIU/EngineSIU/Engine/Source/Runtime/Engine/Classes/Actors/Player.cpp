#include "Player.h"

#include "ShowFlag.h"
#include "BaseGizmos/GizmoBaseComponent.h"
#include "BaseGizmos/TransformGizmo.h"
#include "Components/StaticMeshComponent.h"
#include "Components/Light/LightComponent.h"
#include "Engine/EditorEngine.h"
#include "LevelEditor/SLevelEditor.h"
#include "Math/Quat.h"
#include "UnrealEd/EditorViewportClient.h"
#include "UObject/Casts.h"
#include "UObject/Object.h"
#include "UObject/UObjectIterator.h"

#include "Viewport.h"

#include "ImGUI/imgui.h"


void UEditorPlayer::Initialize()
{
    FSlateAppMessageHandler* Handler = GEngineLoop.GetAppMessageHandler();
    Handler->OnMouseDownDelegate.AddLambda([this](const FPointerEvent& InMouseEvent)
    {
        if (GEngine->ActiveWorld->WorldType != EWorldType::Editor)
        {
            return;
        }
        
        if (ImGui::GetIO().WantCaptureMouse) return;

        if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
        {
            return;
        }
        
        POINT mousePos;
        GetCursorPos(&mousePos);
        GetCursorPos(&m_LastMousePos);
        ScreenToClient(GEngineLoop.AppWnd, &mousePos);

        FVector pickPosition;

        std::shared_ptr<FEditorViewportClient> ActiveViewport = GEngineLoop.GetLevelEditor()->GetActiveViewportClient();
        ScreenToViewSpace(mousePos.x, mousePos.y, ActiveViewport, pickPosition);
        bool Result = PickGizmo(pickPosition, ActiveViewport.get());
        if (Result == false)
        {
            PickActor(pickPosition, ActiveViewport);
        }
    });

    Handler->OnMouseMoveDelegate.AddLambda([this](const FPointerEvent& InMouseEvent)
    {
        if (GEngine->ActiveWorld->WorldType != EWorldType::Editor)
        {
            return;
        }
        
        if (ImGui::GetIO().WantCaptureMouse)
        {
            return;
        }
        
        PickedObjControl();
    });

    Handler->OnMouseUpDelegate.AddLambda([this](const FPointerEvent& InMouseEvent)
    {
        if (GEngine->ActiveWorld->WorldType != EWorldType::Editor)
        {
            return;
        }
        
        if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
        {
            return;
        }
        
        std::shared_ptr<FEditorViewportClient> ActiveViewport = GEngineLoop.GetLevelEditor()->GetActiveViewportClient();
        ActiveViewport->SetPickedGizmoComponent(nullptr);
    });
}

void UEditorPlayer::ProcessGizmoIntersection(USceneComponent* Component, const FVector& PickPosition, FEditorViewportClient* InActiveViewport, bool& bIsPickedGizmo)
{
    int maxIntersect = 0;
    float minDistance = FLT_MAX;
    float Distance = 0.0f;
    int currentIntersectCount = 0;
    if (!Component) return;
    if (RayIntersectsObject(PickPosition, Component, Distance, currentIntersectCount))
    {
        if (Distance < minDistance)
        {
            minDistance = Distance;
            maxIntersect = currentIntersectCount;
            //GetWorld()->SetPickingGizmo(iter);
            InActiveViewport->SetPickedGizmoComponent(Component);
            bIsPickedGizmo = true;
        }
        else if (abs(Distance - minDistance) < FLT_EPSILON && currentIntersectCount > maxIntersect)
        {
            maxIntersect = currentIntersectCount;
            //GetWorld()->SetPickingGizmo(iter);
            InActiveViewport->SetPickedGizmoComponent(Component);
            bIsPickedGizmo = true;
        }
    }
}

bool UEditorPlayer::PickGizmo(FVector pickPosition, FEditorViewportClient* InActiveViewport)
{
    bool isPickedGizmo = false;
    UEditorEngine* Engine = Cast<UEditorEngine>(GEngine);
    if (Engine->GetSelectedActor())
    {
        if (ControlMode == CM_TRANSLATION)
        {
            for (UStaticMeshComponent* iter : InActiveViewport->GetGizmoActor()->GetArrowArr())
            {
                ProcessGizmoIntersection(iter, pickPosition, InActiveViewport, isPickedGizmo);
            }
        }
        else if (ControlMode == CM_ROTATION)
        {
            for (UStaticMeshComponent* iter : InActiveViewport->GetGizmoActor()->GetDiscArr())
            {
                ProcessGizmoIntersection(iter, pickPosition, InActiveViewport, isPickedGizmo);
            }
        }
        else if (ControlMode == CM_SCALE)
        {
            for (UStaticMeshComponent* iter : InActiveViewport->GetGizmoActor()->GetScaleArr())
            {
                ProcessGizmoIntersection(iter, pickPosition, InActiveViewport, isPickedGizmo);
            }
        }
    }
    return isPickedGizmo;
}

void UEditorPlayer::PickActor(const FVector& pickPosition, std::shared_ptr<FEditorViewportClient> ActiveViewport)
{
    if (!(ActiveViewport->GetShowFlag() & EEngineShowFlags::SF_Primitives)) return;

    USceneComponent* Possible = nullptr;
    int maxIntersect = 0;
    float minDistance = FLT_MAX;
    for (const auto iter : TObjectRange<UPrimitiveComponent>())
    {
        UPrimitiveComponent* PrimitiveComponent;
        if (iter->IsA<UPrimitiveComponent>())
        {
            PrimitiveComponent = static_cast<UPrimitiveComponent*>(iter);
        }
        else
        {
            continue;
        }

        if (PrimitiveComponent == nullptr || PrimitiveComponent->IsA<UGizmoBaseComponent>())
        {
            continue;
        }
        
        float Distance = 0.0f;
        int currentIntersectCount = 0;
        if (RayIntersectsObject(pickPosition, PrimitiveComponent, Distance, currentIntersectCount))
        {
            if (Distance < minDistance)
            {
                minDistance = Distance;
                maxIntersect = currentIntersectCount;
                Possible = PrimitiveComponent;
            }
            else if (abs(Distance - minDistance) < FLT_EPSILON && currentIntersectCount > maxIntersect)
            {
                maxIntersect = currentIntersectCount;
                Possible = PrimitiveComponent;
            }
        }
    }
    if (Possible)
    {
        UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine);
        EditorEngine->SelectActor(Possible->GetOwner());
        EditorEngine->SelectComponent(Possible);
    }
    else
    {
        UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine);
        EditorEngine->DeselectActor(EditorEngine->GetSelectedActor());
        EditorEngine->DeselectComponent(EditorEngine->GetSelectedComponent());
    }
}

void UEditorPlayer::SetControlMode()
{
    ControlMode = static_cast<EControlMode>((ControlMode + 1) % CM_END);
}

void UEditorPlayer::SetCoordiMode()
{
    CoordMode = static_cast<ECoordMode>((CoordMode + 1) % CDM_END);
}

void UEditorPlayer::ScreenToViewSpace(int32 ScreenX, int32 ScreenY, std::shared_ptr<FEditorViewportClient> ActiveViewport, FVector& RayOrigin)
{
    FRect Rect = ActiveViewport->GetViewport()->GetRect();
    
    float ViewportX = static_cast<float>(ScreenX) - Rect.TopLeftX;
    float ViewportY = static_cast<float>(ScreenY) - Rect.TopLeftY;

    FMatrix ProjectionMatrix = ActiveViewport->GetProjectionMatrix();
    
    RayOrigin.X = ((2.0f * ViewportX / Rect.Width) - 1) / ProjectionMatrix[0][0];
    RayOrigin.Y = -((2.0f * ViewportY / Rect.Height) - 1) / ProjectionMatrix[1][1];
    
    if (GEngineLoop.GetLevelEditor()->GetActiveViewportClient()->IsOrthographic())
    {
        RayOrigin.Z = 0.0f;  // 오쏘 모드에서는 unproject 시 near plane 위치를 기준
    }
    else
    {
        RayOrigin.Z = 1.0f;  // 퍼스펙티브 모드: near plane
    }
}

int UEditorPlayer::RayIntersectsObject(const FVector& PickPosition, USceneComponent* Component, float& HitDistance, int& IntersectCount)
{
    FMatrix WorldMatrix = Component->GetWorldMatrix();
	FMatrix ViewMatrix = GEngineLoop.GetLevelEditor()->GetActiveViewportClient()->GetViewMatrix();
    
    bool bIsOrtho = GEngineLoop.GetLevelEditor()->GetActiveViewportClient()->IsOrthographic();
    

    if (bIsOrtho)
    {
        // 오쏘 모드: ScreenToViewSpace()에서 계산된 pickPosition이 클립/뷰 좌표라고 가정
        FMatrix inverseView = FMatrix::Inverse(ViewMatrix);
        // pickPosition을 월드 좌표로 변환
        FVector worldPickPos = inverseView.TransformPosition(PickPosition);  
        // 오쏘에서는 픽킹 원점은 unproject된 픽셀의 위치
        FVector rayOrigin = worldPickPos;
        // 레이 방향은 카메라의 정면 방향 (평행)
        FVector orthoRayDir = GEngineLoop.GetLevelEditor()->GetActiveViewportClient()->GetOrthogonalCamera().GetForwardVector().GetSafeNormal();

        // 객체의 로컬 좌표계로 변환
        FMatrix LocalMatrix = FMatrix::Inverse(WorldMatrix);
        FVector LocalRayOrigin = LocalMatrix.TransformPosition(rayOrigin);
        FVector LocalRayDir = (LocalMatrix.TransformPosition(rayOrigin + orthoRayDir) - LocalRayOrigin).GetSafeNormal();
        
        IntersectCount = Component->CheckRayIntersection(LocalRayOrigin, LocalRayDir, HitDistance);
        return IntersectCount;
    }
    else
    {
        FMatrix inverseMatrix = FMatrix::Inverse(WorldMatrix * ViewMatrix);
        FVector cameraOrigin = { 0,0,0 };
        FVector pickRayOrigin = inverseMatrix.TransformPosition(cameraOrigin);
        // 퍼스펙티브 모드의 기존 로직 사용
        FVector transformedPick = inverseMatrix.TransformPosition(PickPosition);
        FVector rayDirection = (transformedPick - pickRayOrigin).GetSafeNormal();
        
        IntersectCount = Component->CheckRayIntersection(pickRayOrigin, rayDirection, HitDistance);

        if (IntersectCount > 0)
        {
            FVector LocalHitPoint = pickRayOrigin + rayDirection * HitDistance;

            FVector WorldHitPoint = WorldMatrix.TransformPosition(LocalHitPoint);

            FVector WorldRayOrigin;
            FMatrix InverseView = FMatrix::Inverse(ViewMatrix);
            WorldRayOrigin = InverseView.TransformPosition(cameraOrigin);

            float WorldDistance = FVector::Distance(WorldRayOrigin, WorldHitPoint);

            HitDistance = WorldDistance;
        }
        return IntersectCount;
    }
}

void UEditorPlayer::PickedObjControl()
{
    UEditorEngine* Engine = Cast<UEditorEngine>(GEngine);
    FEditorViewportClient* ActiveViewport = GEngineLoop.GetLevelEditor()->GetActiveViewportClient().get();
    if (Engine && Engine->GetSelectedActor() && ActiveViewport->GetPickedGizmoComponent())
    {
        POINT CurrentMousePos;
        GetCursorPos(&CurrentMousePos);
        const float DeltaX = static_cast<float>(CurrentMousePos.x - m_LastMousePos.x);
        const float DeltaY = static_cast<float>(CurrentMousePos.y - m_LastMousePos.y);

        USceneComponent* TargetComponent = Engine->GetSelectedComponent();
        if (!TargetComponent)
        {
            if (AActor* SelectedActor = Engine->GetSelectedActor())
            {
                TargetComponent = SelectedActor->GetRootComponent();
            }
            else
            {
                return;
            }
        }
        
        UGizmoBaseComponent* Gizmo = Cast<UGizmoBaseComponent>(ActiveViewport->GetPickedGizmoComponent());
        switch (ControlMode)
        {
        case CM_TRANSLATION:
            // ControlTranslation(TargetComponent, Gizmo, deltaX, deltaY);
            // SLevelEditor에 있음
            break;
        case CM_SCALE:
            ControlScale(TargetComponent, Gizmo, DeltaX, DeltaY);

            break;
        case CM_ROTATION:
            ControlRotation(TargetComponent, Gizmo, DeltaX, DeltaY);
            break;
        default:
            break;
        }
        m_LastMousePos = CurrentMousePos;
    }
}

void UEditorPlayer::ControlRotation(USceneComponent* Component, UGizmoBaseComponent* Gizmo, float DeltaX, float DeltaY)
{
    const auto ActiveViewport = GEngineLoop.GetLevelEditor()->GetActiveViewportClient();
    const FViewportCamera* ViewTransform = ActiveViewport->GetViewportType() == LVT_Perspective
                                                        ? &ActiveViewport->GetPerspectiveCamera()
                                                        : &ActiveViewport->GetOrthogonalCamera();

    FVector CameraForward = ViewTransform->GetForwardVector();
    FVector CameraRight = ViewTransform->GetRightVector();
    FVector CameraUp = ViewTransform->GetUpVector();

    FQuat CurrentRotation = Component->GetWorldRotation().ToQuaternion();

    FQuat RotationDelta = FQuat();

    if (Gizmo->GetGizmoType() == UGizmoBaseComponent::CircleX)
    {
        float RotationAmount = (CameraUp.Z >= 0 ? -1.0f : 1.0f) * DeltaY * 0.01f;
        RotationAmount = RotationAmount + (CameraRight.X >= 0 ? 1.0f : -1.0f) * DeltaX * 0.01f;

        FVector Axis = FVector::ForwardVector;
        if (CoordMode == CDM_LOCAL)
        {
            Axis = Component->GetForwardVector();
        }

        RotationDelta = FQuat(Axis, RotationAmount);
    }
    else if (Gizmo->GetGizmoType() == UGizmoBaseComponent::CircleY)
    {
        float RotationAmount = (CameraRight.X >= 0 ? 1.0f : -1.0f) * DeltaX * 0.01f;
        RotationAmount = RotationAmount + (CameraUp.Z >= 0 ? 1.0f : -1.0f) * DeltaY * 0.01f;

        FVector Axis = FVector::RightVector;
        if (CoordMode == CDM_LOCAL)
        {
            Axis = Component->GetRightVector();
        }

        RotationDelta = FQuat(Axis, RotationAmount);
    }
    else if (Gizmo->GetGizmoType() == UGizmoBaseComponent::CircleZ)
    {
        float RotationAmount = (CameraForward.X <= 0 ? -1.0f : 1.0f) * DeltaX * 0.01f;

        FVector Axis = FVector::UpVector;
        if (CoordMode == CDM_LOCAL)
        {
            Axis = Component->GetUpVector();
        }

        RotationDelta = FQuat(Axis, RotationAmount);
    }
    
    // 쿼터니언의 곱 순서는 delta * current 가 맞음.
    Component->SetWorldRotation(RotationDelta * CurrentRotation); 
}

void UEditorPlayer::ControlScale(USceneComponent* Component, UGizmoBaseComponent* Gizmo, float DeltaX, float DeltaY)
{
    const auto ActiveViewport = GEngineLoop.GetLevelEditor()->GetActiveViewportClient();
    const FViewportCamera* ViewTransform = ActiveViewport->GetViewportType() == LVT_Perspective
                                                        ? &ActiveViewport->GetPerspectiveCamera()
                                                        : &ActiveViewport->GetOrthogonalCamera();
    FVector CameraRight = ViewTransform->GetRightVector();
    FVector CameraUp = ViewTransform->GetUpVector();
    
    // 월드 좌표계에서 카메라 방향을 고려한 이동
    if (Gizmo->GetGizmoType() == UGizmoBaseComponent::ScaleX)
    {
        // 카메라의 오른쪽 방향을 X축 이동에 사용
        FVector moveDir = CameraRight * DeltaX * 0.05f;
        Component->AddScale(FVector(moveDir.X, 0.0f, 0.0f));
    }
    else if (Gizmo->GetGizmoType() == UGizmoBaseComponent::ScaleY)
    {
        // 카메라의 오른쪽 방향을 Y축 이동에 사용
        FVector moveDir = CameraRight * DeltaX * 0.05f;
        Component->AddScale(FVector(0.0f, moveDir.Y, 0.0f));
    }
    else if (Gizmo->GetGizmoType() == UGizmoBaseComponent::ScaleZ)
    {
        // 카메라의 위쪽 방향을 Z축 이동에 사용
        FVector moveDir = CameraUp * -DeltaY * 0.05f;
        Component->AddScale(FVector(0.0f, 0.0f, moveDir.Z));
    }
}
