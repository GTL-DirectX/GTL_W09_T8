#include "Material.h"
#include "UObject/Casts.h"
#include "UObject/ObjectFactory.h"


UObject* UMaterial::Duplicate(UObject* InOuter)
{
    ThisClass* NewMaterial = Cast<ThisClass>(Super::Duplicate(InOuter));

    NewMaterial->materialInfo = materialInfo;

    return NewMaterial;
}


UMaterial* UMaterial::CreateMaterial(FObjMaterialInfo materialInfo)
{
    if (materialMap[materialInfo.MaterialName] != nullptr)
        return materialMap[materialInfo.MaterialName];

    UMaterial* newMaterial = FObjectFactory::ConstructObject<UMaterial>(nullptr); // Material은 Outer가 없이 따로 관리되는 객체이므로 Outer가 없음으로 설정. 추후 Garbage Collection이 추가되면 AssetManager를 생성해서 관리.
    newMaterial->SetMaterialInfo(materialInfo);
    materialMap.Add(materialInfo.MaterialName, newMaterial);
    return newMaterial;
}

UMaterial* UMaterial::GetMaterial(FString name)
{
    return materialMap[name];
}
