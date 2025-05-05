#include "SkeletalMeshCompnent.h"
#include "UObject/ObjectFactory.h"
#include "Engine/Resource/FBXManager.h"
#include "Rendering/Mesh/SkeletalMesh.h"

USkeletalMeshCompnent::USkeletalMeshCompnent()
{
    SkeletalMesh = FObjectFactory::ConstructObject<USkeletalMesh>(nullptr);

    // 이 쓰레기코드는 뭐지
    FSkeletalMeshRenderData* test = new FSkeletalMeshRenderData();
    FFBXManager::Get().LoadFbx("C:\\Users\\Jungle\\Desktop\\ywj\\fbx\\character.fbx", *test);

    SkeletalMesh->SetData(test);
}
