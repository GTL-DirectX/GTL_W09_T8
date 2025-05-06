#pragma once
#include "GameFramework/Actor.h"

class USkeletalMeshComponent;

class ASkeletalMeshActor : public AActor
{
    DECLARE_CLASS(ASkeletalMeshActor, AActor)
public:
    ASkeletalMeshActor();

    UObject* Duplicate(UObject* InOuter) override;
    virtual void GetProperties(TMap<FString, FString>& OutProperties) const;

    virtual void SetProperties(const TMap<FString, FString>& InProperties);

    
    USkeletalMeshComponent* GetSkeletalMeshComponent() const;

protected:
    UPROPERTY
    (USkeletalMeshComponent*, SkeletalMeshComponent, = nullptr);
};
