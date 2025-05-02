#include "Material.h"
#include "UObject/Casts.h"


UObject* UMaterial::Duplicate(UObject* InOuter)
{
    ThisClass* NewMaterial = Cast<ThisClass>(Super::Duplicate(InOuter));

    NewMaterial->materialInfo = materialInfo;

    return NewMaterial;
}
