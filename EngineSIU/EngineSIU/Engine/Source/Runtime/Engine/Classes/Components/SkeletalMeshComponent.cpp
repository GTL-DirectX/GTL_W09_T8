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
