#include "Pawn.h"
#include "Controller.h"
#include "Components/InputComponent.h"
#include "PlayerController.h"
#include "Components/LuaScriptComponent.h"
#include "Engine/Lua/LuaUtils/LuaTypeMacros.h"

void APawn::BeginPlay()
{
    Super::BeginPlay();
}

void APawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!PendingMovement.IsNearlyZero())
    {
        // 최대 길이를 1로 정규화한 뒤 속도·델타타임 곱해서 이동량 계산
        FVector Dir = PendingMovement.GetClampedToMaxSize(1.0f);
        FVector Delta = Dir * MoveSpeed * DeltaTime;

        // sweep=true 로 충돌 처리
        //AddActorWorldOffset(Delta, true);

        // 일단 충돌 처리 없이 이동
        SetActorLocation(GetActorLocation() + Delta);

        // 다음 프레임을 위해 초기화
        PendingMovement = FVector::ZeroVector;
    }
}

void APawn::Destroyed()
{
    Super::Destroyed();
}

void APawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
}

void APawn::GetProperties(TMap<FString, FString>& OutProperties) const
{
    Super::GetProperties(OutProperties);
    OutProperties.Add(TEXT("MoveSpeed"), std::to_string(MoveSpeed));
    OutProperties.Add(TEXT("PendingMovement"), PendingMovement.ToString());
}

void APawn::SetProperties(const TMap<FString, FString>& InProperties)
{
    Super::SetProperties(InProperties);
    const FString* TempStr = nullptr;
    TempStr = InProperties.Find(TEXT("MoveSpeed"));
    if (TempStr)
    {
        MoveSpeed = std::stof(GetData(*TempStr));
    }
    TempStr = InProperties.Find(TEXT("PendingMovement"));
    if (TempStr)
    {
        PendingMovement.InitFromString(*TempStr);
    }
}

void APawn::PossessedBy(AController* NewController)
{
    Controller = NewController;

    if (APlayerController* PC = Cast<APlayerController>(Controller))
    {
        EnableInput(PC);
    }

}

void APawn::UnPossessed()
{
    if (APlayerController* PC = Cast<APlayerController>(Controller))
    {
        DisableInput(PC);
    }

    Controller = nullptr;
}

void APawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
}

void APawn::AddMovementInput(FVector WorldDirection, float ScaleValue)
{
    if (WorldDirection.IsZero() || ScaleValue == 0.0f)
    {
        return;
    }
    // 이동량을 누적
    PendingMovement += WorldDirection.GetSafeNormal() * ScaleValue;
}

void APawn::AddControllerYawInput(float Value)
{
    if (Value != 0.0f)
    {
        // Yaw 회전 처리
        FRotator NewRotation = GetActorRotation();
        NewRotation.Yaw += Value * 0.2;
        SetActorRotation(NewRotation);
    }
}

void APawn::AddControllerPitchInput(float Value)
{
    if (Value != 0.0f)
    {
        // Pitch 회전 처리
        FRotator NewRotation = GetActorRotation();
        NewRotation.Pitch -= Value * 0.2;
        SetActorRotation(NewRotation);
    }
}

FVector APawn::GetPawnViewLocation() const
{
    return FVector();
}

FRotator APawn::GetViewRotation() const
{
    return FRotator();
}

void APawn::EnableInput(APlayerController* PlayerController)
{
    Super::EnableInput(PlayerController);

    if (InputComponent)
    {
        SetupPlayerInputComponent(InputComponent);
    }
}

void APawn::DisableInput(APlayerController* PlayerController)
{
    Super::DisableInput(PlayerController);
}

void APawn::RegisterLuaType(sol::state& Lua)
{
    DEFINE_LUA_TYPE_WITH_PARENT(APawn, sol::bases<AActor>());
}

bool APawn::BindSelfLuaProperties()
{
    Super::BindSelfLuaProperties();

    sol::table& LuaTable = LuaScriptComponent->GetLuaSelfTable();
    if (!LuaTable.valid())
    {
        return false;
    }
    LuaTable["this"] = this;

    return true;
}
