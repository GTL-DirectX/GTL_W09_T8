#include "SkeletalMeshComponent.h"
#include "Engine/Resource/FBXManager.h"

USkeletalMeshComponent::USkeletalMeshComponent()
{
    SkeletalMeshAsset = FFBXManager::Get().LoadSkeletalMesh("C:\\Users\\Jungle\\Desktop\\character.fbx");
}

void USkeletalMeshComponent::SetSkeletalMesh(USkeletalMesh* InSkeletalMesh)
{
    SkeletalMeshAsset = InSkeletalMesh;
}

USkeletalMesh* USkeletalMeshComponent::GetSkeletalMesh() const
{
    return SkeletalMeshAsset;
}
