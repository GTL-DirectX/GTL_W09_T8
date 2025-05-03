#include "SkeletalMeshCompnent.h"

#include "Engine/Resource/FBXManager.h"

USkeletalMeshCompnent::USkeletalMeshCompnent()
{
    FFBXManager::Get().LoadFbx("C:\\Users\\Jungle\\Desktop\\character.fbx", test);
}
