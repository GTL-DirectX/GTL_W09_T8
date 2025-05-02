#pragma once

#include <memory>
#define _TCHAR_DEFINED
#include <d3d11.h>

#include "Define.h"
#include "Core/Container/Array.h"
#include "Core/Container/Map.h"

#include "Engine/Classes/Engine/Texture.h"

#include "Rendering/Types/Buffers.h"


struct FDebugPrimitiveData
{
    FVertexInfo VertexInfo;
    FIndexInfo IndexInfo;
};

// Icon
enum class IconType
{
    None,
    DirectionalLight,
    PointLight,
    SpotLight,
    AmbientLight,
    ExponentialFog,
    AtmosphericFog,
};

struct FRenderResourcesDebug
{
    struct FWorldComponentContainer
    {
        TArray<class UStaticMeshComponent*> StaticMeshComponent;
        TArray<class ULightComponentBase*> Light;
        TArray<class UHeightFogComponent*> Fog;

        TArray<class UBoxComponent*> BoxComponents;
        TArray<class USphereComponent*> SphereComponents;
        TArray<class UCapsuleComponent*> CapsuleComponents;
    } Components;

    struct FPrimitiveResourceContainer
    {
        FDebugPrimitiveData Box;
        FDebugPrimitiveData Sphere;
        FDebugPrimitiveData Cone;
        FDebugPrimitiveData Arrow;
        FDebugPrimitiveData Capsule;
    } Primitives;

    TMap<IconType, std::shared_ptr<FTexture>> IconTextures; // 사용 X
};
