#pragma once
#include "Container/Array.h"
#include "D3D11RHI/GraphicDevice.h"
#include "HAL/PlatformType.h"
#include "UObject/NameTypes.h"

#define UE_LOG Console::GetInstance().AddLog

enum class LogLevel : uint8
{
    Display,
    Warning,
    Error
};

class StatOverlay
{
public:
    // @todo Stat-FPS Default 설정 복구 (showFPS = false, showRender = false)
    bool ShowFPS = false;
    bool ShowMemory = false;
    bool ShowLight = false;
    bool ShowRender = false;

    // Begin Test
    void RenderStatWidgets() const;
    // End Test

    void ToggleStat(const std::string& Command);
    void Render(ID3D11DeviceContext* Context, UINT Width, UINT Height) const;
};

struct FProfiledScope
{
    FString DisplayName;
    FName CPUStatName;
    FName GPUStatName;
};

class FGPUTimingManager;

class FEngineProfiler
{
public:
    FEngineProfiler() = default;
    ~FEngineProfiler() = default;

    void SetGPUTimingManager(FGPUTimingManager* InGPUTimingManager);
    void Render(ID3D11DeviceContext* Context, UINT Width, UINT Height);
    void RegisterStatScope(const FString& DisplayName, const FName& CPUStatName, const FName& GPUStatName);

private:
    FGPUTimingManager* GPUTimingManager = nullptr;
    TArray<FProfiledScope> TrackedScopes;
    bool bShowWindow = true;
};

class Console
{
private:
    Console() = default;

public:
    // 복사 방지
    Console(const Console&) = delete;
    Console& operator=(const Console&) = delete;
    Console(Console&&) = delete;
    Console& operator=(Console&&) = delete;

public:
    static Console& GetInstance(); // 참조 반환으로 변경

    void Clear();
    void AddLog(LogLevel Level, const char* Format, ...);
    void Draw();
    void ExecuteCommand(const std::string& Command);
    void OnResize(HWND hWnd);
public:
    struct LogEntry
    {
        LogLevel Level;
        FString Message;
    };

    TArray<LogEntry> Items;
    TArray<FString> History;
    int32 HistoryPos = -1;
    char InputBuf[256] = "";
    bool ScrollToBottom = false;

    bool ShowLogTemp = true; // LogTemp 체크박스
    bool ShowWarning = true; // Warning 체크박스
    bool ShowError = true;   // Error 체크박스

    bool bWasOpen = true;

    StatOverlay Overlay;

private:
    bool bExpand = true;
    UINT Width;
    UINT Height;
};
