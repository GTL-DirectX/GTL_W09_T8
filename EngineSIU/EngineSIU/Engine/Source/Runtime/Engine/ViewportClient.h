#pragma once
#define _TCHAR_DEFINED
#include <d3d11.h>

#include "EngineBaseTypes.h"
#include "HAL/PlatformType.h"
#include "Math/Matrix.h"

#define MIN_ORTHOZOOM (1.0)  // 2D ortho viewport zoom >= MIN_ORTHOZOOM
#define MAX_ORTHOZOOM (1e25)

class FViewportResource;
class FViewport;
class UWorld;

struct FViewportCamera
{
public:
    FViewportCamera() = default;

    /** Sets the transform's location */
    void SetLocation(const FVector& Position)
    {
        ViewLocation = Position;
    }

    /** Sets the transform's rotation */
    void SetRotation(const FVector& Rotation)
    {
        ViewRotation = Rotation;
    }

    /** Sets the location to look at during orbit */
    void SetLookAt(const FVector& InLookAt)
    {
        LookAt = InLookAt;
    }

    /** Set the ortho zoom amount */
    void SetOrthoZoom(float InOrthoZoom)
    {
        assert(InOrthoZoom >= MIN_ORTHOZOOM && InOrthoZoom <= MAX_ORTHOZOOM);
        OrthoZoom = InOrthoZoom;
    }

    /** Check if transition curve is playing. */
    /*    bool IsPlaying();*/

    /** @return The transform's location */
    FORCEINLINE const FVector& GetLocation() const { return ViewLocation; }

    /** @return The transform's rotation */
    FORCEINLINE const FVector& GetRotation() const { return ViewRotation; }

    /** @return The look at point for orbiting */
    FORCEINLINE const FVector& GetLookAt() const { return LookAt; }

    /** @return The ortho zoom amount */
    FORCEINLINE float GetOrthoZoom() const { return OrthoZoom; }

    FVector GetForwardVector() const;
    FVector GetRightVector() const;
    FVector GetUpVector() const;

public:
    /** Current viewport Position. */
    FVector ViewLocation;
    /** Current Viewport orientation; valid only for perspective projections. */
    FVector ViewRotation;
    FVector DesiredLocation;
    /** When orbiting, the point we are looking at */
    FVector LookAt;
    /** Viewport start location when animating to another location */
    FVector StartLocation;
    /** Ortho zoom amount */
    float OrthoZoom;
};

class FViewportClient
{
public:
    FViewportClient();
    virtual ~FViewportClient();

    // FViewport에서 발생하는 이벤트를 처리하는 가상 함수들
    //virtual void OnInput(const FInputEvent& Event) = 0;
    virtual void Draw(FViewport* Viewport);
    virtual void Tick(float DeltaTime);
    virtual void Release();
    
    // FViewport에 대한 참조 (혹은 소유)

public:
    FViewport* GetViewport() const { return Viewport; }
    uint32 GetViewportIndex() const { return ViewportIndex; }
    D3D11_VIEWPORT& GetD3DViewport() const;
    FViewportResource* GetViewportResource();
    void SetWorld(UWorld* InWorld) { World = InWorld; }
    UWorld* GetWorld() const { return World; }

protected:
    FViewport* Viewport = nullptr;
    uint32 ViewportIndex = 0;
    FViewportResource* ViewportResourceCache = nullptr;


public:
    ELevelViewportType GetViewportType() const;
    void SetViewportType(ELevelViewportType InViewportType);
    
    EViewModeIndex GetViewMode() const { return ViewMode; }
    void SetViewMode(EViewModeIndex InViewMode) { ViewMode = InViewMode; }
    
    uint64 GetShowFlag() const { return ShowFlag; }
    void SetShowFlag(uint64 InShowFlag) { ShowFlag = InShowFlag; }

protected:
    ELevelViewportType ViewportType;
    uint64 ShowFlag;
    EViewModeIndex ViewMode;

protected:
    // 참조중인 월드 정보
    UWorld* World = nullptr;

    
    // 카메라 정보
public:
    FMatrix& GetViewMatrix() { return View; }
    FMatrix& GetProjectionMatrix() { return Projection; }
    
    float GetFieldOfView() const { return FieldOfView; }
    void SetFieldOfView(float InFieldOfView) { FieldOfView = InFieldOfView; }

    float GetAspectRatio() const { return AspectRatio; }
    void SetAspectRatio(float InAspectRatio) { AspectRatio = InAspectRatio; }
    
    float GetNearClip() const { return NearClip; }
    float GetFarClip() const { return FarClip; }

    bool IsOrthographic() const;
    bool IsPerspective() const;
    
    virtual void UpdateViewMatrix();
    virtual void UpdateProjectionMatrix();

    FViewportCamera& GetPerspectiveCamera() { return PerspectiveCamera; }
    FViewportCamera& GetOrthogonalCamera() { return OrthogonalCamera; }
    FVector GetCameraLocation() const;

public:
    void UpdateOrthoCameraLoc();
    
protected:
    //카메라
    /** Viewport camera transform data for perspective viewports */
    FViewportCamera PerspectiveCamera;
    FViewportCamera OrthogonalCamera;
    
    float FieldOfView = 90.0f;
    float AspectRatio;
    float NearClip = 0.1f;
    float FarClip = 1000.0f;

    FMatrix View;
    FMatrix Projection;

public:
    static FVector GetOrthoPivot() { return OrthoPivot; }
    static void SetOrthoPivot(FVector InOrthoPivot) { OrthoPivot = InOrthoPivot; }
    static float GetOrthoSize() { return OrthoSize; }
    static void SetOrthoSize(float InOrthoSize)
    {
        OrthoSize = InOrthoSize;
        OrthoSize = FMath::Max(OrthoSize, 0.1f);
    }
    
protected:
    static FVector OrthoPivot;
    static float OrthoSize;
};
