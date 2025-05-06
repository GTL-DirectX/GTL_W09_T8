#pragma once
#include "Controller.h"
#include "InputCore/InputCoreTypes.h"
#include "Character.h"

class UCameraComponent;
class APlayerCameraManager;
class UPlayerInput;
class UInputComponent;

class APlayerController : public AController
{
    DECLARE_CLASS(APlayerController, AController)

public:
    APlayerController() = default;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    virtual void UpdateCameraManager(float DeltaTime);

    // TODO Check Duplicate

public:
    ACharacter* GetCharacter() const { return Cast<ACharacter>(GetPawn()); }
    virtual void SetupInputComponent() override;
    // === 입력 기능 ===
    void AddYawInput(float Value);
    void AddPitchInput(float Value);
    bool IsInputKeyDown(EKeys::Type Key) const;
    
    void PushInputComponent(UInputComponent* InputComponent);
    void PopInputComponent(UInputComponent* InputComponent);
    const TArray<UInputComponent*>& GetInputComponentStack() const { return InputComponentStack; }

    void InputKey(EKeys::Type Key, EInputEvent EventType);
    void InputAxis(EKeys::Type Key, EInputEvent EventType);
    void MouseInput(float DeltaX, float DeltaY);

    UPROPERTY
    (APlayerCameraManager*, PlayerCameraManager, = nullptr)

    UPROPERTY
    (TArray<UInputComponent*>, InputComponentStack, = {})

protected:
    void SetupInputBindings();

    UPROPERTY
    (UPlayerInput*, PlayerInput, = nullptr)
};

