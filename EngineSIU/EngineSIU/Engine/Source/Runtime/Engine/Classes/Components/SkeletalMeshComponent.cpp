#include "SkeletalMeshComponent.h"
#include "Engine/Resource/FBXManager.h"
#include "UObject/Casts.h"

USkeletalMeshComponent::USkeletalMeshComponent()
{
    //SkeletalMeshAsset = FFBXManager::Get().LoadSkeletalMesh("C:\\Users\\Jungle\\Desktop\\character.fbx");
}

UObject* USkeletalMeshComponent::Duplicate(UObject* InOuter)
{
    ThisClass* NewComponent = Cast<ThisClass>(Super::Duplicate(InOuter));
    NewComponent->SkeletalMeshAsset = SkeletalMeshAsset;
    NewComponent->AABB = AABB;

    return NewComponent;
}

void USkeletalMeshComponent::GetProperties(TMap<FString, FString>& OutProperties) const
{
    Super::GetProperties(OutProperties);

    USkeletalMesh* CurrentMesh = GetSkeletalMesh();
    if (CurrentMesh != nullptr) {

        // 1. std::wstring 경로 얻기
        FString Path = CurrentMesh->GetObjectName(); // 이 함수가 std::wstring 반환 가정

        OutProperties.Add(TEXT("SkeletalMeshPath"), Path);
    }
    else
    {
        OutProperties.Add(TEXT("SkeletalMeshPath"), TEXT("None")); // 메시 없음 명시
    }
}

void USkeletalMeshComponent::SetProperties(const TMap<FString, FString>& InProperties)
{
    Super::SetProperties(InProperties);
    const FString* TempStr = nullptr;

    TempStr = InProperties.Find(TEXT("SkeletalMeshPath"));
    if (TempStr)
    {
        if (*TempStr != TEXT("None")) // 값이 "None"이 아닌지 확인
        {
            // 경로 문자열로 UStaticMesh 에셋 로드 시도

            if (USkeletalMesh* MeshToSet = FFBXManager::Get().LoadSkeletalMesh(*TempStr))
            {
                SetSkeletalMesh(MeshToSet); // 성공 시 메시 설정
                UE_LOG(LogLevel::Display, TEXT("Set SkeletalMesh '%s' for %s"), **TempStr, *GetName());
            }
            else
            {
                // 로드 실패 시 경고 로그
                UE_LOG(LogLevel::Warning, TEXT("Could not load SkeletalMesh '%s' for %s"), **TempStr, *GetName());
                SetSkeletalMesh(nullptr); // 안전하게 nullptr로 설정
            }
        }
    }
    else
    {
        SetSkeletalMesh(nullptr); // 안전하게 nullptr로 설정
        UE_LOG(LogLevel::Display, TEXT("Set SkeletalMesh to None for %s"), *GetName());
    }
}

int USkeletalMeshComponent::CheckRayIntersection(const FVector& InRayOrigin, const FVector& InRayDirection, float& OutHitDistance) const
{
    if (!AABB.Intersect(InRayOrigin, InRayDirection, OutHitDistance))
    {
        return 0;
    }
    if (SkeletalMeshAsset == nullptr)
    {
        return 0;
    }

    OutHitDistance = FLT_MAX;

    int IntersectionNum = 0;

    FSkeletalMeshRenderData* RenderData = SkeletalMeshAsset->GetRenderData();
    const TArray<FSkeletalMeshVertex>& Vertices = RenderData->Vertices;
    const int32 VertexNum = Vertices.Num();
    if (VertexNum == 0)
    {
        return 0;
    }

    const TArray<UINT>& Indices = RenderData->Indices;
    const int32 IndexNum = Indices.Num();
    const bool bHasIndices = (IndexNum > 0);

    int32 TriangleNum = bHasIndices ? (IndexNum / 3) : (VertexNum / 3);
    for (int32 i = 0; i < TriangleNum; i++)
    {
        int32 Idx0 = i * 3;
        int32 Idx1 = i * 3 + 1;
        int32 Idx2 = i * 3 + 2;

        if (bHasIndices)
        {
            Idx0 = Indices[Idx0];
            Idx1 = Indices[Idx1];
            Idx2 = Indices[Idx2];
        }

        // 각 삼각형의 버텍스 위치를 FVector로 불러옵니다.
        FVector v0 = FVector(Vertices[Idx0].Position.X, Vertices[Idx0].Position.Y, Vertices[Idx0].Position.Z);
        FVector v1 = FVector(Vertices[Idx1].Position.X, Vertices[Idx1].Position.Y, Vertices[Idx1].Position.Z);
        FVector v2 = FVector(Vertices[Idx2].Position.X, Vertices[Idx2].Position.Y, Vertices[Idx2].Position.Z);

        float HitDistance = FLT_MAX;
        if (IntersectRayTriangle(InRayOrigin, InRayDirection, v0, v1, v2, HitDistance))
        {
            OutHitDistance = FMath::Min(HitDistance, OutHitDistance);
            IntersectionNum++;
        }

    }
    return IntersectionNum;
}

void USkeletalMeshComponent::SetSkeletalMesh(USkeletalMesh* InSkeletalMesh)
{
    SkeletalMeshAsset = InSkeletalMesh;

    if (SkeletalMeshAsset)
    {
        FVector Min, Max;

        Max = SkeletalMeshAsset->GetRenderData()->BoundingBoxMax;
        Min = SkeletalMeshAsset->GetRenderData()->BoundingBoxMin;
        AABB = FBoundingBox(Min, Max);
    }
    else
    {
        AABB = FBoundingBox(FVector::ZeroVector, FVector::ZeroVector);
    }
}

USkeletalMesh* USkeletalMeshComponent::GetSkeletalMesh() const
{
    return SkeletalMeshAsset;
}
