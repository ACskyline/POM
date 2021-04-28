#pragma once

#include "Texture.h"
#include "Pass.h"

typedef PassCommon<PassUniformIBL> PassIBL;

class ImageBasedLighting
{
public:
	static const int sWidthEnvMap;
	static const int sHeightEnvMap;
	static const int sWidthLUT;
	static const int sHeightLUT;
	static const int sPrefilteredEnvMapMipLevelCount;
	static vector<Camera> sCamerasEnvMap;
	static Camera sCameraLUT;
	static Sampler gSamplerIBL;
	static Texture sEnvMap;
	static RenderTexture sPrefilteredEnvMap;
	static RenderTexture sLUT;
	static vector<PassIBL> sPrefilterEnvMapPasses[Texture::CubeFace::COUNT];
	static PassIBL sPrepareLutPass;
	static Shader sPrefilterEnvMapPS;
	static Shader sPrepareLutPS;
	static void InitIBL(Store& store, Scene& scene);
	static void PrepareIBL(ID3D12GraphicsCommandList* commandList);
	static void Shutdown();
};
