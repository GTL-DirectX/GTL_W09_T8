#pragma once
#include "GameFramework/Actor.h"
#include "UObject/Object.h"
#include "UObject/ObjectTypes.h"


class UGizmoBaseComponent;
class USceneComponent;
class FEditorViewportClient;
class UStaticMeshComponent;

class UEditorPlayer : public UObject
{
    DECLARE_CLASS(UEditorPlayer, UObject)
    UEditorPlayer() = default;
    virtual ~UEditorPlayer() = default;

public:
    void Initialize();

public:
    bool PickGizmo(FVector RayOrigin, FEditorViewportClient* InActiveViewport);
    void ProcessGizmoIntersection(USceneComponent* Component, const FVector& PickPosition, FEditorViewportClient* InActiveViewport, bool& bIsPickedGizmo);
    void PickActor(const FVector& pickPosition, std::shared_ptr<FEditorViewportClient> ActiveViewport);
    void SetControlMode();
    void SetCoordiMode();

private:
    int RayIntersectsObject(const FVector& PickPosition, USceneComponent* Component, float& HitDistance, int& IntersectCount);
    void ScreenToViewSpace(int32 ScreenX, int32 ScreenY, std::shared_ptr<FEditorViewportClient> ActiveViewport, FVector& RayOrigin);
    void PickedObjControl();
    void ControlRotation(USceneComponent* Component, UGizmoBaseComponent* Gizmo, float DeltaX, float DeltaY);
    void ControlScale(USceneComponent* Component, UGizmoBaseComponent* Gizmo, float DeltaX, float DeltaY);

    POINT m_LastMousePos;
    EControlMode ControlMode = CM_TRANSLATION;
    ECoordMode CoordMode = CDM_WORLD;

public:
    void SetMode(EControlMode Mode) { ControlMode = Mode; }
    EControlMode GetControlMode() const { return ControlMode; }
    ECoordMode GetCoordMode() const { return CoordMode; }
};
