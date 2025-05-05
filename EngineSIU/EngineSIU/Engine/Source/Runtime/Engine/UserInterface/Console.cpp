#include "Console.h"
#include <cstdarg>
#include <cstdio>
#include "UnrealEd/EditorViewportClient.h"
#include "Engine/Engine.h"
#include "Renderer/UpdateLightBufferPass.h"
#include "UObject/Casts.h"
#include "UObject/UObjectIterator.h"
#include "Components/Light/LightComponent.h"
#include "Components/Light/PointLightComponent.h"
#include "Components/Light/SpotLightComponent.h"
#include "Components/Light/DirectionalLightComponent.h"
#include "Components/Light/AmbientLightComponent.h"
#include "ImGUI/imgui.h"
#include "Stats/ProfilerStatsManager.h"
#include "Stats/GPUTimingManager.h"

void StatOverlay::RenderStatWidgets() const 
{
    if (!ShowRender)
    {
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f)); // 초록색 텍스트

    if (ShowFPS)
    {
        float FrameTimeMS = ImGui::GetIO().DeltaTime * 1000.0f;
        float FPS = (FrameTimeMS > 0.0f) ? 1.0f / ImGui::GetIO().DeltaTime : 0.0f;

        ImGui::Text("%.2f FPS (%.2f ms)", FPS, FrameTimeMS);
        // ImGui::Text("\n");
    }

    if (ShowMemory)
    {
        ImGui::Text("Obj Cnt: %llu, Mem: %llu B", FPlatformMemory::GetAllocationCount<EAT_Object>(), FPlatformMemory::GetAllocationBytes<EAT_Object>());
        ImGui::Text("Cont Cnt: %llu, Mem: %llu B", FPlatformMemory::GetAllocationCount<EAT_Container>(), FPlatformMemory::GetAllocationBytes<EAT_Container>());
    }

    if (ShowLight)
    {
        int NumPointLights = 0;
        int NumSpotLights = 0;
        if (GEngine && GEngine->ActiveWorld)
        {
            for (const auto iter : TObjectRange<ULightComponentBase>())
            {
                if (iter->GetWorld() == GEngine->ActiveWorld)
                {
                    if (Cast<UPointLightComponent>(iter)) NumPointLights++;
                    else if (Cast<USpotLightComponent>(iter)) NumSpotLights++;
                }
            }
        }
        ImGui::Text("[Lights] Point: %d, Spot: %d", NumPointLights, NumSpotLights);
        // ImGui::Text("\n"); 
    }

    ImGui::PopStyleColor(); 
    ImGui::Separator();
}


void StatOverlay::ToggleStat(const std::string& Command)
{
    if (Command == "stat fps")
    {
        ShowFPS = true;
        ShowRender = true;
    }
    else if (Command == "stat memory")
    {
        ShowMemory = true;
        ShowRender = true;
    }
    else if (Command == "stat light")
    {
        ShowLight = true;
        ShowRender = true;
    }
    else if (Command == "stat none")
    {
        ShowFPS = false;
        ShowMemory = false;
        ShowLight = false;
        ShowRender = false;
    }
}

void StatOverlay::Render(ID3D11DeviceContext* Context, UINT Width, UINT Height) const
{
    if (!ShowRender)
    {
        return;
    }

    const ImVec2 DisplaySize = ImGui::GetIO().DisplaySize;
    // 창 크기를 화면의 50%로 설정합니다.
    const ImVec2 WindowSize(DisplaySize.x * 0.5f, DisplaySize.y * 0.5f);
    // 창을 중앙에 배치하기 위해 위치를 계산합니다.
    const ImVec2 WindowPos((DisplaySize.x - WindowSize.x) * 0.5f, (DisplaySize.y - WindowSize.y) * 0.5f);

    ImGui::SetNextWindowPos(WindowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(WindowSize, ImGuiCond_Always);

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
    ImGui::Begin("Stat Overlay", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar);

    if (ShowFPS)
    {
        static float LastTime = ImGui::GetTime();
        static float FPS = 0.0f;
        static float FrameTimeMS = 0.0f;

        float CurrentTime = ImGui::GetTime();
        float DeltaTime = CurrentTime - LastTime;

        FPS = 1.0f / DeltaTime;
        FrameTimeMS = DeltaTime * 1000.0f;

        ImGui::Text("%.2f FPS", FPS);
        ImGui::Text("%.2f ms", FrameTimeMS);
        ImGui::Text("\n");

        LastTime = CurrentTime;
    }

    if (ShowMemory)
    {
        ImGui::Text("Allocated Object Count: %llu", FPlatformMemory::GetAllocationCount<EAT_Object>());
        ImGui::Text("Allocated Object Memory: %llu B", FPlatformMemory::GetAllocationBytes<EAT_Object>());
        ImGui::Text("Allocated Container Count: %llu", FPlatformMemory::GetAllocationCount<EAT_Container>());
        ImGui::Text("Allocated Container memory: %llu B", FPlatformMemory::GetAllocationBytes<EAT_Container>());
    }

    if (ShowLight)
    {
        // @todo Find Better Way to Get Light Count
        int NumPointLights = 0;
        int NumSpotLights = 0;
        for (const auto iter : TObjectRange<ULightComponentBase>())
        {
            if (iter->GetWorld() == GEngine->ActiveWorld)
            {
                if (UPointLightComponent* PointLight = Cast<UPointLightComponent>(iter))
                {
                    NumPointLights++;
                }
                else if (USpotLightComponent* SpotLight = Cast<USpotLightComponent>(iter))
                {
                    NumSpotLights++;
                }
                /*
                else if (UDirectionalLightComponent* DirectionalLight = Cast<UDirectionalLightComponent>(iter))
                {
                    DirectionalLights.Add(DirectionalLight);
                }
                else if (UAmbientLightComponent* AmbientLight = Cast<UAmbientLightComponent>(iter))
                {
                    AmbientLights.Add(AmbientLight);
                }
                */
            }
        }
        ImGui::Text("[ Light Counters ]\n");
        ImGui::Text("Point Light: %d", NumPointLights);
        ImGui::Text("Spot Light: %d", NumSpotLights);
        ImGui::Text("\n");
    }

    ImGui::PopStyleColor();
    ImGui::End();
}

void FEngineProfiler::SetGPUTimingManager(FGPUTimingManager* InGPUTimingManager)
{
    GPUTimingManager = InGPUTimingManager;
}

void FEngineProfiler::Render(ID3D11DeviceContext* Context, UINT Width, UINT Height)
{
    if (!bShowWindow) return;

    // Example positioning: Top-left corner
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Engine Profiler", &bShowWindow))
    {
        ImGui::End();
        return;
    }

    if (ImGui::BeginTable("ProfilerTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Scope", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("CPU (ms)", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("GPU (ms)", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableHeadersRow();

        for (const auto& [DisplayName, CPUStatName, GPUStatName] : TrackedScopes)
        {
            const double CPUTimeMs = FProfilerStatsManager::GetCpuStatMs(CPUStatName);
            double GPUTimeMs = GPUTimingManager->GetElapsedTimeMs(TStatId(GPUStatName));

            FString CPUText = (CPUTimeMs >= 0.0) ? FString::Printf(TEXT("%.3f"), CPUTimeMs) : TEXT("---");
            FString GPUText;

            if (GPUTimeMs == -1.0) GPUText = TEXT("Disjoint");
            else if (GPUTimeMs == -2.0) GPUText = TEXT("Waiting");
            else if (GPUTimeMs < 0.0) GPUText = TEXT("---");
            else GPUText = FString::Printf(TEXT("%.3f"), GPUTimeMs);

            ImGui::TableNextRow();

            // Scope 열
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", *DisplayName);

            // CPU (ms) 열 - 우측 정렬
            ImGui::TableSetColumnIndex(1);
            float CPUTextWidth = ImGui::CalcTextSize(*CPUText).x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - CPUTextWidth);
            ImGui::TextUnformatted(*CPUText);

            // GPU (ms) 열 - 우측 정렬
            ImGui::TableSetColumnIndex(2);
            float GPUTextWidth = ImGui::CalcTextSize(*GPUText).x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - GPUTextWidth);
            ImGui::TextUnformatted(*GPUText);
        }

        ImGui::EndTable();
    }



    ImGui::End();
}

void FEngineProfiler::RegisterStatScope(const FString& DisplayName, const FName& CPUStatName, const FName& GPUStatName)
{
    TrackedScopes.Add({ DisplayName, CPUStatName, GPUStatName });
}

// 싱글톤 인스턴스 반환
Console& Console::GetInstance() {
    static Console Instance;
    return Instance;
}

// 로그 초기화
void Console::Clear() {
    Items.Empty();
}

// 로그 추가
void Console::AddLog(const LogLevel Level, const char* Format, ...) {
    char Buf[1024];
    va_list args;
    va_start(args, Format);
    vsnprintf(Buf, sizeof(Buf), Format, args);
    va_end(args);

    Items.Add({ Level, std::string(Buf) });
    ScrollToBottom = true;
}

// 콘솔 창 렌더링
void Console::Draw() {
    if (!bWasOpen)
    {
        return;
    }

    if (!ImGui::Begin("Console", &bWasOpen))
    {
        ImGui::End();
        return;
    }

    Overlay.RenderStatWidgets(); 

    static ImGuiTextFilter Filter; 

    if (ImGui::Button("Clear")) Clear();
    ImGui::SameLine();
    if (ImGui::Button("Copy")) ImGui::LogToClipboard();
    ImGui::SameLine();

    ImGui::Text("Filter:"); ImGui::SameLine(); Filter.Draw("##Filter", 100); ImGui::SameLine();
    ImGui::Checkbox("Display", &ShowLogTemp); ImGui::SameLine();
    ImGui::Checkbox("Warning", &ShowWarning); ImGui::SameLine();
    ImGui::Checkbox("Error", &ShowError);

    ImGui::Separator();

    ImGui::BeginChild("ScrollingRegion", ImVec2(0, -ImGui::GetTextLineHeightWithSpacing()), false, ImGuiWindowFlags_HorizontalScrollbar);
    for (const auto& [Level, Message] : Items)
    {
        if (!Filter.PassFilter(*Message)) continue;
        if ((Level == LogLevel::Display && !ShowLogTemp) ||
            (Level == LogLevel::Warning && !ShowWarning) ||
            (Level == LogLevel::Error && !ShowError)) continue;

        ImVec4 Color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        if (Level == LogLevel::Warning) Color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
        else if (Level == LogLevel::Error) Color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
        ImGui::TextColored(Color, "%s", *Message);
    }

    if (ScrollToBottom)
    {
        ImGui::SetScrollHereY(1.0f);
        ScrollToBottom = false;
    }
    ImGui::EndChild();

    ImGui::Separator();

    bool ReclaimFocus = false;
    if (ImGui::InputText("Input", InputBuf, IM_ARRAYSIZE(InputBuf), ImGuiInputTextFlags_EnterReturnsTrue))
    {
        if (InputBuf[0])
        {
            AddLog(LogLevel::Display, ">> %s", InputBuf);
            ExecuteCommand(std::string(InputBuf));
            History.Add(std::string(InputBuf));
            HistoryPos = -1;
            ScrollToBottom = true;
        }
        InputBuf[0] = '\0';
        ReclaimFocus = true;
    }

    if (ReclaimFocus) ImGui::SetKeyboardFocusHere(-1); // Reclaim focus

    ImGui::End();
}

void Console::ExecuteCommand(const std::string& Command)
{
    AddLog(LogLevel::Display, "Executing command: %s", Command.c_str());

    if (Command == "clear")
    {
        Clear();
    }
    else if (Command == "help")
    {
        AddLog(LogLevel::Display, "Available commands:");
        AddLog(LogLevel::Display, " - clear: Clears the console");
        AddLog(LogLevel::Display, " - help: Shows available commands");
        AddLog(LogLevel::Display, " - stat fps: Toggle FPS display");
        AddLog(LogLevel::Display, " - stat memory: Toggle Memory display");
        AddLog(LogLevel::Display, " - stat none: Hide all stat overlays");
    }
    else if (Command.starts_with("stat "))
    {
        Overlay.ToggleStat(Command);
    }
    else
    {
        AddLog(LogLevel::Error, "Unknown command: %s", Command.c_str());
    }
}

void Console::OnResize(HWND hWnd)
{
    RECT ClientRect;
    GetClientRect(hWnd, &ClientRect);
    Width = ClientRect.right - ClientRect.left;
    Height = ClientRect.bottom - ClientRect.top;
}

