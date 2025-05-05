#include "DefaultPawn.h"

#include "Components/InputComponent.h"
#include "Camera/CameraComponent.h"

#include "Engine/Lua/LuaUtils/LuaTypeMacros.h"

ADefaultPawn::ADefaultPawn()
{
    FollowCamera = AddComponent<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(RootComponent);
    FollowCamera->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
}

void ADefaultPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    // Bind input actions and axes here

    PlayerInputComponent->BindAxis("MoveForward", [this](float Value) { MoveForward(Value); });
    PlayerInputComponent->BindAxis("MoveRight", [this](float Value) { MoveRight(Value); });
    PlayerInputComponent->BindAxis("Turn", [this](float Value) { AddYawInput(Value); });
    PlayerInputComponent->BindAxis("LookUp", [this](float Value) { AddPitchInput(Value); });

}

UObject* ADefaultPawn::Duplicate(UObject* InOuter)
{
    UObject* NewActor = Super::Duplicate(InOuter);

    return NewActor;
}

void ADefaultPawn::RegisterLuaType(sol::state& Lua)
{
    DEFINE_LUA_TYPE_WITH_PARENT(ADefaultPawn, sol::bases<APawn>())
}

bool ADefaultPawn::BindSelfLuaProperties()
{
    return false;
}

void ADefaultPawn::MoveForward(float Value)
{
    if (Value != 0.0f)
    {
        AddMovementInput(GetActorForwardVector(), Value);
        SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z));
    }
}

void ADefaultPawn::MoveRight(float Value)
{
    if (Value != 0.0f)
    {
        AddMovementInput(GetActorRightVector(), Value);
        SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z));
    }
}

void ADefaultPawn::AddYawInput(float Value)
{
    if (Value != 0.0f)
    {
        AddControllerYawInput(Value);
    }
}

void ADefaultPawn::AddPitchInput(float Value)
{
    if (Value != 0.0f)
    {
        AddControllerPitchInput(Value);
    }
}

