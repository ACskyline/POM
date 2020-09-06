#pragma once

#include "Texture.h"
#include "Pass.h"


struct PassUniformIBL : PassUniformDefault
{
	float mRoughness;
};

typedef PassCommon<PassUniformIBL> PassIBL;

class ImageBasedLighting
{
public:
	static int sWidthEnvMap;
	static int sHeightEnvMap;
	static int sWidthLUT;
	static int sHeightLUT;
	static const int sPrefilteredEnvMapMipLevelCount;
	static vector<Camera> sCamerasEnvMap;
	static Camera sCameraLUT;
	static Sampler gSamplerIBL;
	static Texture sEnvMap;
	static RenderTexture sPrefilteredEnvMap;
	static RenderTexture sLUT;
	static vector<PassIBL> sPrefilterEnvMapPasses;
	static PassIBL sPrepareLutPass;
	static Shader sPrefilterEnvMapPS;
	static Shader sPrepareLutPS;
	static void CreateIBL(Level& level, Scene& scene);
	static void PrepareIBL(ID3D12GraphicsCommandList* commandList);
	static void Shutdown();
};
