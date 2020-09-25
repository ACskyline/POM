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
int						ImageBasedLighting::sHeightEnvMap = 1024;
int						ImageBasedLighting::sWidthLUT = 1024;
int						ImageBasedLighting::sHeightLUT = 1024;
const int				ImageBasedLighting::sPrefilteredEnvMapMipLevelCount = 8;
vector<Camera>			ImageBasedLighting::sCamerasEnvMap;
Camera					ImageBasedLighting::sCameraLUT(XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), sWidthLUT, sHeightLUT, 45.0f, 100.0f, 0.1f);
Texture					ImageBasedLighting::sEnvMap("probe.jpg", L"IBL env map", gSamplerIBL, true, Format::R8G8B8A8_UNORM);
RenderTexture			ImageBasedLighting::sPrefilteredEnvMap(TextureType::CUBE, L"IBL prefiltered env map", sWidthEnvMap, sHeightEnvMap, 6, sPrefilteredEnvMapMipLevelCount, ReadFrom::COLOR, gSamplerIBL, Format::R16G16B16A16_FLOAT, XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
RenderTexture			ImageBasedLighting::sLUT(TextureType::DEFAULT, L"IBL LUT", sWidthLUT, sHeightLUT, 1, 1, ReadFrom::COLOR, gSamplerIBL, Format::R16G16B16A16_FLOAT, XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
vector<PassIBL>			ImageBasedLighting::sPrefilterEnvMapPasses[Texture::CubeFaces::COUNT];
PassIBL					ImageBasedLighting::sPrepareLutPass(L"IBL prepare LUT", true, false);
Shader					ImageBasedLighting::sPrefilterEnvMapPS(Shader::ShaderType::PIXEL_SHADER, L"ps_prefilterEnvMap.hlsl");
Shader					ImageBasedLighting::sPrepareLutPS(Shader::ShaderType::PIXEL_SHADER, L"ps_prepareLUT.hlsl");

void ImageBasedLighting::CreateIBL(Level& level, Scene& scene)
{
	sCamerasEnvMap.resize(sPrefilteredEnvMapMipLevelCount);

	for (int i = 0; i < Texture::CubeFaces::COUNT; i++)
		sPrefilterEnvMapPasses[i].resize(sPrefilteredEnvMapMipLevelCount);

	for (int i = 0; i < Texture::CubeFaces::COUNT; i++)
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
			level.AddPass(&sPrefilterEnvMapPasses[i][j]);
			level.AddShader(&sPrefilterEnvMapPS);
			level.AddCamera(&sCamerasEnvMap[j]);
		}
	}

	sPrepareLutPass.SetCamera(&sCameraLUT);
	sPrepareLutPass.AddMesh(&gFullscreenTriangle);
	sPrepareLutPass.AddShader(&gDeferredVS);
	sPrepareLutPass.AddShader(&sPrepareLutPS);
	sPrepareLutPass.AddRenderTexture(&sLUT, 0, 0, DepthStencilState::None());
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
	for (int i = 0; i < Texture::CubeFaces::COUNT; i++)
	{
		for (int j = 0; j < sPrefilterEnvMapPasses[i].size(); j++)
		{
			gRenderer.RecordPass(sPrefilterEnvMapPasses[i][j], commandList, true, false, false);
		}
	}
	sPrefilteredEnvMap.MakeReadyToRead(commandList);

	sLUT.MakeReadyToWrite(commandList);
	gRenderer.RecordPass(sPrepareLutPass, commandList, true, false, false);
	sLUT.MakeReadyToRead(commandList);
}

void ImageBasedLighting::Shutdown()
{

}
