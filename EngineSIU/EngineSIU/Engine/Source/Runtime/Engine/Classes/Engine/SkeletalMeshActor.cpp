#include "SkeletalMeshActor.h"
#include "Components/SkeletalMeshComponent.h"

ASkeletalMeshActor::ASkeletalMeshActor()
{
    // 액터가 생성자에서 컴포넌트를 만들면 이름을 넣어줘야 정상적으로 세이브 된다.
    SkeletalMeshComponent = AddComponent<USkeletalMeshComponent>("SkeletalMeshComponent_0");
    RootComponent = SkeletalMeshComponent;
}

UObject* ASkeletalMeshActor::Duplicate(UObject* InOuter)
{
    UObject* NewActor = AActor::Duplicate(InOuter);
    ASkeletalMeshActor* NewSkeletalMeshActor = Cast<ASkeletalMeshActor>(NewActor);
    NewSkeletalMeshActor->SkeletalMeshComponent = GetComponentByFName<USkeletalMeshComponent>("SkeletalMeshComponent_0");

    return NewActor;
}

USkeletalMeshComponent* ASkeletalMeshActor::GetSkeletalMeshComponent() const
{
    return SkeletalMeshComponent;
}
