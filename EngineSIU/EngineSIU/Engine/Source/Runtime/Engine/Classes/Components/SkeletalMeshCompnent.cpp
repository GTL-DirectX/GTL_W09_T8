#include "SkeletalMeshCompnent.h"

#include "Engine/Resource/FBXManager.h"

#include "Rendering/Mesh/SkeletalMesh.h"

USkeletalMeshCompnent::USkeletalMeshCompnent()
{
    // 경로 다시 잡아주기.
    SkeletalMeshAsset = FFBXManager::Get().LoadSkeletalMesh("character.fbx");
}

void USkeletalMeshCompnent::SetSkeletalMesh(USkeletalMesh* InSkeletalMesh)
{
    SkeletalMeshAsset = InSkeletalMesh;
}

USkeletalMesh* USkeletalMeshCompnent::GetSkeletalMesh() const
{
    return SkeletalMeshAsset;
}
