#include "FireballActor.h"
#include "Engine/FLoaderOBJ.h"

#include "Components/Light/PointLightComponent.h"

#include "Components/ProjectileMovementComponent.h"

#include "Components/SphereComp.h"

AFireballActor::AFireballActor()
{
    FObjManager::CreateStaticMesh("Contents/Sphere.obj");


    SphereComp = AddComponent<USphereComp>("USphereComp_0");
    
    SphereComp->SetStaticMesh(FObjManager::GetStaticMesh(L"Contents/Sphere.obj"));
  
    PointLightComponent = AddComponent<UPointLightComponent>("UPointLightComponent_0");
    
    PointLightComponent->SetLightColor(FLinearColor::Red);
    
    ProjectileMovementComponent = AddComponent<UProjectileMovementComponent>("UProjectileMovementComponent_0");
    PointLightComponent->AttachToComponent(RootComponent);

    ProjectileMovementComponent->SetGravity(0);
    ProjectileMovementComponent->SetVelocity(FVector(100, 0, 0));
    ProjectileMovementComponent->SetInitialSpeed(100);
    ProjectileMovementComponent->SetMaxSpeed(100);
    ProjectileMovementComponent->SetLifetime(10);
}

AFireballActor::~AFireballActor()
{
}

void AFireballActor::BeginPlay()
{
}

UObject* AFireballActor::Duplicate(UObject* InOuter)
{
    UObject* NewActor = AActor::Duplicate(InOuter);
    AFireballActor* FireballActor = Cast<AFireballActor>(NewActor);
    FireballActor->ProjectileMovementComponent = GetComponentByFName<UProjectileMovementComponent>("UProjectileMovementComponent_0");
    FireballActor->SphereComp = GetComponentByFName<USphereComp>("USphereComp_0");
    FireballActor->PointLightComponent = GetComponentByFName<UPointLightComponent>("UPointLightComponent_0");

    return NewActor;
}
