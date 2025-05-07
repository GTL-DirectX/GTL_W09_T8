#include "ViewportClient.h"

#include "Viewport.h"
#include "Math/JungleMath.h"
#include "Engine/EditorEngine.h"
#include "World/World.h"
#include "Camera/CameraComponent.h"
#include "ShowFlag.h"


FVector FViewportClient::OrthoPivot = FVector(0.0f, 0.0f, 0.0f);
float FViewportClient::OrthoSize = 10.0f;

FViewportClient::FViewportClient()
    : ViewportType(LVT_Perspective)
   , ShowFlag(1087) // FIX-ME
   , ViewMode(EViewModeIndex::VMI_Lit_BlinnPhong)
{
}

FViewportClient::~FViewportClient()
{
    FViewportClient::Release();
    ViewportResourceCache = nullptr;
}

void FViewportClient::Draw(FViewport* Viewport)
{
}

void FViewportClient::Tick(float DeltaTime)
{
    if (Viewport)
    {
        Viewport->Draw(DeltaTime);
    }
}

void FViewportClient::Release()
{
    delete Viewport;
}

void FViewportClient::Initialize(EViewScreenLocation InViewportIndex, const FRect& InRect, UWorld* InWorld)
{
}


D3D11_VIEWPORT& FViewportClient::GetD3DViewport() const
{
    return Viewport->GetD3DViewport();
}

FViewportResource* FViewportClient::GetViewportResource()
{
    if (!ViewportResourceCache)
    {
        ViewportResourceCache = Viewport->GetViewportResource();
    }
    return ViewportResourceCache;
}

ELevelViewportType FViewportClient::GetViewportType() const
{
    ELevelViewportType EffectiveViewportType = ViewportType;
    if (EffectiveViewportType == LVT_None)
    {
        EffectiveViewportType = LVT_Perspective;
    }
    return EffectiveViewportType;
}

void FViewportClient::SetViewportType(ELevelViewportType InViewportType)
{
    ViewportType = InViewportType;
}

void FViewportClient::SetShowFlagState(uint64 flag, bool bEnable)
{
    if (bEnable) {
        ShowFlag |= static_cast<uint64>(flag);
    }
    else {
        ShowFlag &= ~static_cast<uint64>(flag);
    }
}

bool FViewportClient::IsOrthographic() const
{
    return !IsPerspective();
}

bool FViewportClient::IsPerspective() const
{
    return (GetViewportType() == LVT_Perspective);
}

void FViewportClient::UpdateViewMatrix()
{
    if (IsPerspective())
    {
        View = JungleMath::CreateViewMatrix(PerspectiveCamera.GetLocation(),
        PerspectiveCamera.GetLocation() + PerspectiveCamera.GetForwardVector(),
        FVector{ 0.0f,0.0f, 1.0f }
        );
    }
    else 
    {
        UpdateOrthoCameraLoc();
        if (ViewportType == LVT_OrthoXY || ViewportType == LVT_OrthoNegativeXY)
        {
            View = JungleMath::CreateViewMatrix(OrthogonalCamera.GetLocation(),
                OrthoPivot, FVector(0.0f, -1.0f, 0.0f)
            );
        }
        else
        {
            View = JungleMath::CreateViewMatrix(OrthogonalCamera.GetLocation(),
                OrthoPivot, FVector(0.0f, 0.0f, 1.0f)
            );
        }
    }
}

void FViewportClient::UpdateProjectionMatrix()
{
    AspectRatio = GetViewport()->GetD3DViewport().Width / GetViewport()->GetD3DViewport().Height;
    if (IsPerspective())
    {
        Projection = JungleMath::CreateProjectionMatrix(
            FMath::DegreesToRadians(FieldOfView),
            AspectRatio,
            NearClip,
            FarClip
        );
    }
    else
    {
        // 오쏘그래픽 너비는 줌 값과 가로세로 비율에 따라 결정됩니다.
        const float OrthoWidth = OrthoSize * AspectRatio;
        const float OrthoHeight = OrthoSize;

        // 오쏘그래픽 투영 행렬 생성 (nearPlane, farPlane 은 기존 값 사용)
        Projection = JungleMath::CreateOrthoProjectionMatrix(
            OrthoWidth,
            OrthoHeight,
            NearClip,
            FarClip
        );
    }
}

FVector FViewportClient::GetCameraLocation() const
{
    if (IsPerspective())
    {
        return PerspectiveCamera.GetLocation();
    }
    return OrthogonalCamera.GetLocation();
}

void FViewportClient::UpdateOrthoCameraLoc()
{
    switch (ViewportType)
    {
    case LVT_OrthoXY: // Top
        OrthogonalCamera.SetLocation(OrthoPivot + FVector::UpVector * FarClip * 0.5f);
        OrthogonalCamera.SetRotation(FVector(0.0f, 90.0f, -90.0f));
        break;
    case LVT_OrthoXZ: // Front
        OrthogonalCamera.SetLocation(OrthoPivot + FVector::ForwardVector * FarClip * 0.5f);
        OrthogonalCamera.SetRotation(FVector(0.0f, 0.0f, 180.0f));
        break;
    case LVT_OrthoYZ: // Left
        OrthogonalCamera.SetLocation(OrthoPivot + FVector::RightVector * FarClip * 0.5f);
        OrthogonalCamera.SetRotation(FVector(0.0f, 0.0f, 270.0f));
        break;
    case LVT_OrthoNegativeXY: // Bottom
        OrthogonalCamera.SetLocation(OrthoPivot + FVector::UpVector * -1.0f * FarClip * 0.5f);
        OrthogonalCamera.SetRotation(FVector(0.0f, -90.0f, 90.0f));
        break;
    case LVT_OrthoNegativeXZ: // Back
        OrthogonalCamera.SetLocation(OrthoPivot + FVector::ForwardVector * -1.0f * FarClip * 0.5f);
        OrthogonalCamera.SetRotation(FVector(0.0f, 0.0f, 0.0f));
        break;
    case LVT_OrthoNegativeYZ: // Right
        OrthogonalCamera.SetLocation(OrthoPivot + FVector::RightVector * -1.0f * FarClip * 0.5f);
        OrthogonalCamera.SetRotation(FVector(0.0f, 0.0f, 90.0f));
        break;
    case LVT_None:
    case LVT_Perspective:
    case LVT_MAX:
    default:
        break;
    }
}
