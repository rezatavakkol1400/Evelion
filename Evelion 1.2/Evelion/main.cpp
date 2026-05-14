#include "xorstr.hpp"
#include "Functions.h"
#include "memory.h"
#include "struct.h"
#include "w2s.h"

#include <TlHelp32.h>
#include <thread>
#include <string>
#include <iostream>
#include <cmath>
#include <math.h>
#include <vector>
#include <cstring>

// ==========================================
// Global Variables Definition
// ==========================================
std::uintptr_t hw = 0;
std::uintptr_t client = 0;

size_t viewMatrixSize = 0x40;
void* viewMatrixBuffer = nullptr;

size_t entityListSize = 0x940C;
void* entityListBuffer = nullptr;

HWND hwnd1 = NULL;
int id = 0;

Memory* memory_ptr = nullptr;

// ==========================================
// Core Bypass Functions
// ==========================================
std::uintptr_t FindHiddenBase(const char* hexPattern, const char* logName) {
    if (!memory_ptr) return 0;
    
    std::uintptr_t ptr = memory_ptr->PatternScan(hexPattern, logName);
    
    if (!ptr) {
        MessageBoxA(nullptr, (std::string("Failed to find pattern for: ") + logName + "\nCheck Evelion_DeepDebug.log").c_str(), "Evelion Bypass Error", MB_OK | MB_ICONERROR);
        exit(0);
    }
    
    std::uintptr_t current = ptr & ~0xFFF;
    for (int i = 0; i < 20000; i++) { 
        if (current < 0x10000) break;
        
        short magic = memory_ptr->Read<short>(current);
        if (magic == 0x5A4D) { 
            return current;
        }
        current -= 0x1000; 
    }
    
    MessageBoxA(nullptr, (std::string("Failed to find MZ Header for: ") + logName + "\nCheck Evelion_DeepDebug.log").c_str(), "Evelion Bypass Error", MB_OK | MB_ICONERROR);
    exit(0);
}

void InitCheat() {
    memory_ptr = new Memory("hl.exe");
    
    viewMatrixBuffer = malloc(viewMatrixSize);
    entityListBuffer = malloc(entityListSize);
    
    hw = FindHiddenBase("63 6C 5F 65 6E 74 69 74 79 6C 69 73 74 00", "hw_hidden");
    client = FindHiddenBase("54 65 61 6D 5F 54 65 72 72 6F 72 69 73 74 00", "client_hidden");
    
    hwnd1 = GetProcessHwnd();
    id = GetWindowThreadProcessId(hwnd1, &Game::PID);
}

// ==========================================
// Cheat Logic & ESP Threads
// ==========================================
void MatrixUpdate() {
    while (1) {
        while (esp) {
            if (hw && memory_ptr) {
                memory_ptr->ReadHugeMemory(hw + 0xEC9780, viewMatrixBuffer, viewMatrixSize);
                memcpy(gWorldToScreen, viewMatrixBuffer, sizeof(gWorldToScreen));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void OffsetsUpdate() {
    float screenPositionTemp;
    float stateTemp = 9999;

    while (1) {
        while (esp) {
            if (hw && client && memory_ptr) {
                memory_ptr->ReadHugeMemory(hw + 0x12043CC, entityListBuffer, entityListSize);
                for (int i = 0; i < 64; i++)
                {
                    float playerX = memory_ptr->ReadModuleBuffer<float>(reinterpret_cast<std::uintptr_t>(entityListBuffer) + i * 0x0250 + 0x0184);

                    if (!playerX) continue;

                    float playerY = memory_ptr->ReadModuleBuffer<float>(reinterpret_cast<std::uintptr_t>(entityListBuffer) + i * 0x0250 + 0x0188);
                    float playerZ = memory_ptr->ReadModuleBuffer<float>(reinterpret_cast<std::uintptr_t>(entityListBuffer) + i * 0x0250 + 0x018C);

                    Vector3 TargetPos = { playerX , playerY , playerZ };
                    WorldToScreen(TargetPos, screenPositionTemp);

                    if (def_models) {
                        uintptr_t modelAddress = i * 0x0250 + 0x012C;
                        std::string model;
                        char ch2;

                        do {
                            ch2 = memory_ptr->ReadModuleBuffer<char>(reinterpret_cast<std::uintptr_t>(entityListBuffer) + modelAddress);
                            model.push_back(ch2);
                            ++modelAddress;
                        } while (ch2 != '\0');

                        int team = memory_ptr->Read<int>(client + 0x100DF4);

                        if ((team == 2 && (model.find("urban") != std::string::npos ||
                            model.find("gign") != std::string::npos ||
                            model.find("gsg9") != std::string::npos ||
                            model.find("sas") != std::string::npos)) ||
                            (team == 1 && (model.find("terror") != std::string::npos ||
                                model.find("leet") != std::string::npos ||
                                model.find("arctic") != std::string::npos ||
                                model.find("guerilla") != std::string::npos))) {
                            
                            players[i] = { {0, 0}, 0 };
                            continue;
                        }
                    }

                    players[i].screenPosition = screenPositionTemp; 
                    players[i].screenPosition = screenPositionTemp;

                    if (enemy_name) {
                        std::string name;
                        uintptr_t nameAddress = i * 0x0250 + 0x0100;
                        char ch1;

                        do {
                            ch1 = memory_ptr->ReadModuleBuffer<char>(reinterpret_cast<std::uintptr_t>(entityListBuffer) + nameAddress);
                            name.push_back(ch1);
                            ++nameAddress;
                        } while (ch1 != '\0');
                        
                        players[i].name = name;
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void DeadCheck() {
    float stateTemp = 9999;
    float playerX;

    while (1) {
        while (esp) {
            if (memory_ptr) {
                for (int i = 0; i < 64; i++)
                {
                    playerX = memory_ptr->ReadModuleBuffer<float>(reinterpret_cast<std::uintptr_t>(entityListBuffer) + i * 0x0250 + 0x0184);
                    if (!playerX) continue;

                    stateTemp = memory_ptr->ReadModuleBuffer<float>(reinterpret_cast<std::uintptr_t>(entityListBuffer) + i * 0x0250 + 0x017C + 0x1);
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));

                    if (stateTemp == players[i].state) {
                        if (!players[i].dead) {
                            players[i].screenPosition = 0;
                            players[i].screenPosition = 0;
                            players[i].dead = true;
                        }
                        players[i].state = stateTemp;
                        continue;
                    }
                    else {
                        players[i].dead = false;
                        players[i].state = stateTemp;
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void LobbyCheck() {
    while (1) {
        while (esp) {
            if (hw && memory_ptr) {
                int lobby = memory_ptr->Read<int>(hw + 0x105CFC8);

                if (!lobby && !in_lobby) {
                    memset(players, 0, sizeof(players));
                    in_lobby = true;
                }
                else if (lobby) {
                    in_lobby = false;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}

void Draw() {
    if (esp) {
        if (enemy_box || enemy_name) {
            for (int i = 0; i < 64; i++)
            {
                int x = players[i].screenPosition;
                int y = players[i].screenPosition;

                if (players[i].dead || y < 5 || x < 5) continue;

                if (enemy_box) DrawCircleFilled(x, y, 2.5, &BoxColor);
                if (enemy_name) DrawNewText(x - 14, y - 32, &NameColor, players[i].name.c_str());
            }
        }
    }
}

void DrawMenu() {
    ImGui::SetNextWindowSize(ImVec2(400.f, 200.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::Begin("Evelion Bypass", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    ImGui::Checkbox("Esp", &esp);
    ImGui::Separator();
    ImGui::Checkbox("Box", &enemy_box);
    ImGui::SameLine();
    ImGui::Checkbox("Name", &enemy_name);

    ImGui::Spacing(), ImGui::Spacing(), ImGui::Spacing(), ImGui::Spacing();

    ImGui::Checkbox("Show Only Enemies", &def_models);
    ImGui::SameLine();
    ImGui::Text("(only works with default models)");

    if (ImGui::Button("Change Box Color")) {
        ImGui::OpenPopup("Box Color");
    }
    ImGui::SameLine();
    if (ImGui::Button("Change Name Color")) {
        ImGui::OpenPopup("Name Color");
    }

    if (ImGui::BeginPopup("Box Color")) {
        ImGui::PushItemWidth(100);
        ImGui::ColorPicker3("Box", boxTemp);
        BoxColor.R = static_cast<int>(boxTemp * 255);
        BoxColor.G = static_cast<int>(boxTemp * 255);
        BoxColor.B = static_cast<int>(boxTemp * 255);
        BoxColor.A = 255;
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("Name Color")) {
        ImGui::PushItemWidth(100);
        ImGui::ColorPicker3("Name", nameTemp);
        NameColor.R = static_cast<int>(nameTemp * 255);
        NameColor.G = static_cast<int>(nameTemp * 255);
        NameColor.B = static_cast<int>(nameTemp * 255);
        NameColor.A = 255;
        ImGui::EndPopup();
    }

    ImGui::Spacing();
    ImGui::Separator();
    if (ImGui::Button("Unhook")) {
        Unhook = true;
    }

    ImGui::Text("Evelion Base - Bypassed Version");
    ImGui::End();
}

void Render() {
    if (GetAsyncKeyState(VK_DELETE) & 1) Unhook = true;
    if (GetAsyncKeyState(VK_INSERT) & 1) ShowMenu = !ShowMenu;

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    Draw();

    if (ShowMenu)
        DrawMenu();

    ImGui::EndFrame();

    DirectX9Interface::pDevice->SetRenderState(D3DRS_ZENABLE, false);
    DirectX9Interface::pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, false);
    DirectX9Interface::pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, false);

    DirectX9Interface::pDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
    if (DirectX9Interface::pDevice->BeginScene() >= 0) {
        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
        DirectX9Interface::pDevice->EndScene();
    }

    HRESULT result = DirectX9Interface::pDevice->Present(NULL, NULL, NULL, NULL);
    if (result == D3DERR_DEVICELOST && DirectX9Interface::pDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET) {
        ImGui_ImplDX9_InvalidateDeviceObjects();
        DirectX9Interface::pDevice->Reset(&DirectX9Interface::pParams);
        ImGui_ImplDX9_CreateDeviceObjects();
    }
}

void MainLoop() {
    static RECT OldRect;
    ZeroMemory(&DirectX9Interface::Message, sizeof(MSG));

    while (DirectX9Interface::Message.message != WM_QUIT) {
        if (Unhook) {
            ImGui_ImplDX9_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            DestroyWindow(OverlayWindow::Hwnd);
            UnregisterClass(OverlayWindow::WindowClass.lpszClassName, OverlayWindow::WindowClass.hInstance);
            exit(0);
        }

        if (PeekMessage(&DirectX9Interface::Message, OverlayWindow::Hwnd, 0, 0, PM_REMOVE)) {
            TranslateMessage(&DirectX9Interface::Message);
            DispatchMessage(&DirectX9Interface::Message);
        }
        HWND ForegroundWindow = GetForegroundWindow();
        if (ForegroundWindow == Process::Hwnd) {
            HWND TempProcessHwnd = GetWindow(ForegroundWindow, GW_HWNDPREV);
            SetWindowPos(OverlayWindow::Hwnd, TempProcessHwnd, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        }

        RECT TempRect;
        POINT TempPoint;
        ZeroMemory(&TempRect, sizeof(RECT));
        ZeroMemory(&TempPoint, sizeof(POINT));

        GetClientRect(Process::Hwnd, &TempRect);
        ClientToScreen(Process::Hwnd, &TempPoint);

        TempRect.left = TempPoint.x;
        TempRect.top = TempPoint.y;
        ImGuiIO& io = ImGui::GetIO();
        io.ImeWindowHandle = Process::Hwnd;

        POINT TempPoint2;
        GetCursorPos(&TempPoint2);
        io.MousePos.x = TempPoint2.x - TempPoint.x;
        io.MousePos.y = TempPoint2.y - TempPoint.y;

        if (GetAsyncKeyState(0x1)) {
            io.MouseDown = true;
            io.MouseClicked = true;
            io.MouseClickedPos.x = io.MousePos.x;
            io.MouseClickedPos.y = io.MousePos.y;
        }
        else {
            io.MouseDown = false;
        }

        if (TempRect.left != OldRect.left || TempRect.right != OldRect.right || TempRect.top != OldRect.top || TempRect.bottom != OldRect.bottom) {
            OldRect = TempRect;
            Process::WindowWidth = TempRect.right;
            Process::WindowHeight = TempRect.bottom;
            DirectX9Interface::pParams.BackBufferWidth = Process::WindowWidth;
            DirectX9Interface::pParams.BackBufferHeight = Process::WindowHeight;
            SetWindowPos(OverlayWindow::Hwnd, (HWND)0, TempPoint.x, TempPoint.y, Process::WindowWidth, Process::WindowHeight, SWP_NOREDRAW);
            DirectX9Interface::pDevice->Reset(&DirectX9Interface::pParams);
        }
        Render();
    }
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (DirectX9Interface::pDevice != NULL) {
        DirectX9Interface::pDevice->EndScene();
        DirectX9Interface::pDevice->Release();
    }
    if (DirectX9Interface::Direct3D9 != NULL) {
        DirectX9Interface::Direct3D9->Release();
    }
    DestroyWindow(OverlayWindow::Hwnd);
    UnregisterClass(OverlayWindow::WindowClass.lpszClassName, OverlayWindow::WindowClass.hInstance);
}

bool DirectXInit() {
    if (FAILED(Direct3DCreate9Ex(D3D_SDK_VERSION, &DirectX9Interface::Direct3D9))) {
        return false;
    }

    D3DPRESENT_PARAMETERS Params = { 0 };
    Params.Windowed = TRUE;
    Params.SwapEffect = D3DSWAPEFFECT_DISCARD;
    Params.hDeviceWindow = OverlayWindow::Hwnd;
    Params.MultiSampleQuality = D3DMULTISAMPLE_NONE;
    Params.BackBufferFormat = D3DFMT_A8R8G8B8;
    Params.BackBufferWidth = Process::WindowWidth;
    Params.BackBufferHeight = Process::WindowHeight;
    Params.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    Params.EnableAutoDepthStencil = TRUE;
    Params.AutoDepthStencilFormat = D3DFMT_D16;
    Params.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;

    if (FAILED(DirectX9Interface::Direct3D9->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, OverlayWindow::Hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &Params, 0, &DirectX9Interface::pDevice))) {
        DirectX9Interface::Direct3D9->Release();
        return false;
    }

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantTextInput || ImGui::GetIO().WantCaptureKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.Fonts->AddFontDefault();

    ImGui_ImplWin32_Init(OverlayWindow::Hwnd);
    ImGui_ImplDX9_Init(DirectX9Interface::pDevice);
    DirectX9Interface::Direct3D9->Release();
    return true;
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WinProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, Message, wParam, lParam))
        return true;

    switch (Message) {
    case WM_DESTROY:
        if (DirectX9Interface::pDevice != NULL) {
            DirectX9Interface::pDevice->EndScene();
            DirectX9Interface::pDevice->Release();
        }
        if (DirectX9Interface::Direct3D9 != NULL) {
            DirectX9Interface::Direct3D9->Release();
        }
        PostQuitMessage(0);
        exit(4);
        break;
    case WM_SIZE:
        if (DirectX9Interface::pDevice != NULL && wParam != SIZE_MINIMIZED) {
            ImGui_ImplDX9_InvalidateDeviceObjects();
            DirectX9Interface::pParams.BackBufferWidth = LOWORD(lParam);
            DirectX9Interface::pParams.BackBufferHeight = HIWORD(lParam);
            ImGui_ImplDX9_CreateDeviceObjects();
        }
        break;
    default:
        return DefWindowProc(hWnd, Message, wParam, lParam);
        break;
    }
    return 0;
}

void SetupWindow() {
    OverlayWindow::WindowClass = {
        sizeof(WNDCLASSEX), 0, WinProc, 0, 0, nullptr, LoadIcon(nullptr, IDI_APPLICATION), LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr, OverlayWindow::Name, LoadIcon(nullptr, IDI_APPLICATION)
    };
    
    RegisterClassEx(&OverlayWindow::WindowClass);
    if (Process::Hwnd){
        static RECT TempRect = { NULL };
        static POINT TempPoint;
        GetClientRect(Process::Hwnd, &TempRect);
        ClientToScreen(Process::Hwnd, &TempPoint);
        TempRect.left = TempPoint.x;
        TempRect.top = TempPoint.y;
        Process::WindowWidth = TempRect.right;
        Process::WindowHeight = TempRect.bottom;
    }

    OverlayWindow::Hwnd = CreateWindowEx(NULL, OverlayWindow::Name, OverlayWindow::Name, WS_POPUP | WS_VISIBLE, Process::WindowLeft, Process::WindowTop, Process::WindowWidth, Process::WindowHeight, NULL, NULL, 0, NULL);
    DwmExtendFrameIntoClientArea(OverlayWindow::Hwnd, &DirectX9Interface::Margin);
    SetWindowLong(OverlayWindow::Hwnd, GWL_EXSTYLE, WS_EX_LAYERED | WS_EX_TRANSPARENT);
    ShowWindow(OverlayWindow::Hwnd, SW_SHOW);
    UpdateWindow(OverlayWindow::Hwnd);
}

void GetWindowRect() {
    RECT clientRect;
    if (GetClientRect(hwnd1, &clientRect)) {
        int clientWidth = clientRect.right - clientRect.left;
        int clientHeight = clientRect.bottom - clientRect.top;

        width = clientWidth;
        height = clientHeight;
    }
    else {
        MessageBox(nullptr, "Failed to get window rect.", "Evelion", MB_OK);
        exit(0);
    }
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    bool WindowFocus = false;
    setlocale(LC_ALL, "Russian"); 

    if (!IsProcessAlive("hl.exe")) {
        MessageBox(nullptr, "Please run game before running Evelion!", "Evelion", MB_OK);
        exit(0);
    }

    InitCheat();

    GetWindowRect();

    std::thread aliveThread(ProcessAlive);
    std::thread matrixThread(MatrixUpdate);
    std::thread updateThread(OffsetsUpdate);
    std::thread deadThread(DeadCheck);
    std::thread lobbyThread(LobbyCheck);

    while (WindowFocus == false) {
        DWORD ForegroundWindowProcessID;
        GetWindowThreadProcessId(GetForegroundWindow(), &ForegroundWindowProcessID);
        if (Game::PID == ForegroundWindowProcessID) {
            Process::ID = Game::PID;
            Process::Hwnd = hwnd1;

            RECT TempRect;
            GetWindowRect(Process::Hwnd, &TempRect);
            Process::WindowWidth = TempRect.right - TempRect.left;
            Process::WindowHeight = TempRect.bottom - TempRect.top;
            Process::WindowLeft = TempRect.left;
            Process::WindowRight = TempRect.right;
            Process::WindowTop = TempRect.top;
            Process::WindowBottom = TempRect.bottom;

            char TempTitle[MAX_PATH];
            GetWindowText(Process::Hwnd, TempTitle, sizeof(TempTitle));
            Process::Title = TempTitle;

            char TempClassName[MAX_PATH];
            GetClassName(Process::Hwnd, TempClassName, sizeof(TempClassName));
            Process::ClassName = TempClassName;

            char TempPath[MAX_PATH];
            GetModuleFileNameEx(Process::Handle, NULL, TempPath, sizeof(TempPath));
            Process::Path = TempPath;

            WindowFocus = true;
        }
    }
    
    std::string s = RandomString(10);
    OverlayWindow::Name = s.c_str();
    SetupWindow();
    DirectXInit();
    while (TRUE) {
        MainLoop();
    }
}
