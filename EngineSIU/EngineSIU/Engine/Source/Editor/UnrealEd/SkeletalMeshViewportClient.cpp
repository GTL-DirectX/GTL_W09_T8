#include "SkeletalMeshViewportClient.h"


#include "UObject/Casts.h"
#include "UObject/ObjectFactory.h"
#include "BaseGizmos/TransformGizmo.h"
#include "World/World.h"
#include "Components/SkeletalMeshComponent.h"

#include "Viewport/SkeletalMeshEditorViewport.h"

FSkeletalMeshViewportClient::FSkeletalMeshViewportClient()
{
}

void FSkeletalMeshViewportClient::Initialize(EViewScreenLocation InViewportIndex, const FRect& InRect, UWorld* InWorld)
{
    FEditorViewportClient::Initialize(InViewportIndex, InRect, InWorld);
    
    if (Viewport)
    {
        delete Viewport;
    }
    Viewport = new FSkeletalMeshEditorViewport(InViewportIndex);
    dynamic_cast<FSkeletalMeshEditorViewport*>(Viewport)->SetWorld(InWorld);
    Viewport->Initialize(InRect);

    PerspectiveCamera.SetLocation(FVector(-50, 0, 5));
    FVector TargetLocation = FVector(0, 0, 0);
    FVector CamLocation = PerspectiveCamera.GetLocation();
    FVector Dir = (TargetLocation - CamLocation).GetSafeNormal();
    
    FVector forward;

    float pitch = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.Z));
    float yaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.X, Dir.Z));

    forward.X = FMath::Cos(FMath::DegreesToRadians(pitch)) * FMath::Cos(FMath::DegreesToRadians(yaw));
    forward.Y = FMath::Cos(FMath::DegreesToRadians(pitch)) * FMath::Sin(FMath::DegreesToRadians(yaw));
    forward.Z = FMath::Sin(FMath::DegreesToRadians(pitch));

    PerspectiveCamera.SetRotation(forward);

}

void FSkeletalMeshViewportClient::Tick(float DeltaTime)
{
    FEditorViewportClient::Tick(DeltaTime);

}
