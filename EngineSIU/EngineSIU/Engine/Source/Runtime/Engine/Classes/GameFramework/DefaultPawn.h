#pragma once

#include "Pawn.h"

class UCameraComponent;

class ADefaultPawn : public APawn
{
    DECLARE_CLASS(ADefaultPawn, APawn)

public:
    ADefaultPawn();
    
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual UObject* Duplicate(UObject* InOuter) override;
    // === Lua 관련 ===
    virtual void RegisterLuaType(sol::state& Lua) override; // Lua에 클래스 등록해주는 함수.
    virtual bool BindSelfLuaProperties() override; // LuaEnv에서 사용할 멤버 변수 등록 함수.


private:
    void MoveForward(float Value);
    void MoveRight(float Value);
    void AddYawInput(float Value);
    void AddPitchInput(float Value);

private:
    UCameraComponent* FollowCamera = nullptr;

};

