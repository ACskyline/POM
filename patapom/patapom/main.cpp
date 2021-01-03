#include <windows.h>
#include <wincodec.h>
#include <dinput.h>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx12.h"
#include "Store.h"
#include "Scene.h"
#include "Frame.h"
#include "Pass.h"
#include "Mesh.h"
#include "Texture.h"
#include "Camera.h"
#include "Light.h"
#include "Renderer.h"
#include "ImageBasedLighting.h"
#include "PathTracer.h"

HWND gHwnd = NULL; // Handle to the window
const LPCTSTR WindowName = L"POM"; // name of the window (not the title)
const LPCTSTR WindowTitle = L"POM_1.0"; // title of the window
IDirectInputDevice8* gDIKeyboard;
IDirectInputDevice8* gDIMouse;
DIMOUSESTATE gMouseLastState;
POINT gMouseLastCursorPos;
BYTE gKeyboardLastState[256];
bool gMouseAcquired = false;
LPDIRECTINPUT8 gDirectInput;
bool gFullScreen = false; // is window full screen?
bool gRunning = true; // we will exit the program when this becomes false
int gUpdateCamera = 0;
int gUpdateSettings = 0;
int gPathTracerMode = 0;
const int gWidthDeferred = 960;
const int gHeightDeferred = 960;
const int gWidthShadow = 960;
const int gHeightShadow = 960;
const int gFrameCount = 3;
const int gMultiSampleCount = 1; // msaa doesn't work on deferred back buffer
Format gSwapchainColorBufferFormat = Format::R8G8B8A8_UNORM;
Format gSwapchainDepthStencilBufferFormat = Format::D24_UNORM_S8_UINT;

Renderer gRenderer;
Store gStoreDefault("default store");
Scene gSceneDefault(L"default scene");
PassDefault gPassPom(L"pom pass");
PassDefault gPassStandard(L"standard pass", true, true, true);
PassDefault gPassDeferred(L"deferred pass", true, false);
PassDefault gPassSky(L"sky pass");
PassDefault gPassRedLightShadow(L"red light shadow", false);
PassDefault gPassGreenLightShadow(L"green light shadow", false);
PassDefault gPassBlueLightShadow(L"blue light shadow", false);
PassDefault gPassPathTracerBlit(L"path tracer blit", true, false);
OrbitCamera gMainCamera(4.f, 90.f, 0.f, XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), gWidthDeferred, gHeightDeferred, 45.0f, 100.0f, 0.1f);
Camera gCameraRedLight(XMFLOAT3(8, 0, 0), XMFLOAT3(0, 0, 0), XMFLOAT3(0, 1, 0), gWidthShadow, gHeightShadow, 90.f, 100.0f, 0.1f);
Camera gCameraGreenLight(XMFLOAT3(0, 8, 0), XMFLOAT3(0, 0, 0), XMFLOAT3(1, 0, 0), gWidthShadow, gHeightShadow, 90.f, 100.0f, 0.1f);
Camera gCameraBlueLight(XMFLOAT3(0, 0, 8), XMFLOAT3(0, 0, 0), XMFLOAT3(0, 1, 0), gWidthShadow, gHeightShadow, 90.f, 100.0f, 0.1f);
Mesh gMesh(L"mesh", Mesh::MeshType::MESH, XMFLOAT3(0, 2, 0), XMFLOAT3(0, 0, 0), XMFLOAT3(0.5f, 0.5f, 0.5f), "ball.obj");
Mesh gPlaneX(L"planeX", Mesh::MeshType::PLANE, XMFLOAT3(-3, 0, 0), XMFLOAT3(0, 0, -90), XMFLOAT3(6, 1, 6));
Mesh gPlaneY(L"planeY", Mesh::MeshType::PLANE, XMFLOAT3(0, -3, 0), XMFLOAT3(0, 0, 0), XMFLOAT3(6, 1, 6));
Mesh gPlaneZ(L"planeZ", Mesh::MeshType::PLANE, XMFLOAT3(0, 0, -3), XMFLOAT3(90, 0, 0), XMFLOAT3(6, 1, 6));
Mesh gSky(L"sky", Mesh::MeshType::SKY_FULLSCREEN_TRIANGLE, XMFLOAT3(0, 0, 0), XMFLOAT3(0, 0, 0), XMFLOAT3(1, 1, 1));
Shader gStandardVS(Shader::ShaderType::VERTEX_SHADER, L"vs.hlsl");
Shader gStandardPS(Shader::ShaderType::PIXEL_SHADER, L"ps.hlsl");
Shader gPomPS(Shader::ShaderType::PIXEL_SHADER, L"ps_pom.hlsl");
Shader gDeferredPS(Shader::ShaderType::PIXEL_SHADER, L"ps_deferred.hlsl"); // using multisampled srv
Shader gSkyVS(Shader::ShaderType::VERTEX_SHADER, L"vs_deferred.hlsl");
Shader gSkyPS(Shader::ShaderType::PIXEL_SHADER, L"ps_sky.hlsl");
Texture gTextureAlbedo("brick_albedo.jpg", L"albedo", gSamplerLinear, true, Format::R8G8B8A8_UNORM);
Texture gTextureNormal("brick_normal.jpg", L"normal", gSamplerLinear, true, Format::R8G8B8A8_UNORM);
Texture gTextureHeight("brick_height.jpg", L"height", gSamplerLinear, true, Format::R8G8B8A8_UNORM);
Texture gTextureProbe("probe.jpg", L"probe", gSamplerLinear, true, Format::R8G8B8A8_UNORM);
RenderTexture gRenderTextureGbuffer0(TextureType::DEFAULT, L"gbuffer0", gWidthDeferred, gHeightDeferred, 1, 1, ReadFrom::COLOR, gSamplerPoint, Format::R16G16B16A16_UNORM, XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
RenderTexture gRenderTextureGbuffer1(TextureType::DEFAULT, L"gbuffer1", gWidthDeferred, gHeightDeferred, 1, 1, ReadFrom::COLOR, gSamplerPoint, Format::R16G16B16A16_UNORM, XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
RenderTexture gRenderTextureGbuffer2(TextureType::DEFAULT, L"gbuffer2", gWidthDeferred, gHeightDeferred, 1, 1, ReadFrom::COLOR, gSamplerPoint, Format::R16G16B16A16_UNORM, XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
RenderTexture gRenderTextureDbuffer(TextureType::DEFAULT, L"dbuffer", gWidthDeferred, gHeightDeferred, 1, 1, ReadFrom::DEPTH, gSamplerPoint, Format::D16_UNORM, DEPTH_FARTHEST_REVERSED_Z_SWITCH, 0);
RenderTexture gRenderTextureRedLight(TextureType::DEFAULT, L"red light rt", gWidthShadow, gHeightShadow, 1, 1, ReadFrom::DEPTH, gSamplerPoint, Format::D16_UNORM, DEPTH_FARTHEST_REVERSED_Z_SWITCH, 0);
RenderTexture gRenderTextureGreenLight(TextureType::DEFAULT, L"green light rt", gWidthShadow, gHeightShadow, 1, 1, ReadFrom::DEPTH, gSamplerPoint, Format::D16_UNORM, DEPTH_FARTHEST_REVERSED_Z_SWITCH, 0);
RenderTexture gRenderTextureBlueLight(TextureType::DEFAULT, L"blue light rt", gWidthShadow, gHeightShadow, 1, 1, ReadFrom::DEPTH, gSamplerPoint, Format::D16_UNORM, DEPTH_FARTHEST_REVERSED_Z_SWITCH, 0);
Light gLightRed("light red", XMFLOAT3(0.8f, 0.1f, 0.1f), XMFLOAT3(8, 0, 0), &gCameraRedLight, &gRenderTextureRedLight);
Light gLightGreen("light green", XMFLOAT3(0.1f, 0.8f, 0.1f), XMFLOAT3(0, 8, 0), &gCameraGreenLight, &gRenderTextureGreenLight);
Light gLightBlue("light blue", XMFLOAT3(0.1f, 0.1f, 0.8f), XMFLOAT3(0, 0, 8), &gCameraBlueLight, &gRenderTextureBlueLight);
//imgui stuff
ID3D12DescriptorHeap* g_pd3dSrvDescHeap = NULL;

void CreateStores()
{
	// A. Mesh
	// mesh.AddTexture(...);

	// B. Pass
	gPassRedLightShadow.SetCamera(&gCameraRedLight);
	gPassRedLightShadow.AddMesh(&gCube);
	gPassRedLightShadow.AddMesh(&gMesh);
	gPassRedLightShadow.AddMesh(&gPlaneX);
	gPassRedLightShadow.AddMesh(&gPlaneY);
	gPassRedLightShadow.AddMesh(&gPlaneZ);
	gPassRedLightShadow.AddShader(&gStandardVS);
	gPassRedLightShadow.AddRenderTexture(&gRenderTextureRedLight, 0, 0, BlendState::NoBlend(), DS_REVERSED_Z_SWITCH);

	gPassGreenLightShadow.SetCamera(&gCameraGreenLight);
	gPassGreenLightShadow.AddMesh(&gCube);
	gPassGreenLightShadow.AddMesh(&gMesh);
	gPassGreenLightShadow.AddMesh(&gPlaneX);
	gPassGreenLightShadow.AddMesh(&gPlaneY);
	gPassGreenLightShadow.AddMesh(&gPlaneZ);
	gPassGreenLightShadow.AddShader(&gStandardVS);
	gPassGreenLightShadow.AddRenderTexture(&gRenderTextureGreenLight, 0, 0, BlendState::NoBlend(), DS_REVERSED_Z_SWITCH);

	gPassBlueLightShadow.SetCamera(&gCameraBlueLight);
	gPassBlueLightShadow.AddMesh(&gCube);
	gPassBlueLightShadow.AddMesh(&gMesh);
	gPassBlueLightShadow.AddMesh(&gPlaneX);
	gPassBlueLightShadow.AddMesh(&gPlaneY);
	gPassBlueLightShadow.AddMesh(&gPlaneZ);
	gPassBlueLightShadow.AddShader(&gStandardVS);
	gPassBlueLightShadow.AddRenderTexture(&gRenderTextureBlueLight, 0, 0, BlendState::NoBlend(), DS_REVERSED_Z_SWITCH);

	gPassStandard.SetCamera(&gMainCamera);
	gPassStandard.AddMesh(&gMesh);
	gPassStandard.AddShader(&gStandardVS);
	gPassStandard.AddShader(&gStandardPS);
	gPassStandard.AddTexture(&gTextureAlbedo);
	gPassStandard.AddTexture(&gTextureNormal);
	gPassStandard.AddRenderTexture(&gRenderTextureDbuffer, 0, 0, DS_REVERSED_Z_SWITCH);
	gPassStandard.AddRenderTexture(&gRenderTextureGbuffer0, 0, 0, BlendState::NoBlend());
	gPassStandard.AddRenderTexture(&gRenderTextureGbuffer1, 0, 0, BlendState::NoBlend());
	gPassStandard.AddRenderTexture(&gRenderTextureGbuffer2, 0, 0, BlendState::NoBlend());

	gPassPom.SetCamera(&gMainCamera);
	gPassPom.AddMesh(&gCube);
	gPassPom.AddMesh(&gPlaneX);
	gPassPom.AddMesh(&gPlaneY);
	gPassPom.AddMesh(&gPlaneZ);
	gPassPom.AddShader(&gStandardVS);
	gPassPom.AddShader(&gPomPS);
	gPassPom.AddTexture(&gTextureAlbedo);
	gPassPom.AddTexture(&gTextureNormal);
	gPassPom.AddTexture(&gTextureHeight);
	gPassPom.AddRenderTexture(&gRenderTextureDbuffer, 0, 0, DS_REVERSED_Z_SWITCH);
	gPassPom.AddRenderTexture(&gRenderTextureGbuffer0, 0, 0, BlendState::NoBlend());
	gPassPom.AddRenderTexture(&gRenderTextureGbuffer1, 0, 0, BlendState::NoBlend());
	gPassPom.AddRenderTexture(&gRenderTextureGbuffer2, 0, 0, BlendState::NoBlend());

	// render to path tracer backbuffer
	gPassPathTracerBlit.SetCamera(&gMainCamera);
	gPassPathTracerBlit.AddMesh(&gFullscreenTriangle);
	gPassPathTracerBlit.AddShader(&gDeferredVS);
	gPassPathTracerBlit.AddShader(&gBlitPS);
	gPassPathTracerBlit.AddTexture(&PathTracer::sPtBackbuffer);

	// render to backbuffer
	gPassDeferred.SetCamera(&gMainCamera);
	gPassDeferred.AddMesh(&gFullscreenTriangle);
	gPassDeferred.AddShader(&gDeferredVS);
	gPassDeferred.AddShader(&gDeferredPS);
	gPassDeferred.AddTexture(&gRenderTextureGbuffer0);
	gPassDeferred.AddTexture(&gRenderTextureGbuffer1);
	gPassDeferred.AddTexture(&gRenderTextureGbuffer2);
	gPassDeferred.AddTexture(&gRenderTextureDbuffer);
	gPassDeferred.AddTexture(&gTextureProbe);
	gPassDeferred.AddTexture(&ImageBasedLighting::sPrefilteredEnvMap);
	gPassDeferred.AddTexture(&ImageBasedLighting::sLUT);

	// render to back buffer
	gPassSky.SetCamera(&gMainCamera);
	gPassSky.AddMesh(&gSky);
	gPassSky.AddShader(&gSkyVS);
	gPassSky.AddShader(&gSkyPS);
	gPassSky.AddRenderTexture(&gRenderTextureDbuffer, 0, 0, DS_EQUAL_NO_WRITE_REVERSED_Z_SWITCH);

	// C. Frame
	gRenderer.mFrames.resize(gFrameCount);
	// renderer.mFrames[i].AddTexture(...);

	// D. Scene
	gSceneDefault.mSceneUniform.prefilteredEnvMapLevelCount = ImageBasedLighting::sPrefilteredEnvMapMipLevelCount;
	gSceneDefault.AddPass(&gPassRedLightShadow);
	gSceneDefault.AddPass(&gPassGreenLightShadow);
	gSceneDefault.AddPass(&gPassBlueLightShadow);
	gSceneDefault.AddPass(&gPassStandard);
	gSceneDefault.AddPass(&gPassPom);
	gSceneDefault.AddPass(&gPassSky);
	gSceneDefault.AddPass(&gPassDeferred);
	gSceneDefault.AddPass(&gPassPathTracerBlit);
	gSceneDefault.AddLight(&gLightRed);
	gSceneDefault.AddLight(&gLightGreen);
	gSceneDefault.AddLight(&gLightBlue);
	// scene.AddTexture(...);

	// E. Store
	gStoreDefault.AddCamera(&gMainCamera); // camera
	gStoreDefault.AddCamera(&gCameraDummy);
	gStoreDefault.AddCamera(&gCameraRedLight);
	gStoreDefault.AddCamera(&gCameraGreenLight);
	gStoreDefault.AddCamera(&gCameraBlueLight);
	gStoreDefault.AddScene(&gSceneDefault); // scene
	gStoreDefault.AddPass(&gPassStandard); // pass
	gStoreDefault.AddPass(&gPassPom);
	gStoreDefault.AddPass(&gPassSky);
	gStoreDefault.AddPass(&gPassDeferred);
	gStoreDefault.AddPass(&gPassRedLightShadow);
	gStoreDefault.AddPass(&gPassGreenLightShadow);
	gStoreDefault.AddPass(&gPassBlueLightShadow);
	gStoreDefault.AddPass(&gPassPathTracerBlit);
	gStoreDefault.AddMesh(&gFullscreenTriangle); // mesh
	gStoreDefault.AddMesh(&gCube);
	gStoreDefault.AddMesh(&gSky);
	gStoreDefault.AddMesh(&gMesh);
	gStoreDefault.AddMesh(&gPlaneX);
	gStoreDefault.AddMesh(&gPlaneY);
	gStoreDefault.AddMesh(&gPlaneZ);
	gStoreDefault.AddShader(&gStandardVS); // vertex shader
	gStoreDefault.AddShader(&gDeferredVS);
	gStoreDefault.AddShader(&gSkyVS);
	gStoreDefault.AddShader(&gStandardPS); // pixel shader
	gStoreDefault.AddShader(&gPomPS); 
	gStoreDefault.AddShader(&gDeferredPS);
	gStoreDefault.AddShader(&gSkyPS);
	gStoreDefault.AddShader(&gBlitPS);
	gStoreDefault.AddTexture(&gTextureAlbedo); // texture
	gStoreDefault.AddTexture(&gTextureNormal); // texture
	gStoreDefault.AddTexture(&gTextureHeight); // texture
	gStoreDefault.AddTexture(&gTextureProbe); // texture
	gStoreDefault.AddTexture(&gRenderTextureRedLight);
	gStoreDefault.AddTexture(&gRenderTextureGreenLight);
	gStoreDefault.AddTexture(&gRenderTextureBlueLight);
	gStoreDefault.AddTexture(&gRenderTextureGbuffer0);
	gStoreDefault.AddTexture(&gRenderTextureGbuffer1);
	gStoreDefault.AddTexture(&gRenderTextureGbuffer2);
	gStoreDefault.AddTexture(&gRenderTextureDbuffer);

	// F. Renderer
	gRenderer.SetStore(&gStoreDefault);
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

void UpdateDetectInput()
{
	BYTE keyboardCurrState[256];
	memset(keyboardCurrState, 0, sizeof(keyboardCurrState));//initialization is important, sometimes the value of unpressed key will not be changed

	gDIKeyboard->Acquire();
	gDIKeyboard->GetDeviceState(sizeof(keyboardCurrState), (LPVOID)&keyboardCurrState);

	POINT mouseCurrentCursorPos = {};
	GetCursorPos(&mouseCurrentCursorPos);
	ScreenToClient(gHwnd, &mouseCurrentCursorPos);
	
	//keyboard control
	//this is handled in mainloop, no need to do this here again
	//if (KEYDOWN(keyboardCurrState, DIK_ESCAPE))
	//{
	//	PostMessage(hwnd, WM_DESTROY, 0, 0);
	//}

	if (KEYDOWN(keyboardCurrState, DIK_C)) // hold c to operate camera
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
		// camera control
		bool orbit = false;
		bool pan = false;

		// mouse control
		DIMOUSESTATE mouseCurrentState = {};
		gDIMouse->GetDeviceState(sizeof(DIMOUSESTATE), &mouseCurrentState);

		// c + scroll to zoom
		if (mouseCurrentState.lZ != 0)
		{
			float tempDistance = gMainCamera.GetDistance() - mouseCurrentState.lZ * 0.01;
			if (tempDistance < 0 + EPSILON) tempDistance = 0.1 + EPSILON;
			gMainCamera.SetDistance(tempDistance);
		}

		// c + left button + drag to orbit
		if (BUTTONDOWN(mouseCurrentState.rgbButtons[0]))
		{
			if (mouseCurrentState.lX != 0)
			{
				gMainCamera.SetHorizontalAngle(gMainCamera.GetHorizontalAngle() + mouseCurrentState.lX * -0.1f);
			}
			if (mouseCurrentState.lY != 0)
			{
				float tempVerticalAngle = gMainCamera.GetVerticalAngle() + mouseCurrentState.lY * 0.1f;
				if (tempVerticalAngle > 90 - EPSILON) tempVerticalAngle = 89.f - EPSILON;
				if (tempVerticalAngle < -90 + EPSILON) tempVerticalAngle = -89.f + EPSILON;
				gMainCamera.SetVerticalAngle(tempVerticalAngle);
			}
		}

		// c + mid button + drag to pan
		if (BUTTONDOWN(mouseCurrentState.rgbButtons[2]))
		{
			float dX = (mouseCurrentCursorPos.x - gMouseLastCursorPos.x) * 0.005f;
			float dy = (mouseCurrentCursorPos.y - gMouseLastCursorPos.y) * 0.005f;
			XMVECTOR originalTarget = XMLoadFloat3(&gMainCamera.GetTarget());
			XMVECTOR right = XMLoadFloat3(&gMainCamera.GetRight());
			XMVECTOR up = XMLoadFloat3(&gMainCamera.GetRealUp());
			XMFLOAT3 newTarget = {};
			XMStoreFloat3(&newTarget, originalTarget + right * dX + up * dy);
			gMainCamera.SetTarget(newTarget);
		}

		gMouseLastState = mouseCurrentState;
		gMainCamera.Update();
		gUpdateCamera = gRenderer.mFrameCount;
	}
	
	gMouseLastCursorPos = mouseCurrentCursorPos;
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
		Renderer::TranslateFormat(gSwapchainColorBufferFormat),
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

void UpdateGameLogic()
{
	// update game logic (before waiting for the current frame to finish)
}

void UpdateResourcesGPU()
{
	// update per frame resource (after waiting for the current frame to finish)
	if (gUpdateCamera > 0)
	{
		// TODO: store a list of passes in camera and automate this shit
		gMainCamera.UpdatePassUniformBuffer(gRenderer.mCurrentFrameIndex);
		gUpdateCamera--;
	}

	if (gUpdateSettings > 0)
	{
		gSceneDefault.UpdateUniformBuffer(gRenderer.mCurrentFrameIndex);
		gUpdateSettings--;
	}
}

void WaitForCurrentFrame()
{
	if (!gRenderer.WaitForFrame(gRenderer.mCurrentFrameIndex))
		debugbreak(gRunning = false);
}

void Record()
{
	if (!gRenderer.RecordBegin(gRenderer.mCurrentFrameIndex, gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex]))
		debugbreak(gRunning = false);

	if (gPathTracerMode)
	{
		PathTracer::RunPathTracer(gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex]);
		gRenderer.RecordPass(gPassPathTracerBlit, gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex], true, false, false);
	}
	else
	{
		gRenderTextureRedLight.MakeReadyToRender(gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex]);
		gRenderTextureGreenLight.MakeReadyToRender(gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex]);
		gRenderTextureBlueLight.MakeReadyToRender(gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex]);

		gRenderer.RecordPass(gPassRedLightShadow, gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex], false);
		gRenderer.RecordPass(gPassGreenLightShadow, gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex], false);
		gRenderer.RecordPass(gPassBlueLightShadow, gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex], false);

		gRenderTextureRedLight.MakeReadyToRead(gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex]);
		gRenderTextureGreenLight.MakeReadyToRead(gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex]);
		gRenderTextureBlueLight.MakeReadyToRead(gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex]);

		gRenderTextureGbuffer0.MakeReadyToRender(gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex]);
		gRenderTextureGbuffer1.MakeReadyToRender(gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex]);
		gRenderTextureGbuffer2.MakeReadyToRender(gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex]);
		gRenderTextureDbuffer.MakeReadyToRender(gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex]);

		gRenderer.RecordPass(gPassStandard, gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex]);
		gRenderer.RecordPass(gPassPom, gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex], false, false, false);

		gRenderTextureGbuffer0.MakeReadyToRead(gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex]);
		gRenderTextureGbuffer1.MakeReadyToRead(gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex]);
		gRenderTextureGbuffer2.MakeReadyToRead(gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex]);
		gRenderTextureDbuffer.MakeReadyToRead(gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex]);

		gRenderer.RecordPass(gPassDeferred, gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex], true, false, false);

		gRenderTextureDbuffer.MakeReadyToRender(gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex]);
		gRenderer.RecordPass(gPassSky, gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex], false, false, false);
	}

	gRenderer.ResolveFrame(gRenderer.mCurrentFrameIndex, gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex]);

	///////// IMGUI PIPELINE /////////
	//vvvvvvvvvvvvvvvvvvvvvvvvvvvvvv//
	gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex]->OMSetRenderTargets(1, &gRenderer.GetRtvHandle(gRenderer.mCurrentFrameIndex), FALSE, nullptr);
	gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex]->SetDescriptorHeaps(1, &g_pd3dSrvDescHeap);
	ImGui::Render();
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex]);
	///////// IMGUI PIPELINE /////////
	//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^//

	if (!gRenderer.RecordEnd(gRenderer.mCurrentFrameIndex, gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex]))
		debugbreak(gRunning = false);
}

void Submit()
{
	if (!gRenderer.Submit(gRenderer.mCurrentFrameIndex, gRenderer.mGraphicsCommandQueue, { gRenderer.mCommandLists[gRenderer.mCurrentFrameIndex] }))
		debugbreak(gRunning = false);
}

void Present()
{
	if (!gRenderer.Present())
		debugbreak(gRunning = false);
}

void DrawBegin()
{
	WaitForCurrentFrame();
}

void Draw()
{
	Record();
	Submit();
	Present();
	gRenderer.mFrameCountTotal++;
}

void UpdateUI()
{
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	static int mode = gSceneDefault.mSceneUniform.mode = 0;
	static int pomMarchStep = gSceneDefault.mSceneUniform.pomMarchStep = 10;
	static float pomScale = gSceneDefault.mSceneUniform.pomScale = 0.07f;
	static float pomBias = gSceneDefault.mSceneUniform.pomBias = 0.3f;
	static float skyScatterG = gSceneDefault.mSceneUniform.skyScatterG = 0.975f;
	static int skyMarchStep = gSceneDefault.mSceneUniform.skyMarchStep = 10;
	static int skyMarchStepTr = gSceneDefault.mSceneUniform.skyMarchStepTr = 5;
	static XMFLOAT3 sunRadiance = gSceneDefault.mSceneUniform.sunRadiance = XMFLOAT3(6.639f, 3.718f, 5.777f);
	static float sunAzimuth = gSceneDefault.mSceneUniform.sunAzimuth = 0.080f;
	static float sunAltitude = gSceneDefault.mSceneUniform.sunAltitude = 0.116f;
	static int lightDebugOffset = gSceneDefault.mSceneUniform.lightDebugOffset = 0;
	static int lightDebugCount = gSceneDefault.mSceneUniform.lightDebugCount = 0;
	static float fresnel = gSceneDefault.mSceneUniform.fresnel = 0.9f;
	static float roughness = gSceneDefault.mSceneUniform.roughness = 0.265f;
	static bool useStandardTextures = gSceneDefault.mSceneUniform.useStandardTextures = false;
	static XMFLOAT4 standardColor = gSceneDefault.mSceneUniform.standardColor = XMFLOAT4(0.9f, 0.8f, 0.05f, 1.0f);
	static float metallic = gSceneDefault.mSceneUniform.metallic = 1.0f;
	static float specularity = gSceneDefault.mSceneUniform.specularity = 1.0f;
	static int sampleNumIBL = gSceneDefault.mSceneUniform.sampleNumIBL = 32;
	static int showReferenceIBL = gSceneDefault.mSceneUniform.showReferenceIBL = 0;
	static int useSceneLight = gSceneDefault.mSceneUniform.useSceneLight = 1;
	static int useSunLight = gSceneDefault.mSceneUniform.useSunLight = 1;
	static int useIBL = gSceneDefault.mSceneUniform.useIBL = 1;
	static bool needToUpdateSceneUniform = true;

	ImGui::SetNextWindowPos(ImVec2(0, 0));

	ImGui::Begin("Control Panel ");

	if (ImGui::Combo("mode", &mode, "default\0albedo\0normal\0uv\0pos\0restored pos g\0restored pos d\0abs diff pos"))
	{
		gSceneDefault.mSceneUniform.mode = mode;
		needToUpdateSceneUniform = true;
	}

	if (ImGui::SliderInt("pomMarchStep", &pomMarchStep, 0, 50))
	{
		gSceneDefault.mSceneUniform.pomMarchStep = pomMarchStep;
		needToUpdateSceneUniform = true;
	}

	if (ImGui::SliderFloat("pomScale", &pomScale, 0.0f, 1.0f))
	{
		gSceneDefault.mSceneUniform.pomScale = pomScale;
		needToUpdateSceneUniform = true;
	}

	if (ImGui::SliderFloat("pomBias", &pomBias, 0.0f, 1.0f))
	{
		gSceneDefault.mSceneUniform.pomBias = pomBias;
		needToUpdateSceneUniform = true;
	}
	
	if (ImGui::SliderFloat("skyScatterG", &skyScatterG, -2.0f, 2.0f))
	{
		gSceneDefault.mSceneUniform.skyScatterG = skyScatterG;
		needToUpdateSceneUniform = true;
	}

	if (ImGui::SliderInt("skyMarchStep", &skyMarchStep, 0, 50))
	{
		gSceneDefault.mSceneUniform.skyMarchStep = skyMarchStep;
		needToUpdateSceneUniform = true;
	}

	if (ImGui::SliderInt("skyMarchStepTr", &skyMarchStepTr, 0, 50))
	{
		gSceneDefault.mSceneUniform.skyMarchStepTr = skyMarchStepTr;
		needToUpdateSceneUniform = true;
	}

	if (ImGui::SliderFloat("sunRadianceR", &sunRadiance.x, 0.0f, 10.0f))
	{
		gSceneDefault.mSceneUniform.sunRadiance.x = sunRadiance.x;
		needToUpdateSceneUniform = true;
	}

	if (ImGui::SliderFloat("sunRadianceG", &sunRadiance.y, 0.0f, 10.0f))
	{
		gSceneDefault.mSceneUniform.sunRadiance.y = sunRadiance.y;
		needToUpdateSceneUniform = true;
	}

	if (ImGui::SliderFloat("sunRadianceB", &sunRadiance.z, 0.0f, 10.0f))
	{
		gSceneDefault.mSceneUniform.sunRadiance.z = sunRadiance.z;
		needToUpdateSceneUniform = true;
	}

	if (ImGui::SliderFloat("sunAzimuth", &sunAzimuth, 0.0f, 1.0f))
	{
		gSceneDefault.mSceneUniform.sunAzimuth = sunAzimuth;
		needToUpdateSceneUniform = true;
	}

	if (ImGui::SliderFloat("sunAltitude", &sunAltitude, 0.0f, 1.0f))
	{
		gSceneDefault.mSceneUniform.sunAltitude = sunAltitude;
		needToUpdateSceneUniform = true;
	}

	if (ImGui::SliderInt("lightDebugOffset", &lightDebugOffset, 0, gSceneDefault.GetLightCount() - 1))
	{
		gSceneDefault.mSceneUniform.lightDebugOffset = lightDebugOffset;
		needToUpdateSceneUniform = true;
	}

	if (ImGui::SliderInt("lightDebugCount", &lightDebugCount, 0, gSceneDefault.GetLightCount()))
	{
		gSceneDefault.mSceneUniform.lightDebugCount = lightDebugCount;
		needToUpdateSceneUniform = true;
	}

	if (ImGui::SliderFloat("fresnel", &fresnel, 0.0f, 1.0f))
	{
		gSceneDefault.mSceneUniform.fresnel = fresnel;
		needToUpdateSceneUniform = true;
	}

	if (ImGui::SliderFloat("roughness", &roughness, 0.0f, 1.0f))
	{
		gSceneDefault.mSceneUniform.roughness = roughness;
		needToUpdateSceneUniform = true;
	}

	if (ImGui::Checkbox("useStandardTextures", &useStandardTextures))
	{
		gSceneDefault.mSceneUniform.useStandardTextures = useStandardTextures ? 1.0f : 0.0f;
		needToUpdateSceneUniform = true;
	}

	if (ImGui::ColorEdit4("standardColor", (float*)&standardColor))
	{
		gSceneDefault.mSceneUniform.standardColor = standardColor;
		needToUpdateSceneUniform = true;
	}

	if (ImGui::SliderFloat("metallic", &metallic, 0.0f, 1.0f))
	{
		gSceneDefault.mSceneUniform.metallic = metallic;
		needToUpdateSceneUniform = true;
	}

	if (ImGui::SliderFloat("specularity", &specularity, 0.0f, 1.0f))
	{
		gSceneDefault.mSceneUniform.specularity = specularity;
		needToUpdateSceneUniform = true;
	}

	if (ImGui::SliderInt("sampleNumIBL", &sampleNumIBL, 0, 128))
	{
		gSceneDefault.mSceneUniform.sampleNumIBL = sampleNumIBL;
		needToUpdateSceneUniform = true;
	}

	if (ImGui::SliderInt("showReferenceIBL", &showReferenceIBL, 0, 1))
	{
		gSceneDefault.mSceneUniform.showReferenceIBL = showReferenceIBL;
		needToUpdateSceneUniform = true;
	}

	if (ImGui::SliderInt("useSceneLight", &useSceneLight, 0, 1))
	{
		gSceneDefault.mSceneUniform.useSceneLight = useSceneLight;
		needToUpdateSceneUniform = true;
	}

	if (ImGui::SliderInt("useSunLight", &useSunLight, 0, 1))
	{
		gSceneDefault.mSceneUniform.useSunLight = useSunLight;
		needToUpdateSceneUniform = true;
	}

	if (ImGui::SliderInt("useIBL", &useIBL, 0, 1))
	{
		gSceneDefault.mSceneUniform.useIBL = useIBL;
		needToUpdateSceneUniform = true;
	}

	if (ImGui::SliderInt("pathTracerMode", &gPathTracerMode, 0, 1))
	{
		gSceneDefault.mSceneUniform.pathTracerMode = gPathTracerMode;
		needToUpdateSceneUniform = true;
	}

	ImGui::Text("%.3f ms/frame (%.1f FPS) ", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	ImGui::Text("Hold C and use mouse to rotate camera.");
	ImGui::End();

	if (needToUpdateSceneUniform)
	{
		gUpdateSettings = gRenderer.mFrameCount;
		needToUpdateSceneUniform = false;
	}
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
	SAFE_RELEASE_NO_CHECK(g_pd3dSrvDescHeap);

	//direct input stuff
	gDIKeyboard->Unacquire();
	gDIMouse->Unacquire();
	gDirectInput->Release();

	ImageBasedLighting::Shutdown();
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
			UpdateDetectInput();
			UpdateGameLogic();
			DrawBegin();
			UpdateResourcesGPU();
			Draw();
		}
	}
}

void CreateSystems()
{
	ImageBasedLighting::InitIBL(gStoreDefault, gSceneDefault);
	PathTracer::InitPathTracer(gStoreDefault, gSceneDefault);
}

void PrepareSystems()
{
	gUpdateCamera = gRenderer.mFrameCount + 1; // 1 for preparation, rest for initialization
	UpdateResourcesGPU();

	ID3D12GraphicsCommandList* commandList = gRenderer.BeginSingleTimeCommands();

	ImageBasedLighting::PrepareIBL(commandList);
	PathTracer::PreparePathTracer();

	gRenderer.EndSingleTimeCommands(commandList);
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

	// use hard coded data to create the store
	CreateStores();

	// create systems
	CreateSystems();

	// initialize renderer
	if (!gRenderer.InitRenderer(
		gHwnd, 
		gFrameCount, 
		gMultiSampleCount, 
		gWidth, 
		gHeight,
		gSwapchainColorBufferFormat,
		gSwapchainDepthStencilBufferFormat,
		DebugMode::GBV))
	{
		MessageBox(0, L"Failed to initialize renderer", L"Error", MB_OK);
		Cleanup();
		return 1;
	}

	// initialize imgui
	if (!InitImgui())
	{
		MessageBox(0, L"Failed to initialize imgui", L"Error", MB_OK);
		Cleanup();
		return 1;
	}

	// prepare systems
	PrepareSystems();

	// start the main loop
	MainLoop();

	// clean up everything
	Cleanup();

	return 0;
}

int GetUniformSlot(RegisterSpace space, RegisterType type)
{
	// constant buffer is root parameter, SRV, sampler and UAV are stored in table
	//                 b0 CBV | t0 SRV | s0 sampler | u0 UAV |
	// space 0 scene   0      | 4      | 8          | 12     |
	// space 1 frame   1      | 5      | 9          | 13     |
	// space 2 pass    2      | 6      | 10         | 14     |
	// space 3 object  3      | 7      | 11         | 15     |

	return (int)space + (int)type * (int)RegisterSpace::COUNT;
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
