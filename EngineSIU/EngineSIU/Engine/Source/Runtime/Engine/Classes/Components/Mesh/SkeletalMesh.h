#pragma once

#include "SkinnedAsset.h"

class FSkeletalMeshRenderData;

class USkeletalMesh : public USkinnedAsset
{   
    DECLARE_CLASS(USkeletalMesh, USkinnedAsset)

private:
    FSkeletalMeshRenderData* RenderData = nullptr;


};

