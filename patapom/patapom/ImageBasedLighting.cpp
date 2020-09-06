#include "ImageBasedLighting.h"
#include "Level.h"
#include "Scene.h"

Sampler ImageBasedLighting::gSamplerIBL = {
	Sampler::Filter::LINEAR,
	Sampler::Filter::LINEAR,
	Sampler::Filter::LINEAR,
	Sampler::AddressMode::CLAMP,
	Sampler::AddressMode::CLAMP,
	Sampler::AddressMode::CLAMP,
	0.f,
	false,
	1.f,
	false,
	CompareOp::NEVER,
	0.f,
	1.f,
	{0.f, 0.f, 0.f, 0.f}
};

int						ImageBasedLighting::sWidthEnvMap = 1024;
int						ImageBasedLighting::sHeightEnvMap = 512;
int						ImageBasedLighting::sWidthLUT = 1024;
int						ImageBasedLighting::sHeightLUT = 1024;
const int				ImageBasedLighting::sPrefilteredEnvMapMipLevelCount = 8;
vector<Camera>			ImageBasedLighting::sCamerasEnvMap;
Camera					ImageBasedLighting::sCameraLUT(XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), sWidthLUT, sHeightLUT, 45.0f, 100.0f, 0.1f);
Texture					ImageBasedLighting::sEnvMap("probe.jpg", L"IBL env map", gSampler, true, Format::R8G8B8A8_UNORM);
RenderTexture			ImageBasedLighting::sPrefilteredEnvMap(L"IBL prefiltered env map", sWidthEnvMap, sHeightEnvMap, sPrefilteredEnvMapMipLevelCount, ReadFrom::COLOR, gSamplerIBL, Format::R16G16B16A16_FLOAT, XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
RenderTexture			ImageBasedLighting::sLUT(L"IBL LUT", sWidthLUT, sHeightLUT, 1, ReadFrom::COLOR, gSamplerIBL, Format::R16G16B16A16_FLOAT, XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
vector<PassIBL>			ImageBasedLighting::sPrefilterEnvMapPasses;
PassIBL					ImageBasedLighting::sPrepareLutPass(L"IBL prepare LUT", true, false);
Shader					ImageBasedLighting::sPrefilterEnvMapPS(Shader::ShaderType::PIXEL_SHADER, L"ps_prefilterEnvMap.hlsl");
Shader					ImageBasedLighting::sPrepareLutPS(Shader::ShaderType::PIXEL_SHADER, L"ps_prepareLUT.hlsl");

void ImageBasedLighting::CreateIBL(Level& level, Scene& scene)
{
	sCamerasEnvMap.resize(sPrefilteredEnvMapMipLevelCount);
	sPrefilterEnvMapPasses.resize(sPrefilteredEnvMapMipLevelCount);

	for (int i = 0; i < sPrefilterEnvMapPasses.size(); i++)
	{
		wstring passName = L"IBL prefilter env map pass ";
		passName = passName + to_wstring(i);
		sCamerasEnvMap[i] = Camera(XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), sPrefilteredEnvMap.GetWidth(i), sPrefilteredEnvMap.GetHeight(i), 45.0f, 100.0f, 0.1f);
		sPrefilterEnvMapPasses[i] = PassIBL(passName, true, false);
		sPrefilterEnvMapPasses[i].mPassUniform.mRoughness = (float)i / (sPrefilterEnvMapPasses.size() - 1.0f);
		sPrefilterEnvMapPasses[i].SetCamera(&sCamerasEnvMap[i]);
		sPrefilterEnvMapPasses[i].AddMesh(&gFullscreenTriangle);
		sPrefilterEnvMapPasses[i].AddShader(&gDeferredVS);
		sPrefilterEnvMapPasses[i].AddShader(&sPrefilterEnvMapPS);
		sPrefilterEnvMapPasses[i].AddTexture(&sEnvMap);
		sPrefilterEnvMapPasses[i].AddRenderTexture(&sPrefilteredEnvMap, i, DepthStencilState::None());
		scene.AddPass(&sPrefilterEnvMapPasses[i]);
		level.AddPass(&sPrefilterEnvMapPasses[i]);
		level.AddShader(&sPrefilterEnvMapPS);
		level.AddCamera(&sCamerasEnvMap[i]);
	}

	sPrepareLutPass.SetCamera(&sCameraLUT);
	sPrepareLutPass.AddMesh(&gFullscreenTriangle);
	sPrepareLutPass.AddShader(&gDeferredVS);
	sPrepareLutPass.AddShader(&sPrepareLutPS);
	sPrepareLutPass.AddRenderTexture(&sLUT, 0, DepthStencilState::None());
	scene.AddPass(&sPrepareLutPass);
	level.AddPass(&sPrepareLutPass);
	level.AddShader(&sPrepareLutPS);
	level.AddCamera(&sCameraLUT);

	level.AddTexture(&sEnvMap);
	level.AddTexture(&sPrefilteredEnvMap);
	level.AddTexture(&sLUT);
}

void ImageBasedLighting::PrepareIBL(ID3D12GraphicsCommandList* commandList)
{
	sPrefilteredEnvMap.MakeReadyToWrite(commandList);
	for (int i = 0; i < sPrefilteredEnvMapMipLevelCount; i++)
	{
		gRenderer.RecordPass(sPrefilterEnvMapPasses[i], commandList, true, false, false);
	}
	sPrefilteredEnvMap.MakeReadyToRead(commandList);

	sLUT.MakeReadyToWrite(commandList);
	gRenderer.RecordPass(sPrepareLutPass, commandList, true, false, false);
	sLUT.MakeReadyToRead(commandList);
}

void ImageBasedLighting::Shutdown()
{

}
