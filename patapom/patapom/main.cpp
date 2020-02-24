#include <windows.h>
#include <wincodec.h>
#include <dinput.h>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx12.h"
#include "Level.h"
#include "Scene.h"
#include "Frame.h"
#include "Pass.h"
#include "Mesh.h"
#include "Texture.h"
#include "Camera.h"

HWND gHwnd = NULL; // Handle to the window
const LPCTSTR WindowName = L"WPWIV"; // name of the window (not the title)
const LPCTSTR WindowTitle = L"WPWIV_1.0"; // title of the window
IDirectInputDevice8* gDIKeyboard;
IDirectInputDevice8* gDIMouse;
DIMOUSESTATE gMouseLastState;
BYTE gKeyboardLastState[256];
bool gMouseAcquired = false;
LPDIRECTINPUT8 gDirectInput;
bool gFullScreen = false; // is window full screen?
bool gRunning = true; // we will exit the program when this becomes false
int gUpdateCamera = 0;
int gUpdateSettings = 0;

const int gWidth = 1024;
const int gHeight = 768;
const int gWidthDeferred = 1024;
const int gHeightDeferred = 1024;
const int gFrameCount = 3;
const int gMultiSampleCount = 1;

Renderer gRenderer;
Level gLevelDefault("default level");
Scene gSceneDefault("default scene");
Pass gPassDefault(L"default pass");
Pass gPassDeferred(L"deferred pass");
OrbitCamera gMainCamera(4.f, 0.f, 0.f, XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), gWidthDeferred, gHeightDeferred, 45.0f, 0.1f, 1000.0f);
Camera gDummyCamera(XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), gWidth, gHeight, 45.0f, 0.1f, 1000.0f);
Mesh gQuad(Mesh::MeshType::FULLSCREEN_QUAD, XMFLOAT3(0, 0, 0), XMFLOAT3(0, 0, 0), XMFLOAT3(1, 1, 1));
Shader gStandardVS(Shader::ShaderType::VERTEX_SHADER, L"vs.hlsl");
Shader gStandardPS(Shader::ShaderType::PIXEL_SHADER, L"ps.hlsl");
Shader gDeferredVS(Shader::ShaderType::VERTEX_SHADER, L"vs_deferred.hlsl");
Shader gDeferredPS(Shader::ShaderType::PIXEL_SHADER, L"ps_deferred_ms.hlsl");//using multisampled srv
Sampler gSampler = {
	Sampler::Filter::LINEAR,
	Sampler::Filter::LINEAR,
	Sampler::Filter::LINEAR,
	Sampler::AddressMode::WRAP,
	Sampler::AddressMode::WRAP,
	Sampler::AddressMode::WRAP,
	0.f,
	false,
	1.f,
	false,
	CompareOp::NEVER,
	0.f,
	1.f,
	{0.f, 0.f, 0.f, 0.f}
};
Texture gTextureAlbedo("foam.jpg", L"albedo", gSampler, true, Format::R8G8B8A8_UNORM);
RenderTexture gRenderTexture(L"rt0", gWidthDeferred, gHeightDeferred, RenderTexture::ReadFrom::COLOR, gSampler, Format::R8G8B8A8_UNORM, 2);

//imgui stuff
ID3D12DescriptorHeap* g_pd3dSrvDescHeap = NULL;
bool show_demo_window = true;
bool show_another_window = false;
ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

void CreateLevel()
{
	// A. Mesh
	// mesh.AddTexture(...);

	// B. Pass
	gPassDefault.SetCamera(&gMainCamera);
	gPassDefault.AddMesh(&gQuad);
	gPassDefault.AddShader(&gStandardVS);
	gPassDefault.AddShader(&gStandardPS);
	gPassDefault.AddTexture(&gTextureAlbedo);
	gPassDefault.AddRenderTexture(&gRenderTexture);

	gPassDeferred.SetCamera(&gDummyCamera);
	gPassDeferred.AddMesh(&gQuad);
	gPassDeferred.AddShader(&gDeferredVS);
	gPassDeferred.AddShader(&gDeferredPS);
	gPassDeferred.AddTexture(&gRenderTexture);
	
	// C. Frame
	gRenderer.mFrames.resize(gFrameCount);
	// renderer.mFrames[i].AddTexture(...);

	// D. Scene
	gSceneDefault.AddPass(&gPassDefault);
	gSceneDefault.AddPass(&gPassDeferred);
	// scene.AddTexture(...);

	// E. Level
	gLevelDefault.AddCamera(&gMainCamera); // camera
	gLevelDefault.AddCamera(&gDummyCamera);
	gLevelDefault.AddScene(&gSceneDefault); // scene
	gLevelDefault.AddPass(&gPassDefault); // pass
	gLevelDefault.AddPass(&gPassDeferred);
	gLevelDefault.AddMesh(&gQuad); // mesh
	gLevelDefault.AddShader(&gStandardVS); // vertex shader
	gLevelDefault.AddShader(&gDeferredVS);
	gLevelDefault.AddShader(&gStandardPS); // pixel shader
	gLevelDefault.AddShader(&gDeferredPS);
	gLevelDefault.AddTexture(&gTextureAlbedo); // texture
	gLevelDefault.AddTexture(&gRenderTexture);

	// F. Renderer
	gRenderer.SetLevel(&gLevelDefault);
}

void InitConsole()
{
	AllocConsole();
	AttachConsole(GetCurrentProcessId());
	FILE* stream;
	freopen_s(&stream, "CON", "w", stdout);
}

bool InitDirectInput(HINSTANCE hInstance)
{
	HRESULT hr;

	hr = DirectInput8Create(hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&gDirectInput, NULL);
	if (CheckError(hr, nullptr)) return false;

	hr = gDirectInput->CreateDevice(GUID_SysKeyboard, &gDIKeyboard, NULL);
	if (CheckError(hr, nullptr)) return false;

	hr = gDirectInput->CreateDevice(GUID_SysMouse, &gDIMouse, NULL);
	if (CheckError(hr, nullptr)) return false;

	hr = gDIKeyboard->SetDataFormat(&c_dfDIKeyboard);
	if (CheckError(hr, nullptr)) return false;

	hr = gDIKeyboard->SetCooperativeLevel(gHwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
	if (CheckError(hr, nullptr)) return false;

	hr = gDIMouse->SetDataFormat(&c_dfDIMouse);
	if (CheckError(hr, nullptr)) return false;

	hr = gDIMouse->SetCooperativeLevel(gHwnd, DISCL_EXCLUSIVE | DISCL_NOWINKEY | DISCL_FOREGROUND);
	if (CheckError(hr, nullptr)) return false;

	return true;
}

void DetectInput()
{
	BYTE keyboardCurrState[256];
	memset(keyboardCurrState, 0, sizeof(keyboardCurrState));//initialization is important, sometimes the value of unpressed key will not be changed

	gDIKeyboard->Acquire();

	gDIKeyboard->GetDeviceState(sizeof(keyboardCurrState), (LPVOID)&keyboardCurrState);

	POINT currentCursorPos = {};
	GetCursorPos(&currentCursorPos);
	ScreenToClient(gHwnd, &currentCursorPos);

	int mouseX = currentCursorPos.x;
	int mouseY = currentCursorPos.y;

	//keyboard control
	//this is handled in mainloop, no need to do this here again
	//if (KEYDOWN(keyboardCurrState, DIK_ESCAPE))
	//{
	//	PostMessage(hwnd, WM_DESTROY, 0, 0);
	//}

	//mouse control
	if (KEYDOWN(keyboardCurrState, DIK_C))//control camera
	{
		if (!gMouseAcquired)
		{
			gDIMouse->Acquire();
			gMouseAcquired = true;
		}
	}
	else
	{
		gDIMouse->Unacquire();
		gMouseAcquired = false;
	}

	if (gMouseAcquired)
	{
		DIMOUSESTATE mouseCurrState;
		gDIMouse->GetDeviceState(sizeof(DIMOUSESTATE), &mouseCurrState);
		if (mouseCurrState.lX != 0)
		{
			gMainCamera.SetHorizontalAngle(gMainCamera.GetHorizontalAngle() + mouseCurrState.lX * 0.1);
		}
		if (mouseCurrState.lY != 0)
		{
			float tempVerticalAngle = gMainCamera.GetVerticalAngle() + mouseCurrState.lY * 0.1;
			if (tempVerticalAngle > 90 - EPSILON) tempVerticalAngle = 89 - EPSILON;
			if (tempVerticalAngle < -90 + EPSILON) tempVerticalAngle = -89 + EPSILON;
			gMainCamera.SetVerticalAngle(tempVerticalAngle);
		}
		if (mouseCurrState.lZ != 0)
		{
			float tempDistance = gMainCamera.GetDistance() - mouseCurrState.lZ * 0.01;
			if (tempDistance < 0 + EPSILON) tempDistance = 0.1 + EPSILON;
			gMainCamera.SetDistance(tempDistance);
		}
		gMouseLastState = mouseCurrState;
		gMainCamera.Update();
		gUpdateCamera = gRenderer.mFrameCount;
	}
	
	memcpy(gKeyboardLastState, keyboardCurrState, sizeof(gKeyboardLastState));
}

bool InitImgui()
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.NumDescriptors = 1;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	if (gRenderer.mDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dSrvDescHeap)) != S_OK)
		return false;

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls

	// Setup Platform/Renderer bindings
	ImGui_ImplWin32_Init(gHwnd);
	ImGui_ImplDX12_Init(gRenderer.mDevice,
		gRenderer.mFrameCount,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		g_pd3dSrvDescHeap->GetCPUDescriptorHandleForHeapStart(),
		g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart());

	// Setup Style
	// ImGui::StyleColorsDark();
	ImGui::StyleColorsClassic();

	// Load Fonts
	// - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them. 
	// - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple. 
	// - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
	// - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
	// - Read 'misc/fonts/README.txt' for more instructions and details.
	// - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
	//io.Fonts->AddFontDefault();
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
	//ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
	//IM_ASSERT(font != NULL);
	return true;
}

void Update()
{
	// game logic updates
	if (gUpdateCamera > 0)
	{
		gPassDefault.UpdateUniformBuffer(gRenderer.mCurrentFrameIndex, &gMainCamera);
		gUpdateCamera--;
	}

	if (gUpdateSettings > 0)
	{
		gSceneDefault.UpdateUniformBuffer(gRenderer.mCurrentFrameIndex);
		gUpdateSettings--;
	}
}

void RecordCommands()
{
	if (!gRenderer.WaitForFrame(gRenderer.mCurrentFrameIndex))
		debugbreak(gRunning = false);

	if (!gRenderer.RecordBegin(gRenderer.mCurrentFrameIndex, gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex]))
		debugbreak(gRunning = false);

	gRenderTexture.TransitionColorBufferLayout(gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex], TextureLayout::RENDER_TARGET);

	gRenderer.RecordPass(gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex], gPassDefault, true, false, false, gRenderTexture.mColorClearValue);

	gRenderTexture.TransitionColorBufferLayout(gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex], TextureLayout::SHADER_READ);

	gRenderer.RecordPass(gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex], gPassDeferred);

	///////// IMGUI PIPELINE /////////
	//vvvvvvvvvvvvvvvvvvvvvvvvvvvvvv//
	gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex]->SetDescriptorHeaps(1, &g_pd3dSrvDescHeap);
	ImGui::Render();
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex]);
	///////// IMGUI PIPELINE /////////
	//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^//

	if (!gRenderer.RecordEnd(gRenderer.mCurrentFrameIndex, gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex]))
		debugbreak(gRunning = false);
}

void SubmitCommands()
{
	if (!gRenderer.SubmitCommands(gRenderer.mCurrentFrameIndex, gRenderer.mGraphicsCommandQueue, { gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex] }))
		debugbreak(gRunning = false);
}

void Present()
{
	if (!gRenderer.Present())
		debugbreak(gRunning = false);
}

void Draw()
{
	gRenderer.mFrameCountTotal++;
	RecordCommands();
	SubmitCommands();
	Present();
}

void UpdateUI()
{
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	static int mode = gSceneDefault.mSceneUniform.sceneMode;
	bool needToUpdateSceneUniform = false;

	ImGui::SetNextWindowPos(ImVec2(0, 0));

	ImGui::Begin("Control Panel ");

	if (ImGui::Combo("mode", &mode, "default\0debug"))
	{
		gSceneDefault.mSceneUniform.sceneMode = mode;
		needToUpdateSceneUniform = true;
	}

	ImGui::Text("%.3f ms/frame (%.1f FPS) ", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	ImGui::Text("Hold C and use mouse to rotate camera.");
	ImGui::End();

	if (needToUpdateSceneUniform)
		gUpdateSettings = gRenderer.mFrameCount;
}

void Cleanup()
{
	displayf("clean up");
	gRenderer.WaitAllFrames();
	gRenderer.Release();

	//imgui stuff
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	SAFE_RELEASE(g_pd3dSrvDescHeap);

	//direct input stuff
	gDIKeyboard->Unacquire();
	gDIMouse->Unacquire();
	gDirectInput->Release();
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProc(HWND hwnd,	UINT msg, WPARAM wParam, LPARAM lParam)
{
	//vv imgui vv//
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
		return true;
	//^^ imgui ^^//

	switch (msg)
	{
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE) {
			if (MessageBox(0, L"Are you sure you want to exit?",
				L"Really?", MB_YESNO | MB_ICONQUESTION) == IDYES)
			{
				gRunning = false;
				DestroyWindow(hwnd);
			}
		}
		return 0;

	case WM_DESTROY: // x button on top right corner of window was pressed
		gRunning = false;
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd,
		msg,
		wParam,
		lParam);
}

// create and show the window
bool InitWindow(HINSTANCE hInstance, int width, int height, int ShowWnd, bool fullscreen)
{
	if (fullscreen)
	{
		HMONITOR hmon = MonitorFromWindow(gHwnd, MONITOR_DEFAULTTONEAREST);
		MONITORINFO mi = { sizeof(mi) };
		GetMonitorInfo(hmon, &mi);

		width = mi.rcMonitor.right - mi.rcMonitor.left;
		height = mi.rcMonitor.bottom - mi.rcMonitor.top;
	}

	WNDCLASSEX wc;

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = NULL;
	wc.cbWndExtra = NULL;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 2);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = WindowName;
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	if (!RegisterClassEx(&wc))
	{
		MessageBox(NULL, L"Error registering class",
			L"Error", MB_OK | MB_ICONERROR);
		return false;
	}

	gHwnd = CreateWindowEx(NULL,
		WindowName,
		WindowTitle,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		width, height,
		NULL,
		NULL,
		hInstance,
		NULL);

	if (!gHwnd)
	{
		MessageBox(NULL, L"Error creating window",
			L"Error", MB_OK | MB_ICONERROR);
		return false;
	}

	if (fullscreen)
	{
		SetWindowLong(gHwnd, GWL_STYLE, 0);
	}

	ShowWindow(gHwnd, ShowWnd);
	UpdateWindow(gHwnd);

	return true;
}

void MainLoop() 
{
	MSG msg;
	ZeroMemory(&msg, sizeof(MSG));

	while (gRunning)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
				break;

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		
		{
			UpdateUI();
			DetectInput();
			Update();
			Draw();
		}
	}
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	InitConsole();

	// create the window
	if (!InitWindow(hInstance, gWidth, gHeight, nShowCmd, gFullScreen))
	{
		MessageBox(0, L"Window Initialization - Failed", L"Error", MB_OK);
		return 1;
	}

	// initialize input device
	if (!InitDirectInput(hInstance))
	{
		MessageBox(0, L"Failed to initialize input", L"Error", MB_OK);
		return 1;
	}
	
	// use declared data to create the level
	CreateLevel();

	// initialize renderer
	if (!gRenderer.InitRenderer(
		gHwnd, 
		gFrameCount, 
		gMultiSampleCount, 
		gWidth, 
		gHeight,
		DebugMode::GBV))
	{
		MessageBox(0, L"Failed to initialize renderer", L"Error", MB_OK);
		Cleanup();
		return 1;
	}

	if (!InitImgui())
	{
		MessageBox(0, L"Failed to initialize imgui", L"Error", MB_OK);
		Cleanup();
		return 1;
	}

	// start the main loop
	MainLoop();

	// clean up everything
	Cleanup();

	return 0;
}

int GetUniformSlot(UNIFORM_REGISTER_SPACE space, UNIFORM_TYPE type)
{
	// constant buffer is root parameter, texture and sampler are stored in table
	//                 b0 constant | t0 texture | s0 sampler
	// space 0 scene   0           | 4          | 8
	// space 1 frame   1           | 5          | 9
	// space 2 pass    2           | 6          | 10
	// space 3 object  3           | 7          | 11

	return (int)space + (int)type * (int)UNIFORM_REGISTER_SPACE::COUNT;
}

// return true if it is error
bool CheckError(HRESULT hr, ID3D12Device* device, ID3D10Blob* error_message)
{
	if (FAILED(hr))
	{
		displayf("FAILED: 0x%x", hr);
		if (error_message != nullptr)
			displayf("return value: %d, error message: %s", hr, (char*)error_message->GetBufferPointer());
		if (hr == 0x887A0005 && device != nullptr)
			displayf("device removed reason: 0x%x", device->GetDeviceRemovedReason());
		fatalf("check error failed!");
		return true;
	}
	return false;
}
