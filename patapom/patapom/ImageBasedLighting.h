#pragma once

#include "Texture.h"
#include "Pass.h"


struct PassUniformIBL : PassUniformDefault
{
	float mRoughness;
	u32 mFaceIndex;
	u32 PADDING_0;
	u32 PADDING_1;
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
	static vector<PassIBL> sPrefilterEnvMapPasses[Texture::CubeFaces::COUNT];
	static PassIBL sPrepareLutPass;
	static Shader sPrefilterEnvMapPS;
	static Shader sPrepareLutPS;
	static void CreateIBL(Level& level, Scene& scene);
	static void PrepareIBL(ID3D12GraphicsCommandList* commandList);
	static void Shutdown();
};
