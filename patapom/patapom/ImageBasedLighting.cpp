#include "ImageBasedLighting.h"
#include "Store.h"
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

const int				ImageBasedLighting::sWidthEnvMap = 1024;
const int				ImageBasedLighting::sHeightEnvMap = 1024;
const int				ImageBasedLighting::sWidthLUT = 1024;
const int				ImageBasedLighting::sHeightLUT = 1024;
const int				ImageBasedLighting::sPrefilteredEnvMapMipLevelCount = 8;
vector<Camera>			ImageBasedLighting::sCamerasEnvMap;
Camera					ImageBasedLighting::sCameraLUT(XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), sWidthLUT, sHeightLUT, 45.0f, 100.0f, 0.1f);
Texture					ImageBasedLighting::sEnvMap("probe.jpg", L"IBL env map", gSamplerIBL, true, Format::R8G8B8A8_UNORM);
RenderTexture			ImageBasedLighting::sPrefilteredEnvMap(TextureType::CUBE, L"IBL prefiltered env map", sWidthEnvMap, sHeightEnvMap, 6, sPrefilteredEnvMapMipLevelCount, ReadFrom::COLOR, gSamplerIBL, Format::R16G16B16A16_FLOAT, XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
RenderTexture			ImageBasedLighting::sLUT(TextureType::DEFAULT, L"IBL LUT", sWidthLUT, sHeightLUT, 1, 1, ReadFrom::COLOR, gSamplerIBL, Format::R16G16B16A16_FLOAT, XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
vector<PassIBL>			ImageBasedLighting::sPrefilterEnvMapPasses[Texture::CubeFace::COUNT];
PassIBL					ImageBasedLighting::sPrepareLutPass(L"IBL prepare LUT", true, false);
Shader					ImageBasedLighting::sPrefilterEnvMapPS(Shader::ShaderType::PIXEL_SHADER, L"ps_prefilterEnvMap.hlsl");
Shader					ImageBasedLighting::sPrepareLutPS(Shader::ShaderType::PIXEL_SHADER, L"ps_prepareLUT.hlsl");

void ImageBasedLighting::InitIBL(Store& store, Scene& scene)
{
	sCamerasEnvMap.resize(sPrefilteredEnvMapMipLevelCount);

	for (int i = 0; i < Texture::CubeFace::COUNT; i++)
		sPrefilterEnvMapPasses[i].resize(sPrefilteredEnvMapMipLevelCount);

	for (int i = 0; i < Texture::CubeFace::COUNT; i++)
	{
		for (int j = 0; j < sPrefilterEnvMapPasses[i].size(); j++)
		{
			wstring passName = L"IBL prefilter env map pass, face:";
			passName = passName + to_wstring(i) + L", mip:" + to_wstring(j);
			sCamerasEnvMap[j] = Camera(XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), sPrefilteredEnvMap.GetWidth(j), sPrefilteredEnvMap.GetHeight(j), 45.0f, 100.0f, 0.1f);
			sPrefilterEnvMapPasses[i][j] = PassIBL(passName, true, false);
			sPrefilterEnvMapPasses[i][j].mPassUniform.mRoughness = (float)j / (sPrefilterEnvMapPasses[i].size() - 1.0f);
			sPrefilterEnvMapPasses[i][j].mPassUniform.mFaceIndex = i;
			sPrefilterEnvMapPasses[i][j].SetCamera(&sCamerasEnvMap[j]);
			sPrefilterEnvMapPasses[i][j].AddMesh(&gFullscreenTriangle);
			sPrefilterEnvMapPasses[i][j].AddShader(&gDeferredVS);
			sPrefilterEnvMapPasses[i][j].AddShader(&sPrefilterEnvMapPS);
			sPrefilterEnvMapPasses[i][j].AddTexture(&sEnvMap);
			sPrefilterEnvMapPasses[i][j].AddRenderTexture(&sPrefilteredEnvMap, i, j, DepthStencilState::None());
			scene.AddPass(&sPrefilterEnvMapPasses[i][j]);
			store.AddPass(&sPrefilterEnvMapPasses[i][j]);
			store.AddCamera(&sCamerasEnvMap[j]);
		}
	}

	store.AddShader(&sPrefilterEnvMapPS);
	sPrepareLutPass.SetCamera(&sCameraLUT);
	sPrepareLutPass.AddMesh(&gFullscreenTriangle);
	sPrepareLutPass.AddShader(&gDeferredVS);
	sPrepareLutPass.AddShader(&sPrepareLutPS);
	sPrepareLutPass.AddRenderTexture(&sLUT, 0, 0, DepthStencilState::None());
	scene.AddPass(&sPrepareLutPass);
	store.AddPass(&sPrepareLutPass);
	store.AddShader(&sPrepareLutPS);
	store.AddCamera(&sCameraLUT);

	store.AddTexture(&sEnvMap);
	store.AddTexture(&sPrefilteredEnvMap);
	store.AddTexture(&sLUT);
}

void ImageBasedLighting::PrepareIBL(ID3D12GraphicsCommandList* commandList)
{
	sPrefilteredEnvMap.MakeReadyToRender(commandList);
	for (int i = 0; i < Texture::CubeFace::COUNT; i++)
	{
		for (int j = 0; j < sPrefilterEnvMapPasses[i].size(); j++)
		{
			gRenderer.RecordPass(sPrefilterEnvMapPasses[i][j], commandList, true, false, false);
		}
	}
	sPrefilteredEnvMap.MakeReadyToRead(commandList);

	sLUT.MakeReadyToRender(commandList);
	gRenderer.RecordPass(sPrepareLutPass, commandList, true, false, false);
	sLUT.MakeReadyToRead(commandList);
}

void ImageBasedLighting::Shutdown()
{

}
