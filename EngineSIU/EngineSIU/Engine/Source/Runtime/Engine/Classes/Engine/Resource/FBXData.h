#pragma once
#include "Container/String.h"
#include "Container/Array.h"
#include "Container/Map.h"
#include "Math/Matrix.h"
#include <fbxsdk.h>

namespace FBX
{
    struct FNodeInfo {
        FString Name;
        int32 ParentIndex = -1; // Index in the flat node list, -1 for root's children
        FMatrix LocalTransform; // Converted to engine coordinate system
        FMatrix WorldTransform; // Converted to engine coordinate system (calculated during import)
        int32 AttributeIndex = -1; // Index into MeshData, LightData etc. if applicable
        FbxNodeAttribute::EType AttributeType = FbxNodeAttribute::eUnknown; // Store the type
        // Store the original FbxNode* temporarily during import for parent lookup
        FbxNode* TempFbxNodePtr = nullptr;
    };

    struct FMeshData {
        FString NodeName; // Name of the node holding this mesh
        // FSkeletalMeshRenderData members (Vertices, Indices, UVs, Normals, etc.)
        TArray<FVector> Vertices;
        TArray<uint32> Indices;
        TArray<FVector2D> UVs;
        TArray<FVector> Normals;
        // ... Tangents, Skinning Weights, Bone Indices ...
        // Material information can be added here too
    };

    struct FSkeletonBoneInfo {
        FString Name;
        int32 ParentIndex = -1; // Index within the bone list
        FMatrix ReferencePose; // Bind pose or reference pose (World or Local - choose convention)
        // Store the original FbxNode* temporarily for parent lookup
        FbxNode* TempFbxNodePtr = nullptr;
    };

    struct FLightData {
        FString NodeName;
        // Light properties (Type, Color, Intensity, Cone Angle, etc.)
        // Extracted from FbxLight attribute
        FVector Position; // From node's world transform
        FVector Direction; // From node's world transform
        // ... other properties ...
    };

    struct FCameraData {
        FString NodeName;
        // Camera properties (FOV, Aspect Ratio, Projection Type, Near/Far Plane)
        // Extracted from FbxCamera attribute
        FVector Position;
        FVector Target; // Or Rotation/Direction
        // ... other properties ...
    };

    // Main structure to hold all imported scene data
    struct FImportSceneData {
        FString FilePath;
        TArray<FNodeInfo> NodeHierarchy; // Flat list of all nodes
        TArray<FMeshData> MeshDatas;
        TArray<FSkeletonBoneInfo> SkeletonBones; // Assuming one main skeleton for now
        TArray<FLightData> LightDatas;
        TArray<FCameraData> CameraDatas;

        // Temporary map for finding parent indices during import
        TMap<FbxNode*, int32> FbxNodeToIndexMap;

        void Clear() {
            NodeHierarchy.Empty();
            MeshDatas.Empty();
            SkeletonBones.Empty();
            LightDatas.Empty();
            CameraDatas.Empty();
            FbxNodeToIndexMap.Empty();
        }
    };
}

