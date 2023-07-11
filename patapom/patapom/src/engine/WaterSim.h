#pragma once

#include "Pass.h"

typedef PassCommon<PassUniformWaterSim> PassWaterSim;

class WaterSim
{
public:
	enum WaterSimMode { FLIP, FLIP_STEP, MPM, MPM_STEP, WATERSIM_MODE_COUNT };
	enum WaterSimPassType { 
		P2G, 
		P2G_RASTERIZER,
		PRE_UPDATE_GRID, 
		UPDATE_GRID,
		PROJECT,
		G2P,
		CLEAR,
		RASTERIZER,
		RASTERIZER_CS,
		BLIT,
		RESET_PARTICLES,
		RESET_GRID,
		DEBUG_LINE,
		DEBUG_CUBE,
		WATERSIM_PASSTYPE_COUNT,
	};
	static PassWaterSim sPassWaterSim[WATERSIM_PASSTYPE_COUNT];
	static Shader sWaterSimP2GCS;
	static Shader sWaterSimP2GVS;
	static Shader sWaterSimP2GPS;
	static Shader sWaterSimPreUpdateGridCS;
	static Shader sWaterSimUpdateGridCS;
	static Shader sWaterSimProjectCS;
	static Shader sWaterSimG2PCS;
	static Shader sWaterSimClearMinMaxBufferCS;
	static Shader sWaterSimRasterizerVS;
	static Shader sWaterSimRasterizerPS;
	static Shader sWaterSimRasterizerCS;
	static Shader sWaterSimBlitBackbufferPS;
	static Shader sWaterSimResetParticlesCS;
	static Shader sWaterSimResetGridCS;
	static Shader sWaterSimDebugLineVS;
	static Shader sWaterSimDebugLinePS;
	static Shader sWaterSimDebugCubeVS;
	static Shader sWaterSimDebugCubePS;
	static Mesh sWaterSimDebugMeshLine;
	static Mesh sWaterSimDebugMeshCube;
	static Mesh sWaterSimParticleMesh;
	static const int sCellCountX;
	static const int sCellCountY;
	static const int sCellCountZ;
	static const int sCellFaceCountX;
	static const int sCellFaceCountY;
	static const int sCellFaceCountZ;
	static const int sCellFaceThreadGroupCount;
	static const int sParticleCount;
	static const int sParticleThreadGroupCount;
	static const int sBackbufferWidth;
	static const int sBackbufferHeight;
	static const int sCellRenderTextureWidth;
	static const int sCellRenderTextureHeight;
	static const float sCellSize;
	static XMFLOAT3 sParticleSpawnSourcePos;
	static XMFLOAT3 sParticleSpawnSourceSpan;
	static XMFLOAT3 sExplosionPos;
	static XMFLOAT3 sExplosionForceScale;
	static WaterSimMode sWaterSimMode;
	static bool sUseComputeRasterizer;
	static bool sUseRasterizerP2G;
	static bool sApplyExplosion;
	static int sAliveParticleCount;
#if WATERSIM_DEBUG
	static int sDebugRasterizerParticleCount;
#endif
	static float sTimeStepScale;
	static int sSubStepCount;
	static bool sApplyForce;
	static float sGravitationalAccel;
	static float sWaterDensity;
	static float sFlipScale;
	static bool sWarmStart;
	static bool sEnableDebugCell;
	static bool sEnableDebugCellVelocity;
	static int sJacobiIterationCount;
	static float sYoungModulus;
	static float sPoissonRatio;
	static WriteBuffer sCellBuffer;
	static WriteBuffer sCellBufferTemp;
	static WriteBuffer sCellFaceBuffer;
	static WriteBuffer sCellFaceBufferTemp;
	static WriteBuffer sCellAuxBuffer;
	static WriteBuffer sParticleBuffer;
	static RenderTexture sDebugBackbuffer;
	static RenderTexture sDepthBufferMax;
	static RenderTexture sDepthBufferMin;
	static RenderTexture sCellRenderTexture;
	static std::vector<WaterSimParticle> sParticles;
	static std::vector<WaterSimCellFace> sCellFaces;
	static std::vector<WaterSimCell> sCells;
	static void InitWaterSim(Store& store, Scene& scene);
	static void PrepareWaterSim(CommandList commandList);
	static void WaterSimResetGrid(CommandList commandList);
	static void WaterSimResetParticles(CommandList commandList);
	static void WaterSimResetParticlesMPM(CommandList commandList);
	static void WaterSimStepOnce(CommandList commandList);
	static void WaterSimStepOnceMPM(CommandList commandList);
	static void WaterSimRasterize(CommandList commandList);
	static void SetTimeStepScale(float timeStep);
	static void SetApplyForce(bool applyForce);
	static void SetG(float G);
	static void SetWaterDensity(float density);
	static void SetFlipScale(float flipScale);
	static void SetAliveParticleCount(int particleCount);
	static void SetWarmStart(bool warmStart);
	static void SetJacobiIterationCount(int jacobiIterationCount);
	static void SetExplosionPos(XMFLOAT3 explosionCellIndexXYZ);
	static void SetExplosionForceScale(XMFLOAT3 explosionForceScale);
	static void SetApplyExplosion(bool applyExplosion);
	static void SetWaterSimMode(int mode);
	static void SetUseRasterizerP2G(bool useRasterizerP2G);
	static void SetWaterSimYoungModulus(float young);
	static void SetWaterSimPoissonRatio(float poisson);
	static int GetCellCount() { return sCellCountX * sCellCountY * sCellCountZ; }
};
