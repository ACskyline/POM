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
const DebugMode gRendererDebugMode = DebugMode::ON; // turning on debug will impact the framerate badly
IDirectInputDevice8* gDIKeyboard;
IDirectInputDevice8* gDIMouse;
DIMOUSESTATE gMouseLastState;
POINT gMouseLastCursorPos;
BYTE gKeyboardLastState[256];
bool gMouseAcquired = false;
LPDIRECTINPUT8 gDirectInput;
bool gFullScreen = false; // is window full screen?
bool gRunning = true; // we will exit the program when this becomes false
int gPathTracerMode = 0;
bool gPathTracerForceUpdate = false;

Renderer gRenderer;
Store gStoreDefault("default store");
Scene gSceneDefault(L"default scene");
PassDefault gPassPom(L"pom pass", true, true, true);
PassDefault gPassStandard(L"standard pass", true, true, true);
PassDefault gPassDeferred(L"deferred pass", true, false);
PassDefault gPassSky(L"sky pass");
PassDefault gPassRedLightShadow(L"red light shadow", false);
PassDefault gPassGreenLightShadow(L"green light shadow", false);
PassDefault gPassBlueLightShadow(L"blue light shadow", false);
PassDefault gPassPathTracerBlit(L"path tracer blit", true, false);
Camera gCameraRedLight(XMFLOAT3(8, 0, 0), XMFLOAT3(0, 0, 0), XMFLOAT3(0, 1, 0), gWidthShadow, gHeightShadow, 90.f, 100.0f, 0.1f);
Camera gCameraGreenLight(XMFLOAT3(0, 8, 0), XMFLOAT3(0, 0, 0), XMFLOAT3(1, 0, 0), gWidthShadow, gHeightShadow, 90.f, 100.0f, 0.1f);
Camera gCameraBlueLight(XMFLOAT3(0, 0, 8), XMFLOAT3(0, 0, 0), XMFLOAT3(0, 1, 0), gWidthShadow, gHeightShadow, 90.f, 100.0f, 0.1f);
Mesh gMesh(L"mesh", Mesh::MeshType::MESH, XMFLOAT3(0, 1, 0), XMFLOAT3(0, 45, 0), XMFLOAT3(0.5f, 0.5f, 0.5f), "box.obj");
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
RenderTexture gRenderTextureGbuffer0(TextureType::TEX_2D, L"gbuffer0", gWidthDeferred, gHeightDeferred, 1, 1, ReadFrom::COLOR, gSamplerPoint, Format::R16G16B16A16_UNORM, XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
RenderTexture gRenderTextureGbuffer1(TextureType::TEX_2D, L"gbuffer1", gWidthDeferred, gHeightDeferred, 1, 1, ReadFrom::COLOR, gSamplerPoint, Format::R16G16B16A16_UNORM, XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
RenderTexture gRenderTextureGbuffer2(TextureType::TEX_2D, L"gbuffer2", gWidthDeferred, gHeightDeferred, 1, 1, ReadFrom::COLOR, gSamplerPoint, Format::R16G16B16A16_UNORM, XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
RenderTexture gRenderTextureGbuffer3(TextureType::TEX_2D, L"gbuffer3", gWidthDeferred, gHeightDeferred, 1, 1, ReadFrom::COLOR, gSamplerPoint, Format::R16G16B16A16_UNORM, XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
RenderTexture gRenderTextureDbuffer(TextureType::TEX_2D, L"dbuffer", gWidthDeferred, gHeightDeferred, 1, 1, ReadFrom::DEPTH, gSamplerPoint, Format::D16_UNORM, DEPTH_FAR_REVERSED_Z_SWITCH, 0);
RenderTexture gRenderTextureRedLight(TextureType::TEX_2D, L"red light rt", gWidthShadow, gHeightShadow, 1, 1, ReadFrom::DEPTH, gSamplerPoint, Format::D16_UNORM, DEPTH_FAR_REVERSED_Z_SWITCH, 0);
RenderTexture gRenderTextureGreenLight(TextureType::TEX_2D, L"green light rt", gWidthShadow, gHeightShadow, 1, 1, ReadFrom::DEPTH, gSamplerPoint, Format::D16_UNORM, DEPTH_FAR_REVERSED_Z_SWITCH, 0);
RenderTexture gRenderTextureBlueLight(TextureType::TEX_2D, L"blue light rt", gWidthShadow, gHeightShadow, 1, 1, ReadFrom::DEPTH, gSamplerPoint, Format::D16_UNORM, DEPTH_FAR_REVERSED_Z_SWITCH, 0);
Light gLightRed("light red", XMFLOAT3(0.8f, 0.1f, 0.1f), XMFLOAT3(8, 0, 0), &gCameraRedLight, &gRenderTextureRedLight);
Light gLightGreen("light green", XMFLOAT3(0.1f, 0.8f, 0.1f), XMFLOAT3(0, 8, 0), &gCameraGreenLight, &gRenderTextureGreenLight);
Light gLightBlue("light blue", XMFLOAT3(0.1f, 0.1f, 0.8f), XMFLOAT3(0, 0, 8), &gCameraBlueLight, &gRenderTextureBlueLight);
//imgui stuff
ID3D12DescriptorHeap* g_pd3dSrvDescHeap = NULL;

void CreateStores()
{
	// A. Mesh
	gMesh.AddTexture(&gTextureAlbedo);
	gMesh.AddTexture(&gTextureNormal);
	gCube.AddTexture(&gTextureAlbedo);
	gCube.AddTexture(&gTextureNormal);
	gCube.AddTexture(&gTextureHeight);
	gPlaneX.AddTexture(&gTextureAlbedo);
	gPlaneX.AddTexture(&gTextureNormal);
	gPlaneX.AddTexture(&gTextureHeight);
	gPlaneY.AddTexture(&gTextureAlbedo);
	gPlaneY.AddTexture(&gTextureNormal);
	gPlaneY.AddTexture(&gTextureHeight);
	gPlaneZ.AddTexture(&gTextureAlbedo);
	gPlaneZ.AddTexture(&gTextureNormal);
	gPlaneZ.AddTexture(&gTextureHeight);

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

	gPassStandard.SetCamera(&gCameraMain);
	gPassStandard.AddMesh(&gMesh);
	gPassStandard.AddShader(&gStandardVS);
	gPassStandard.AddShader(&gStandardPS);
	gPassStandard.AddTexture(&gTextureAlbedo);
	gPassStandard.AddTexture(&gTextureNormal);
	gPassStandard.AddRenderTexture(&gRenderTextureDbuffer, 0, 0, DS_REVERSED_Z_SWITCH);
	gPassStandard.AddRenderTexture(&gRenderTextureGbuffer0, 0, 0, BlendState::NoBlend());
	gPassStandard.AddRenderTexture(&gRenderTextureGbuffer1, 0, 0, BlendState::NoBlend());
	gPassStandard.AddRenderTexture(&gRenderTextureGbuffer2, 0, 0, BlendState::NoBlend());
	gPassStandard.AddRenderTexture(&gRenderTextureGbuffer3, 0, 0, BlendState::NoBlend());

	gPassPom.SetCamera(&gCameraMain);
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
	gPassPom.AddRenderTexture(&gRenderTextureGbuffer3, 0, 0, BlendState::NoBlend());

	// render to path tracer backbuffer
	gPassPathTracerBlit.SetCamera(&gCameraMain);
	gPassPathTracerBlit.AddMesh(&gFullscreenTriangle);
	gPassPathTracerBlit.AddShader(&gDeferredVS);
	gPassPathTracerBlit.AddShader(&PathTracer::sPathTracerBlitPS);
	gPassPathTracerBlit.AddTexture(&PathTracer::sBackbufferPT);
	gPassPathTracerBlit.AddTexture(&PathTracer::sDebugBackbufferPT);
	
	// render to backbuffer
	gPassDeferred.SetCamera(&gCameraMain);
	gPassDeferred.AddMesh(&gFullscreenTriangle);
	gPassDeferred.AddShader(&gDeferredVS);
	gPassDeferred.AddShader(&gDeferredPS);
	gPassDeferred.AddTexture(&gRenderTextureGbuffer0);
	gPassDeferred.AddTexture(&gRenderTextureGbuffer1);
	gPassDeferred.AddTexture(&gRenderTextureGbuffer2);
	gPassDeferred.AddTexture(&gRenderTextureGbuffer3);
	gPassDeferred.AddTexture(&gRenderTextureDbuffer);
	gPassDeferred.AddTexture(&gTextureProbe);
	gPassDeferred.AddTexture(&ImageBasedLighting::sPrefilteredEnvMap);
	gPassDeferred.AddTexture(&ImageBasedLighting::sLUT);

	// render to back buffer
	gPassSky.SetCamera(&gCameraMain);
	gPassSky.AddMesh(&gSky);
	gPassSky.AddShader(&gSkyVS);
	gPassSky.AddShader(&gSkyPS);
	gPassSky.AddRenderTexture(&gRenderTextureDbuffer, 0, 0, DS_EQUAL_NO_WRITE_REVERSED_Z_SWITCH);

	// C. Frame
	gRenderer.mFrames.resize(gFrameCount);
	// renderer.mFrames[i].AddTexture(...);

	// D. Scene
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
	gStoreDefault.AddCamera(&gCameraMain); // camera
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
	gStoreDefault.AddTexture(&gRenderTextureGbuffer3);
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
	//ScreenToClient(gHwnd, &mouseCurrentCursorPos); // don't want to use absolute position
	
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
		bool needToUpdateCamera = false;
		bool needToRestartPathTracer = false;
		bool needToUpdateSceneUniform = false;

		// mouse control
		DIMOUSESTATE mouseCurrentState = {};
		gDIMouse->GetDeviceState(sizeof(DIMOUSESTATE), &mouseCurrentState);

		// c + scroll to zoom
		if (mouseCurrentState.lZ != 0)
		{
			float tempDistance = gCameraMain.GetDistance() - mouseCurrentState.lZ * 0.01;
			if (tempDistance < 0 + EPSILON) tempDistance = 0.1 + EPSILON;
			gCameraMain.SetDistance(tempDistance);
			needToUpdateCamera = true;
		}

		// c + left button + drag to orbit
		if (BUTTONDOWN(mouseCurrentState.rgbButtons[0]))
		{
			if (mouseCurrentState.lX != 0)
			{
				gCameraMain.SetHorizontalAngle(gCameraMain.GetHorizontalAngle() + mouseCurrentState.lX * -0.1f);
				needToUpdateCamera = true;
			}
			if (mouseCurrentState.lY != 0)
			{
				float tempVerticalAngle = gCameraMain.GetVerticalAngle() + mouseCurrentState.lY * 0.1f;
				if (tempVerticalAngle > 90 - EPSILON) tempVerticalAngle = 89.f - EPSILON;
				if (tempVerticalAngle < -90 + EPSILON) tempVerticalAngle = -89.f + EPSILON;
				gCameraMain.SetVerticalAngle(tempVerticalAngle);
				needToUpdateCamera = true;
			}
		}

		// c + mid button + drag to pan
		if (BUTTONDOWN(mouseCurrentState.rgbButtons[2]))
		{
			float dX = (mouseCurrentCursorPos.x - gMouseLastCursorPos.x) * 0.005f;
			float dY = (mouseCurrentCursorPos.y - gMouseLastCursorPos.y) * 0.005f;
			if (dX || dY)
			{
				XMVECTOR originalTarget = XMLoadFloat3(&gCameraMain.GetTarget());
				XMVECTOR right = XMLoadFloat3(&gCameraMain.GetRight());
				XMVECTOR up = XMLoadFloat3(&gCameraMain.GetRealUp());
				XMFLOAT3 newTarget = {};
				XMStoreFloat3(&newTarget, originalTarget + right * dX + up * dY);
				gCameraMain.SetTarget(newTarget);
				needToUpdateCamera = true;
			}
		}

		// c + right button to debug ray
		if (BUTTONDOWN(mouseCurrentState.rgbButtons[1]))
		{
			bool xPosChanged = mouseCurrentCursorPos.x != gMouseLastCursorPos.x;
			bool yPosChanged = mouseCurrentCursorPos.y != gMouseLastCursorPos.y;
			if (xPosChanged || yPosChanged)
			{
				gSceneDefault.mSceneUniform.mPathTracerDebugPixelX = mouseCurrentCursorPos.x;
				gSceneDefault.mSceneUniform.mPathTracerDebugPixelY = mouseCurrentCursorPos.y;
				needToRestartPathTracer = true;
			}
		}

		needToRestartPathTracer = needToRestartPathTracer || needToUpdateCamera;
		needToUpdateSceneUniform = needToUpdateSceneUniform || needToRestartPathTracer;

		if (needToUpdateCamera)
		{
			gCameraMain.Update();
			gCameraMain.SetPassUniformDirty();
		}

		if (needToRestartPathTracer)
		{
			gSceneDefault.mSceneUniform.mPathTracerCurrentSampleIndex = 0;
			gSceneDefault.mSceneUniform.mPathTracerCurrentDepth = 0;
		}

		if (needToUpdateSceneUniform)
		{
			gSceneDefault.SetUniformDirty();
		}

		gMouseLastState = mouseCurrentState;
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
		gRenderer.mFramebufferCount,
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
	gRenderer.mFrames[gRenderer.mCurrentFramebufferIndex].mFrameUniform.mFrameCountSinceGameStart = gRenderer.mFrameCountSinceGameStart;
	gRenderer.mFrames[gRenderer.mCurrentFramebufferIndex].UpdateUniformBuffer();
}

void WaitForCurrentFrame()
{
	if (!gRenderer.WaitForFrame(gRenderer.mCurrentFramebufferIndex))
		debugbreak(gRunning = false);
}

void Record()
{
	ID3D12GraphicsCommandList* commandList = gRenderer.mCommandLists[gRenderer.mCurrentFramebufferIndex];

	if (!gRenderer.RecordBegin(gRenderer.mCurrentFramebufferIndex, commandList))
		debugbreak(gRunning = false);

	if (gPathTracerMode) // path tracer
	{
		bool finished = gSceneDefault.mSceneUniform.mPathTracerCurrentSampleIndex >= gSceneDefault.mSceneUniform.mPathTracerMaxSampleCountPerPixel;
		if (finished && gPathTracerForceUpdate)
			gSceneDefault.mSceneUniform.mPathTracerCurrentSampleIndex = 0;
		if (!finished || gPathTracerForceUpdate)
		{
			if (gSceneDefault.mSceneUniform.mPathTracerMode == PATH_TRACER_MODE_PROGRESSIVE)
			{
				PathTracer::RunPathTracer(commandList);
				if (gSceneDefault.mSceneUniform.mPathTracerCurrentDepth < gSceneDefault.mSceneUniform.mPathTracerMaxDepth)
				{
					gSceneDefault.mSceneUniform.mPathTracerCurrentDepth++;
				}
				else
				{
					gSceneDefault.mSceneUniform.mPathTracerCurrentDepth = 0;
					gSceneDefault.mSceneUniform.mPathTracerCurrentSampleIndex++;
				}
			}
			else
			{
				PathTracer::RunPathTracer(commandList);
				gSceneDefault.mSceneUniform.mPathTracerCurrentSampleIndex++;
			}
		}

		// always render debug passes for easier renderdoc capture
		PathTracer::sDebugRayBuffer.MakeReadyToRead(commandList);
		PathTracer::sDebugBackbufferPT.MakeReadyToRender(commandList);
		PathTracer::sDepthbufferRenderPT.MakeReadyToRender(commandList);
		gRenderer.RecordGraphicsPassInstanced(PathTracer::sPathTracerDebugPass, commandList, PATH_TRACER_MAX_DEPTH_MAX, true, false, false);
		gRenderer.RecordGraphicsPass(PathTracer::sPathTracerDebugFullscreenPass, commandList, false, false, false);

		PathTracer::sBackbufferPT.MakeReadyToRead(commandList);
		PathTracer::sDebugBackbufferPT.MakeReadyToRead(commandList);
		gRenderer.RecordGraphicsPass(gPassPathTracerBlit, commandList, false, false, false);
		gSceneDefault.SetUniformDirty(); // set dirty in the end for subsequent drawcalls (mainly for next frame)
	}
	else // rasterizer
	{
		gRenderTextureRedLight.MakeReadyToRender(commandList);
		gRenderTextureGreenLight.MakeReadyToRender(commandList);
		gRenderTextureBlueLight.MakeReadyToRender(commandList);

		gRenderer.RecordGraphicsPass(gPassRedLightShadow, commandList, false);
		gRenderer.RecordGraphicsPass(gPassGreenLightShadow, commandList, false);
		gRenderer.RecordGraphicsPass(gPassBlueLightShadow, commandList, false);

		gRenderTextureRedLight.MakeReadyToRead(commandList);
		gRenderTextureGreenLight.MakeReadyToRead(commandList);
		gRenderTextureBlueLight.MakeReadyToRead(commandList);

		gRenderTextureGbuffer0.MakeReadyToRender(commandList);
		gRenderTextureGbuffer1.MakeReadyToRender(commandList);
		gRenderTextureGbuffer2.MakeReadyToRender(commandList);
		gRenderTextureGbuffer3.MakeReadyToRender(commandList);
		gRenderTextureDbuffer.MakeReadyToRender(commandList);

		gRenderer.RecordGraphicsPass(gPassStandard, commandList);
		gRenderer.RecordGraphicsPass(gPassPom, commandList, false, false, false);

		gRenderTextureGbuffer0.MakeReadyToRead(commandList);
		gRenderTextureGbuffer1.MakeReadyToRead(commandList);
		gRenderTextureGbuffer2.MakeReadyToRead(commandList);
		gRenderTextureGbuffer3.MakeReadyToRead(commandList);
		gRenderTextureDbuffer.MakeReadyToRead(commandList);

		gRenderer.RecordGraphicsPass(gPassDeferred, commandList, true, false, false);

		gRenderTextureDbuffer.MakeReadyToRender(commandList);
		gRenderer.RecordGraphicsPass(gPassSky, commandList, false, false, false);
	}

	gRenderer.ResolveFrame(gRenderer.mCurrentFramebufferIndex, commandList);

	///////// IMGUI PIPELINE /////////
	//vvvvvvvvvvvvvvvvvvvvvvvvvvvvvv//
	commandList->OMSetRenderTargets(1, &gRenderer.GetRtvHandle(gRenderer.mCurrentFramebufferIndex), FALSE, nullptr);
	commandList->SetDescriptorHeaps(1, &g_pd3dSrvDescHeap);
	ImGui::Render();
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
	///////// IMGUI PIPELINE /////////
	//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^//

	if (!gRenderer.RecordEnd(gRenderer.mCurrentFramebufferIndex, commandList))
		debugbreak(gRunning = false);
}

void Submit()
{
	if (!gRenderer.Submit(gRenderer.mCurrentFramebufferIndex, gRenderer.mGraphicsCommandQueue, { gRenderer.mCommandLists[gRenderer.mCurrentFramebufferIndex] }))
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
}

void UpdateUI(bool initOnly = false)
{
	static int mode = gSceneDefault.mSceneUniform.mMode = 0;
	static int pomMarchStep = gSceneDefault.mSceneUniform.mPomMarchStep = 10;
	static float pomScale = gSceneDefault.mSceneUniform.mPomScale = 0.07f;
	static float pomBias = gSceneDefault.mSceneUniform.mPomBias = 0.3f;
	static float skyScatterG = gSceneDefault.mSceneUniform.mSkyScatterG = 0.975f;
	static int skyMarchStep = gSceneDefault.mSceneUniform.mSkyMarchStep = 10;
	static int skyMarchStepTr = gSceneDefault.mSceneUniform.mSkyMarchStepTr = 5;
	static XMFLOAT3 sunRadiance = gSceneDefault.mSceneUniform.mSunRadiance = XMFLOAT3(6.639f, 3.718f, 5.777f);
	static float sunAzimuth = gSceneDefault.mSceneUniform.mSunAzimuth = 0.080f;
	static float sunAltitude = gSceneDefault.mSceneUniform.mSunAltitude = 0.116f;
	static int lightDebugOffset = gSceneDefault.mSceneUniform.mLightDebugOffset = 0;
	static int lightDebugCount = gSceneDefault.mSceneUniform.mLightDebugCount = 0;
	static float fresnel = gSceneDefault.mSceneUniform.mFresnel = 0.9f;
	static float roughness = gSceneDefault.mSceneUniform.mRoughness = 0.265f;
	static bool usePerPassTextures = gSceneDefault.mSceneUniform.mUsePerPassTextures = 0;
	static XMFLOAT4 standardColor = gSceneDefault.mSceneUniform.mStandardColor = XMFLOAT4(0.9f, 0.8f, 0.05f, 1.0f);
	static float metallic = gSceneDefault.mSceneUniform.mMetallic = 1.0f;
	static float specularity = gSceneDefault.mSceneUniform.mSpecularity = 1.0f;
	static int sampleNumIBL = gSceneDefault.mSceneUniform.mSampleNumIBL = 32;
	static int showReferenceIBL = gSceneDefault.mSceneUniform.mShowReferenceIBL = 0;
	static int useSceneLight = gSceneDefault.mSceneUniform.mUseSceneLight = 1;
	static int useSunLight = gSceneDefault.mSceneUniform.mUseSunLight = 1;
	static int useIBL = gSceneDefault.mSceneUniform.mUseIBL = 1;
	static int pathTracerSPP = gSceneDefault.mSceneUniform.mPathTracerMaxSampleCountPerPixel = 10;
	static int pathTracerMinDepth = gSceneDefault.mSceneUniform.mPathTracerMinDepth = 3;
	static int pathTracerMaxDepth = gSceneDefault.mSceneUniform.mPathTracerMaxDepth = 10;
	static bool pathTracerEnableDebug = gSceneDefault.mSceneUniform.mPathTracerEnableDebug = 0;
	static bool pathTracerUpdateDebug = gSceneDefault.mSceneUniform.mPathTracerUpdateDebug = 0;
	bool needToUpdateSceneUniform = false;
	bool needToRestartPathTracer = false;

	if (!initOnly)
	{
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::SetNextWindowPos(ImVec2(0, 0));

		ImGui::Begin("Control Panel ");

		if (ImGui::Combo("mode", &mode, "default\0albedo\0normal\0uv\0pos\0restored pos g\0restored pos d\0abs diff pos"))
		{
			gSceneDefault.mSceneUniform.mMode = mode;
			needToUpdateSceneUniform = true;
		}

		if (ImGui::SliderInt("pomMarchStep", &pomMarchStep, 0, 50))
		{
			gSceneDefault.mSceneUniform.mPomMarchStep = pomMarchStep;
			needToUpdateSceneUniform = true;
		}

		if (ImGui::SliderFloat("pomScale", &pomScale, 0.0f, 1.0f))
		{
			gSceneDefault.mSceneUniform.mPomScale = pomScale;
			needToUpdateSceneUniform = true;
		}

		if (ImGui::SliderFloat("pomBias", &pomBias, 0.0f, 1.0f))
		{
			gSceneDefault.mSceneUniform.mPomBias = pomBias;
			needToUpdateSceneUniform = true;
		}

		if (ImGui::SliderFloat("skyScatterG", &skyScatterG, -2.0f, 2.0f))
		{
			gSceneDefault.mSceneUniform.mSkyScatterG = skyScatterG;
			needToUpdateSceneUniform = true;
		}

		if (ImGui::SliderInt("skyMarchStep", &skyMarchStep, 0, 50))
		{
			gSceneDefault.mSceneUniform.mSkyMarchStep = skyMarchStep;
			needToUpdateSceneUniform = true;
		}

		if (ImGui::SliderInt("skyMarchStepTr", &skyMarchStepTr, 0, 50))
		{
			gSceneDefault.mSceneUniform.mSkyMarchStepTr = skyMarchStepTr;
			needToUpdateSceneUniform = true;
		}

		if (ImGui::SliderFloat("sunRadianceR", &sunRadiance.x, 0.0f, 10.0f))
		{
			gSceneDefault.mSceneUniform.mSunRadiance.x = sunRadiance.x;
			needToUpdateSceneUniform = true;
		}

		if (ImGui::SliderFloat("sunRadianceG", &sunRadiance.y, 0.0f, 10.0f))
		{
			gSceneDefault.mSceneUniform.mSunRadiance.y = sunRadiance.y;
			needToUpdateSceneUniform = true;
		}

		if (ImGui::SliderFloat("sunRadianceB", &sunRadiance.z, 0.0f, 10.0f))
		{
			gSceneDefault.mSceneUniform.mSunRadiance.z = sunRadiance.z;
			needToUpdateSceneUniform = true;
		}

		if (ImGui::SliderFloat("sunAzimuth", &sunAzimuth, 0.0f, 1.0f))
		{
			gSceneDefault.mSceneUniform.mSunAzimuth = sunAzimuth;
			needToUpdateSceneUniform = true;
		}

		if (ImGui::SliderFloat("sunAltitude", &sunAltitude, 0.0f, 1.0f))
		{
			gSceneDefault.mSceneUniform.mSunAltitude = sunAltitude;
			needToUpdateSceneUniform = true;
		}

		if (ImGui::SliderInt("lightDebugOffset", &lightDebugOffset, 0, gSceneDefault.GetLightCount() - 1))
		{
			gSceneDefault.mSceneUniform.mLightDebugOffset = lightDebugOffset;
			needToUpdateSceneUniform = true;
		}

		if (ImGui::SliderInt("lightDebugCount", &lightDebugCount, 0, gSceneDefault.GetLightCount()))
		{
			gSceneDefault.mSceneUniform.mLightDebugCount = lightDebugCount;
			needToUpdateSceneUniform = true;
		}

		if (ImGui::SliderFloat("fresnel", &fresnel, 0.0f, 1.0f))
		{
			gSceneDefault.mSceneUniform.mFresnel = fresnel;
			needToUpdateSceneUniform = true;
		}

		if (ImGui::SliderFloat("roughness", &roughness, 0.0f, 1.0f))
		{
			gSceneDefault.mSceneUniform.mRoughness = roughness;
			needToUpdateSceneUniform = true;
		}

		if (ImGui::Checkbox("useStandardTextures", &usePerPassTextures))
		{
			gSceneDefault.mSceneUniform.mUsePerPassTextures = usePerPassTextures ? 1 : 0;
			needToUpdateSceneUniform = true;
		}

		if (ImGui::ColorEdit4("standardColor", (float*)&standardColor))
		{
			gSceneDefault.mSceneUniform.mStandardColor = standardColor;
			needToUpdateSceneUniform = true;
		}

		if (ImGui::SliderFloat("metallic", &metallic, 0.0f, 1.0f))
		{
			gSceneDefault.mSceneUniform.mMetallic = metallic;
			needToUpdateSceneUniform = true;
		}

		if (ImGui::SliderFloat("specularity", &specularity, 0.0f, 1.0f))
		{
			gSceneDefault.mSceneUniform.mSpecularity = specularity;
			needToUpdateSceneUniform = true;
		}

		if (ImGui::SliderInt("sampleNumIBL", &sampleNumIBL, 0, 128))
		{
			gSceneDefault.mSceneUniform.mSampleNumIBL = sampleNumIBL;
			needToUpdateSceneUniform = true;
		}

		if (ImGui::SliderInt("showReferenceIBL", &showReferenceIBL, 0, 1))
		{
			gSceneDefault.mSceneUniform.mShowReferenceIBL = showReferenceIBL;
			needToUpdateSceneUniform = true;
		}

		if (ImGui::SliderInt("useSceneLight", &useSceneLight, 0, 1))
		{
			gSceneDefault.mSceneUniform.mUseSceneLight = useSceneLight;
			needToUpdateSceneUniform = true;
		}

		if (ImGui::SliderInt("useSunLight", &useSunLight, 0, 1))
		{
			gSceneDefault.mSceneUniform.mUseSunLight = useSunLight;
			needToUpdateSceneUniform = true;
		}

		if (ImGui::SliderInt("useIBL", &useIBL, 0, 1))
		{
			gSceneDefault.mSceneUniform.mUseIBL = useIBL;
			needToUpdateSceneUniform = true;
		}

		ImGui::Separator();

		if (ImGui::Combo("pathTracerMode", &gPathTracerMode, "off\0default\0progressive\0triangle\0light\0mesh\0albedo\0normal"))
		{
			gSceneDefault.mSceneUniform.mPathTracerMode = gPathTracerMode;
			needToRestartPathTracer = true;
		}

		ImGui::Checkbox("pathTracerForceUpdate", &gPathTracerForceUpdate);

		if (ImGui::SliderInt("pathTracerSPP", &pathTracerSPP, 1, 100))
		{
			gSceneDefault.mSceneUniform.mPathTracerMaxSampleCountPerPixel = pathTracerSPP;
			needToRestartPathTracer = true;
		}

		if (ImGui::SliderInt("pathTracerMinDepth", &pathTracerMinDepth, 0, PATH_TRACER_MIN_DEPTH_MAX))
		{
			gSceneDefault.mSceneUniform.mPathTracerMinDepth = pathTracerMinDepth;
			needToRestartPathTracer = true;
		}

		if (ImGui::SliderInt("pathTracerMaxDepth", &pathTracerMaxDepth, PATH_TRACER_MIN_DEPTH_MAX + 1, PATH_TRACER_MAX_DEPTH_MAX))
		{
			gSceneDefault.mSceneUniform.mPathTracerMaxDepth = pathTracerMaxDepth;
			needToRestartPathTracer = true;
		}

		if (ImGui::Checkbox("pathTracerEnableDebug", &pathTracerEnableDebug))
		{
			gSceneDefault.mSceneUniform.mPathTracerEnableDebug = pathTracerEnableDebug;
			needToUpdateSceneUniform = true;
		}

		if (ImGui::Checkbox("pathTracerUpdateDebug", &pathTracerUpdateDebug))
		{
			gSceneDefault.mSceneUniform.mPathTracerUpdateDebug = pathTracerUpdateDebug;
			needToUpdateSceneUniform = true;
		}
		
		ImGui::Text("[Path Tracer Debug] Debug Pixel Pos (%d,%d)",
			gSceneDefault.mSceneUniform.mPathTracerDebugPixelX,
			gSceneDefault.mSceneUniform.mPathTracerDebugPixelY);
		ImGui::Text("[Path Tracer] (%d/%d) depth (%d/%d) sample per pixel done",
			gSceneDefault.mSceneUniform.mPathTracerCurrentDepth,
			gSceneDefault.mSceneUniform.mPathTracerMaxDepth,
			gSceneDefault.mSceneUniform.mPathTracerCurrentSampleIndex,
			gSceneDefault.mSceneUniform.mPathTracerMaxSampleCountPerPixel);
		ImGui::Text("%.3f ms/frame (%.1f FPS) ", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::Text("Hold C and use mouse to rotate camera.");
		ImGui::End();
	}

	needToRestartPathTracer = needToRestartPathTracer || initOnly;
	needToUpdateSceneUniform = needToUpdateSceneUniform || needToRestartPathTracer;
	
	if (needToRestartPathTracer)
	{
		gSceneDefault.mSceneUniform.mPathTracerCurrentSampleIndex = 0;
		gSceneDefault.mSceneUniform.mPathTracerCurrentDepth = 0;
	}

	if (needToUpdateSceneUniform)
	{
		gSceneDefault.SetUniformDirty();
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

void InitSystems()
{
	ImageBasedLighting::InitIBL(gStoreDefault, gSceneDefault);
	PathTracer::InitPathTracer(gStoreDefault, gSceneDefault);
}

void PrepareSystems()
{
	gCameraMain.SetPassUniformDirty();
	gSceneDefault.SetUniformDirty();
	UpdateResourcesGPU();

	ID3D12GraphicsCommandList* commandList = gRenderer.BeginSingleTimeCommands();

	ImageBasedLighting::PrepareIBL(commandList);
	PathTracer::PreparePathTracer(commandList, gSceneDefault);

	gRenderer.EndSingleTimeCommands(commandList);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	InitConsole();

	// create the window
	if (!InitWindow(hInstance, gWindowWidth, gWindowHeight, nShowCmd, gFullScreen))
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

	// use hard coded data to create the stores and the scene structure
	CreateStores();

	// init systems and update related scene structures
	InitSystems();

	// initialize renderer
	if (!gRenderer.InitRenderer(
		gHwnd, 
		gFrameCount, 
		gMultiSampleCount, 
		gWindowWidth, 
		gWindowHeight,
		gSwapchainColorBufferFormat,
		gSwapchainDepthStencilBufferFormat,
		gRendererDebugMode))
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

	// force flush UI once so that debug data are updated and marked dirty
	UpdateUI(true);

	// prepare systems
	PrepareSystems();

	// start the main loop
	MainLoop();

	// clean up everything
	Cleanup();

	return 0;
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
