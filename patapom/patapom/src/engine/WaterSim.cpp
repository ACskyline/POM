#include "WaterSim.h"
#include "SharedHeader.h"
#include "Store.h"
#include "Scene.h"
#include "Buffer.h"
#include "Texture.h"
#include "Mesh.h"
#include "DeferredLighting.h"
#include <cmath>

CommandLineArg PARAM_sanitizeWaterSim("-sanitizeWaterSim");

PassWaterSim WaterSim::sPassWaterSim[WaterSim::WATERSIM_PASSTYPE_COUNT];
Shader WaterSim::sWaterSimP2GCS(Shader::ShaderType::COMPUTE_SHADER, "cs_watersim_p2g");
Shader WaterSim::sWaterSimP2GVS(Shader::ShaderType::VERTEX_SHADER, "vs_watersim_p2g");
Shader WaterSim::sWaterSimP2GPS(Shader::ShaderType::PIXEL_SHADER, "ps_watersim_p2g");
Shader WaterSim::sWaterSimPreUpdateGridCS(Shader::ShaderType::COMPUTE_SHADER, "cs_watersim_preupdategrid");
Shader WaterSim::sWaterSimUpdateGridCS(Shader::ShaderType::COMPUTE_SHADER, "cs_watersim_updategrid");
Shader WaterSim::sWaterSimProjectCS(Shader::ShaderType::COMPUTE_SHADER, "cs_watersim_project");
Shader WaterSim::sWaterSimG2PCS(Shader::ShaderType::COMPUTE_SHADER, "cs_watersim_g2p");
Shader WaterSim::sWaterSimClearMinMaxBufferCS(Shader::ShaderType::COMPUTE_SHADER, "cs_watersim_clearminmaxbuffer");
Shader WaterSim::sWaterSimRasterizerVS(Shader::ShaderType::VERTEX_SHADER, "vs_watersim_rasterizer");
Shader WaterSim::sWaterSimRasterizerPS(Shader::ShaderType::PIXEL_SHADER, "ps_watersim_rasterizer");
Shader WaterSim::sWaterSimRasterizerCS(Shader::ShaderType::COMPUTE_SHADER, "cs_watersim_rasterizer");
Shader WaterSim::sWaterSimBlitBackbufferPS(Shader::ShaderType::PIXEL_SHADER, "ps_watersim_blitbackbuffer");
Shader WaterSim::sWaterSimResetParticlesCS(Shader::ShaderType::COMPUTE_SHADER, "cs_watersim_resetparticles");
Shader WaterSim::sWaterSimPreUpdateParticlesCS(Shader::ShaderType::COMPUTE_SHADER, "cs_watersim_preupdateparticles");
Shader WaterSim::sWaterSimUpdateParticlesCS(Shader::ShaderType::COMPUTE_SHADER, "cs_watersim_updateparticles");
Shader WaterSim::sWaterSimResetGridCS(Shader::ShaderType::COMPUTE_SHADER, "cs_watersim_resetgrid");
Shader WaterSim::sWaterSimDebugLineVS(Shader::ShaderType::VERTEX_SHADER, "vs_watersim_debug_line");
Shader WaterSim::sWaterSimDebugLinePS(Shader::ShaderType::PIXEL_SHADER, "ps_watersim_debug_line");
Shader WaterSim::sWaterSimDebugCubeVS(Shader::ShaderType::VERTEX_SHADER, "vs_watersim_debug_cube");
Shader WaterSim::sWaterSimDebugCubePS(Shader::ShaderType::PIXEL_SHADER, "ps_watersim_debug_cube");
Mesh WaterSim::sWaterSimDebugMeshLine("water sim debug mesh line", Mesh::MeshType::LINE, XMFLOAT3(0, 0, 0), XMFLOAT3(0, 0, 0), XMFLOAT3(1, 1, 1));
Mesh WaterSim::sWaterSimDebugMeshCube("water sim debug mesh cube", Mesh::MeshType::CUBE, XMFLOAT3(0, 0, 0), XMFLOAT3(0, 0, 0), XMFLOAT3(1, 1, 1));
Mesh WaterSim::sWaterSimParticleMesh("particle mesh", Mesh::MeshType::MESH, XMFLOAT3(0, 1, 0), XMFLOAT3(0, 45, 0), XMFLOAT3(1.0f, 1.0f, 1.0f), "ball.obj");
const int WaterSim::sCellCountX = WATERSIM_CELL_COUNT_X;
const int WaterSim::sCellCountY = WATERSIM_CELL_COUNT_Y;
const int WaterSim::sCellCountZ = WATERSIM_CELL_COUNT_Z;
const int WaterSim::sCellFaceCountX = WATERSIM_CELLFACE_COUNT_X;
const int WaterSim::sCellFaceCountY = WATERSIM_CELLFACE_COUNT_Y;
const int WaterSim::sCellFaceCountZ = WATERSIM_CELLFACE_COUNT_Z;
const int WaterSim::sCellFaceThreadGroupCount = ceil((WaterSim::sCellFaceCountX * WaterSim::sCellFaceCountY * WaterSim::sCellFaceCountZ) / (float)(WATERSIM_THREAD_PER_THREADGROUP_X * WATERSIM_THREAD_PER_THREADGROUP_Y * WATERSIM_THREAD_PER_THREADGROUP_Z));
const int WaterSim::sParticleCount = WATERSIM_PARTICLE_COUNT_MAX;
const int WaterSim::sParticleThreadGroupCount = ceil(sParticleCount / (float)(WATERSIM_PARTICLE_THREAD_PER_THREADGROUP_X * WATERSIM_PARTICLE_THREAD_PER_THREADGROUP_Y * WATERSIM_PARTICLE_THREAD_PER_THREADGROUP_Z));
const int WaterSim::sBackbufferWidth = WATERSIM_BACKBUFFER_WIDTH;
const int WaterSim::sBackbufferHeight = WATERSIM_BACKBUFFER_HEIGHT;
const int WaterSim::sCellRenderTextureWidth = WATERSIM_CELL_RT_WIDTH;
const int WaterSim::sCellRenderTextureHeight = WATERSIM_CELL_RT_HEIGHT;
const float WaterSim::sCellSize = 1.0f;
XMFLOAT3 WaterSim::sParticleSpawnSourcePos;
XMFLOAT3 WaterSim::sParticleSpawnSourceSpan;
XMFLOAT3 WaterSim::sExplosionPos;
XMFLOAT3 WaterSim::sExplosionForceScale = {100.0f, 100.0f, 100.0f};
WaterSim::WaterSimMode WaterSim::sWaterSimMode = WaterSim::MPM;
bool WaterSim::sUseComputeRasterizer = false;
bool WaterSim::sUseRasterizerP2G = true;
bool WaterSim::sApplyExplosion = false;
int WaterSim::sAliveParticleCount = 256;
float WaterSim::sTimeStepScale = 1.0f;
int WaterSim::sSubStepCount = 1; // 5 is for flip
bool WaterSim::sApplyForce = true;
float WaterSim::sGravitationalAccel = 9.86f;
float WaterSim::sWaterDensity = 1.0f;
float WaterSim::sFlipScale = 0.001f;
bool WaterSim::sWarmStart = true;
bool WaterSim::sEnableDebugCell = false;
bool WaterSim::sEnableDebugCellVelocity = false;
int WaterSim::sJacobiIterationCount = 8;
WriteBuffer WaterSim::sCellBuffer("water sim cell buffer", sizeof(WaterSimCell), sCellCountX * sCellCountY * sCellCountZ);
WriteBuffer WaterSim::sCellBufferTemp("water sim cell buffer temp", sizeof(WaterSimCell), sCellCountX * sCellCountY * sCellCountZ);
WriteBuffer WaterSim::sCellFaceBuffer("water sim cell face buffer", sizeof(WaterSimCellFace), sCellFaceCountX * sCellFaceCountY * sCellFaceCountZ * 3);
WriteBuffer WaterSim::sCellFaceBufferTemp("water sim cell face buffer temp", sizeof(WaterSimCellFace), sCellFaceCountX * sCellFaceCountY * sCellFaceCountZ * 3);
WriteBuffer WaterSim::sCellAuxBuffer("water sim cell aux buffer", sizeof(WaterSimCellAux), sCellCountX * sCellCountY * sCellCountZ);
WriteBuffer WaterSim::sParticleBuffer("water particle buffer", sizeof(WaterSimParticle), sParticleCount);
// use uint buffer for atmoic operations
RenderTexture WaterSim::sDepthBufferMax(TextureType::TEX_2D, "water sim depth buffer max", sBackbufferWidth, sBackbufferHeight, 1, 1, ReadFrom::COLOR, Format::R32_UINT, XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
RenderTexture WaterSim::sDepthBufferMin(TextureType::TEX_2D, "water sim depth buffer min", sBackbufferWidth, sBackbufferHeight, 1, 1, ReadFrom::COLOR, Format::R32_UINT, XMFLOAT4(WATERSIM_DEPTHBUFFER_MAX, 0.0f, 0.0f, 0.0f));
RenderTexture WaterSim::sDebugBackbuffer(TextureType::TEX_2D, "water sim debug backbuffer", sBackbufferWidth, sBackbufferHeight, 1, 1, ReadFrom::COLOR, Format::R16G16B16A16_UNORM, XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
RenderTexture WaterSim::sCellRenderTexture(TextureType::TEX_2D, "water sim cell rt", sCellRenderTextureWidth, sCellRenderTextureHeight, 1, 1, ReadFrom::COLOR, Format::R16G16B16A16_FLOAT, XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
std::vector<WaterSimParticle> WaterSim::sParticles;
std::vector<WaterSimCellFace> WaterSim::sCellFaces;
std::vector<WaterSimCell> WaterSim::sCells;

void WaterSim::InitWaterSim(Store& store, Scene& scene)
{
	sPassWaterSim[P2G].CreatePass("p2g");
	sPassWaterSim[P2G].AddShader(&sWaterSimP2GCS);
	sPassWaterSim[P2G].AddWriteBuffer(&sCellBuffer);
	sPassWaterSim[P2G].AddWriteBuffer(&sCellFaceBuffer);
	sPassWaterSim[P2G].AddWriteBuffer(&sParticleBuffer);

	// TODO: we actually need point rasterization, one point for one particle
	sPassWaterSim[P2G_RASTERIZER].CreatePass("p2g rasterizer", true, false, false, PrimitiveType::POINT);
	sPassWaterSim[P2G_RASTERIZER].AddShader(&sWaterSimP2GVS);
	sPassWaterSim[P2G_RASTERIZER].AddShader(&sWaterSimP2GPS);
	sPassWaterSim[P2G_RASTERIZER].AddBuffer(&sParticleBuffer);
	sPassWaterSim[P2G_RASTERIZER].AddRenderTexture(&sCellRenderTexture, 0, 0, BlendState::AdditiveBlend());

	sPassWaterSim[PRE_UPDATE_GRID].CreatePass("preupdate grid");
	sPassWaterSim[PRE_UPDATE_GRID].AddShader(&sWaterSimPreUpdateGridCS);
	sPassWaterSim[PRE_UPDATE_GRID].AddWriteBuffer(&sCellBuffer);
	sPassWaterSim[PRE_UPDATE_GRID].AddWriteBuffer(&sCellFaceBuffer);
	sPassWaterSim[PRE_UPDATE_GRID].AddWriteBuffer(&sCellFaceBufferTemp);

	sPassWaterSim[UPDATE_GRID].CreatePass("update grid");
	sPassWaterSim[UPDATE_GRID].AddShader(&sWaterSimUpdateGridCS);
	sPassWaterSim[UPDATE_GRID].AddBuffer(&sCellFaceBufferTemp);
	sPassWaterSim[UPDATE_GRID].AddWriteBuffer(&sCellBuffer);
	sPassWaterSim[UPDATE_GRID].AddWriteBuffer(&sCellFaceBuffer);
	sPassWaterSim[UPDATE_GRID].AddWriteTexture(&sCellRenderTexture, 0);

	sPassWaterSim[PROJECT].CreatePass("project");
	sPassWaterSim[PROJECT].AddShader(&sWaterSimProjectCS);
	sPassWaterSim[PROJECT].AddWriteBuffer(&sCellBuffer);
	sPassWaterSim[PROJECT].AddWriteBuffer(&sCellBufferTemp);
	sPassWaterSim[PROJECT].AddWriteBuffer(&sCellFaceBuffer);
	sPassWaterSim[PROJECT].AddWriteBuffer(&sCellAuxBuffer);

	sPassWaterSim[G2P].CreatePass("g2p");
	sPassWaterSim[G2P].AddShader(&sWaterSimG2PCS);
	sPassWaterSim[G2P].AddBuffer(&sCellFaceBuffer);
	sPassWaterSim[G2P].AddWriteBuffer(&sCellBuffer);
	sPassWaterSim[G2P].AddWriteBuffer(&sParticleBuffer);
	sPassWaterSim[G2P].AddWriteTexture(&sCellRenderTexture, 0);

	// clear min max buffer
	sPassWaterSim[CLEAR].CreatePass("water sim clear");
	sPassWaterSim[CLEAR].AddShader(&sWaterSimClearMinMaxBufferCS);
	sPassWaterSim[CLEAR].AddWriteTexture(&sDepthBufferMax, 0);
	sPassWaterSim[CLEAR].AddWriteTexture(&sDepthBufferMin, 0);
	sPassWaterSim[CLEAR].AddWriteTexture(&sDebugBackbuffer, 0);

	// particle rasterizer
	sPassWaterSim[RASTERIZER].CreatePass("rasterizer");
	sPassWaterSim[RASTERIZER].SetCamera(&gCameraMain);
	sPassWaterSim[RASTERIZER].AddMesh(&sWaterSimParticleMesh);
	sPassWaterSim[RASTERIZER].AddShader(&sWaterSimRasterizerVS);
	sPassWaterSim[RASTERIZER].AddShader(&sWaterSimRasterizerPS);
	sPassWaterSim[RASTERIZER].AddBuffer(&sParticleBuffer);
	sPassWaterSim[RASTERIZER].AddRenderTexture(&DeferredLighting::gRenderTextureDbuffer, 0, 0, DS_EQUAL_NO_WRITE_REVERSED_Z_SWITCH);

	// particle rasterizer CS
	sPassWaterSim[RASTERIZER_CS].CreatePass("rasterizerCS");
	sPassWaterSim[RASTERIZER_CS].SetCamera(&gCameraMain);
	sPassWaterSim[RASTERIZER_CS].AddShader(&sWaterSimRasterizerCS);
	sPassWaterSim[RASTERIZER_CS].AddBuffer(&sParticleBuffer);
	sPassWaterSim[RASTERIZER_CS].AddWriteTexture(&sDepthBufferMax, 0);
	sPassWaterSim[RASTERIZER_CS].AddWriteTexture(&sDepthBufferMin, 0);

	// blit water sim backbuffer
	sPassWaterSim[BLIT].CreatePass("water sim blit", true, false);
	sPassWaterSim[BLIT].SetCamera(&gCameraMain);
	sPassWaterSim[BLIT].AddMesh(&gFullscreenTriangle);
	sPassWaterSim[BLIT].AddShader(&DeferredLighting::gDeferredVS);
	sPassWaterSim[BLIT].AddShader(&sWaterSimBlitBackbufferPS);
	sPassWaterSim[BLIT].AddTexture(&sDepthBufferMax);
	sPassWaterSim[BLIT].AddTexture(&sDepthBufferMin);
	sPassWaterSim[BLIT].AddTexture(&sDebugBackbuffer);
	sPassWaterSim[BLIT].AddRenderTexture(nullptr, 0, 0, BlendState::AlphaBlend());

	// reset grid
	sPassWaterSim[RESET_GRID].CreatePass("water sim reset grid");
	sPassWaterSim[RESET_GRID].AddShader(&sWaterSimResetGridCS);
	sPassWaterSim[RESET_GRID].AddWriteBuffer(&sCellBuffer);
	sPassWaterSim[RESET_GRID].AddWriteBuffer(&sCellBufferTemp);
	sPassWaterSim[RESET_GRID].AddWriteBuffer(&sCellFaceBuffer);
	sPassWaterSim[RESET_GRID].AddWriteBuffer(&sCellFaceBufferTemp);
	sPassWaterSim[RESET_GRID].AddWriteTexture(&sCellRenderTexture, 0);

	// reset particles
	sPassWaterSim[RESET_PARTICLES].CreatePass("water sim reset particles");
	sPassWaterSim[RESET_PARTICLES].AddShader(&sWaterSimResetParticlesCS);
	sPassWaterSim[RESET_PARTICLES].AddWriteBuffer(&sCellBuffer);
	sPassWaterSim[RESET_PARTICLES].AddWriteBuffer(&sParticleBuffer);

	// preupdate particles
	sPassWaterSim[PRE_UPDATE_PARTICLES].CreatePass("water sim preupdate particles");
	sPassWaterSim[PRE_UPDATE_PARTICLES].AddShader(&sWaterSimPreUpdateParticlesCS);
	sPassWaterSim[PRE_UPDATE_PARTICLES].AddWriteBuffer(&sCellBuffer);
	sPassWaterSim[PRE_UPDATE_PARTICLES].AddWriteBuffer(&sParticleBuffer);

	// update particles
	sPassWaterSim[UPDATE_PARTICLES].CreatePass("water sim update particles");
	sPassWaterSim[UPDATE_PARTICLES].AddShader(&sWaterSimUpdateParticlesCS);
	sPassWaterSim[UPDATE_PARTICLES].AddWriteBuffer(&sCellBuffer);
	sPassWaterSim[UPDATE_PARTICLES].AddWriteBuffer(&sParticleBuffer);
	
	// debug line
	sPassWaterSim[DEBUG_LINE].CreatePass("water sim debug line", true, false, false, PrimitiveType::LINE);
	sPassWaterSim[DEBUG_LINE].AddMesh(&sWaterSimDebugMeshLine);
	sPassWaterSim[DEBUG_LINE].AddShader(&sWaterSimDebugLineVS);
	sPassWaterSim[DEBUG_LINE].AddShader(&sWaterSimDebugLinePS);
	sPassWaterSim[DEBUG_LINE].AddBuffer(&sCellBuffer);
	sPassWaterSim[DEBUG_LINE].AddBuffer(&sCellFaceBuffer);
	sPassWaterSim[DEBUG_LINE].AddRenderTexture(&sDebugBackbuffer, 0, 0, BlendState::PremultipliedAlphaBlend());
	sPassWaterSim[DEBUG_LINE].SetCamera(&gCameraMain);

	// debug cube
	sPassWaterSim[DEBUG_CUBE].CreatePass("water sim debug cube", true, false, false, PrimitiveType::TRIANGLE);
	sPassWaterSim[DEBUG_CUBE].AddMesh(&sWaterSimDebugMeshCube);
	sPassWaterSim[DEBUG_CUBE].AddShader(&sWaterSimDebugCubeVS);
	sPassWaterSim[DEBUG_CUBE].AddShader(&sWaterSimDebugCubePS);
	sPassWaterSim[DEBUG_CUBE].AddBuffer(&sCellBuffer);
	sPassWaterSim[DEBUG_CUBE].AddBuffer(&sCellFaceBuffer);
	sPassWaterSim[DEBUG_CUBE].AddRenderTexture(&sDebugBackbuffer, 0, 0, BlendState::PremultipliedAlphaBlend());
	sPassWaterSim[DEBUG_CUBE].SetCamera(&gCameraMain);

	for (int i = 0; i < WATERSIM_PASSTYPE_COUNT; i++)
	{
		scene.AddPass(&sPassWaterSim[i]);
		store.AddPass(&sPassWaterSim[i]);
	}

	store.AddShader(&sWaterSimP2GCS);
	store.AddShader(&sWaterSimP2GVS);
	store.AddShader(&sWaterSimP2GPS);
	store.AddShader(&sWaterSimPreUpdateGridCS);
	store.AddShader(&sWaterSimUpdateGridCS);
	store.AddShader(&sWaterSimProjectCS);
	store.AddShader(&sWaterSimG2PCS);
	store.AddShader(&sWaterSimClearMinMaxBufferCS);
	store.AddShader(&sWaterSimRasterizerVS);
	store.AddShader(&sWaterSimRasterizerPS);
	store.AddShader(&sWaterSimRasterizerCS);
	store.AddShader(&sWaterSimBlitBackbufferPS);
	store.AddShader(&sWaterSimResetGridCS);
	store.AddShader(&sWaterSimResetParticlesCS);
	store.AddShader(&sWaterSimPreUpdateParticlesCS);
	store.AddShader(&sWaterSimUpdateParticlesCS);
	store.AddShader(&sWaterSimDebugLineVS);
	store.AddShader(&sWaterSimDebugLinePS);
	store.AddShader(&sWaterSimDebugCubeVS);
	store.AddShader(&sWaterSimDebugCubePS);
	store.AddBuffer(&sCellBuffer);
	store.AddBuffer(&sCellBufferTemp);
	store.AddBuffer(&sCellFaceBuffer);
	store.AddBuffer(&sCellFaceBufferTemp);
	store.AddBuffer(&sCellAuxBuffer);
	store.AddBuffer(&sParticleBuffer);
	store.AddTexture(&sDebugBackbuffer);
	store.AddTexture(&sDepthBufferMax);
	store.AddTexture(&sDepthBufferMin);
	store.AddTexture(&sCellRenderTexture);
	store.AddMesh(&sWaterSimDebugMeshLine);
	store.AddMesh(&sWaterSimDebugMeshCube);
	store.AddMesh(&sWaterSimParticleMesh);
}

void WaterSim::PrepareWaterSim(CommandList commandList)
{
	fatalAssertf(GetCellCount() < sCellRenderTextureWidth * sCellRenderTextureHeight, "Cell count exceeds render texture limit");

	XMStoreFloat3(&sParticleSpawnSourcePos, WaterSim::sCellSize * XMVectorSet(WaterSim::sCellCountX / 2, WaterSim::sCellCountY / 3, WaterSim::sCellCountZ / 2, 0));
	XMStoreFloat3(&sParticleSpawnSourceSpan, WaterSim::sCellSize * XMVectorSet(1, 1, 1, 0));

	for (int i = 0; i < WATERSIM_PASSTYPE_COUNT; i++)
	{
		SET_UNIFORM_VAR(sPassWaterSim[i], mPassUniform, mCellSize, sCellSize);
		SET_UNIFORM_VAR(sPassWaterSim[i], mPassUniform, mParticleCount, sParticleCount);
		SET_UNIFORM_VAR(sPassWaterSim[i], mPassUniform, mCellCount, XMUINT3(sCellCountX, sCellCountY, sCellCountZ));
		SET_UNIFORM_VAR(sPassWaterSim[i], mPassUniform, mParticleCountPerCell, WATERSIM_PARTICLE_COUNT_PER_CELL);
		SET_UNIFORM_VAR(sPassWaterSim[i], mPassUniform, mGridRenderOffset, XMFLOAT3(0.5f, 0.5f, 0.5f));
		SET_UNIFORM_VAR(sPassWaterSim[i], mPassUniform, mGridRenderScale, XMFLOAT3(1.0f / (sCellSize * sCellCountX), 1.0f / (sCellSize * sCellCountY), 1.0f / (sCellSize * sCellCountZ)));
		SET_UNIFORM_VAR(sPassWaterSim[i], mPassUniform, mWaterSimBackbufferSize, XMUINT2(sBackbufferWidth, sBackbufferHeight));
		SET_UNIFORM_VAR(sPassWaterSim[i], mPassUniform, mCurrentJacobiIteration, 0);
		SET_UNIFORM_VAR(sPassWaterSim[i], mPassUniform, mParticleSpawnSourcePos, sParticleSpawnSourcePos);
		SET_UNIFORM_VAR(sPassWaterSim[i], mPassUniform, mParticleSpawnSourceSpan, sParticleSpawnSourceSpan);
		SET_UNIFORM_VAR(sPassWaterSim[i], mPassUniform, mCellRenderTextureSize, XMUINT2(sCellRenderTextureWidth, sCellRenderTextureHeight));
	}

	SetTimeStepScale(sTimeStepScale);
	SetApplyForce(sApplyForce);
	SetG(sGravitationalAccel);
	SetWaterDensity(sWaterDensity);
	SetFlipScale(sFlipScale);
	SetAliveParticleCount(sAliveParticleCount);
	SetWarmStart(sWarmStart);
	SetJacobiIterationCount(sJacobiIterationCount);
	SetExplosionPos(sExplosionPos);
	SetExplosionForceScale(sExplosionForceScale);
	SetApplyExplosion(sApplyExplosion);
	SetWaterSimMode(sWaterSimMode);
	SetUseRasterizerP2G(sUseRasterizerP2G);

	sParticles.resize(sParticleCount);
	sCellFaces.resize(sCellFaceCountX * sCellFaceCountY * sCellFaceCountZ * 3);
	sCells.resize(sCellCountX * sCellCountY * sCellCountZ);
}

void WaterSim::WaterSimResetParticles(CommandList commandList)
{
	GPU_LABEL(commandList, "WaterSim Reset Particles");

	sCellBuffer.MakeReadyToWriteAgain(commandList);
	sParticleBuffer.MakeReadyToWrite(commandList);
	gRenderer.RecordComputePass(sPassWaterSim[RESET_PARTICLES], commandList, sParticleThreadGroupCount, 1, 1);
}

void WaterSim::WaterSimResetParticlesMPM(CommandList commandList)
{
	GPU_LABEL(commandList, "WaterSim MPM Reset Particles");

	sCellBuffer.MakeReadyToWriteAgain(commandList);
	sParticleBuffer.MakeReadyToWrite(commandList);
	gRenderer.RecordComputePass(sPassWaterSim[RESET_PARTICLES], commandList, sParticleThreadGroupCount, 1, 1);

	sCellBuffer.MakeReadyToWriteAgain(commandList);
	sParticleBuffer.MakeReadyToWriteAgain(commandList);
	gRenderer.RecordComputePass(sPassWaterSim[PRE_UPDATE_PARTICLES], commandList, sParticleThreadGroupCount, 1, 1);

	sCellBuffer.MakeReadyToWriteAgain(commandList);
	sParticleBuffer.MakeReadyToWriteAgain(commandList);
	gRenderer.RecordComputePass(sPassWaterSim[UPDATE_PARTICLES], commandList, sParticleThreadGroupCount, 1, 1);
}

void WaterSim::WaterSimResetGrid(CommandList commandList)
{
	GPU_LABEL(commandList, "WaterSim Reset Grid");

	sCellBuffer.MakeReadyToWriteAgain(commandList);
	sCellBufferTemp.MakeReadyToWriteAgain(commandList);
	sCellFaceBuffer.MakeReadyToWrite(commandList);
	sCellFaceBufferTemp.MakeReadyToWrite(commandList);
	sCellRenderTexture.MakeReadyToWrite(commandList);
	gRenderer.RecordComputePass(sPassWaterSim[RESET_GRID], commandList, sCellFaceThreadGroupCount, 1, 1);
}

void WaterSim::WaterSimStepOnce(CommandList commandList)
{
	GPU_LABEL(commandList, "WaterSim Step Once");

	if (PARAM_sanitizeWaterSim.Get())
	{
		sCellBuffer.GetReadbackBufferData(sCells.data(), sCells.size() * sizeof(sCells[0]));
		for (int i = 0; i < sCells.size(); i++)
		{
			fatalAssertf(!isnan(sCells[i].mPressure), "cell nan pressure detected!");
			fatalAssertf(!isinf(sCells[i].mPressure), "cell infinite pressure detected!");
			fatalAssertf(!XMVector4IsNaN(XMLoadFloat4(&sCells[i].mPosPressureAndNonSolidCount)), "cell pos nan detected!");
			fatalAssertf(!XMVector4IsInfinite(XMLoadFloat4(&sCells[i].mPosPressureAndNonSolidCount)), "cell pos infinite detected!");
			fatalAssertf(!XMVector4IsNaN(XMLoadFloat4(&sCells[i].mNegPressureAndDivergence)), "cell neg nan detected!");
			fatalAssertf(!XMVector4IsInfinite(XMLoadFloat4(&sCells[i].mNegPressureAndDivergence)), "cell neg infinite detected!");
		}
		sCellFaceBuffer.GetReadbackBufferData(sCellFaces.data(), sCellFaces.size() * sizeof(sCellFaces[0]));
		for (int i = 0; i < sCellFaces.size(); i++)
		{
			fatalAssertf(!isnan(sCellFaces[i].mVelocity), "cell face nan velocity detected!");
			fatalAssertf(!isinf(sCellFaces[i].mVelocity), "cell face infinite velocity detected!");
		}
		sParticleBuffer.GetReadbackBufferData(sParticles.data(), sParticles.size() * sizeof(sParticles[0]));
		for (int i = 0; i < sParticles.size(); i++)
		{
			fatalAssertf(!XMVector3IsNaN(XMLoadFloat3(&sParticles[i].mVelocity)), "particle nan velocity detected!");
			fatalAssertf(!XMVector3IsInfinite(XMLoadFloat3(&sParticles[i].mVelocity)), "particle infinite velocity detected!");
		}
	}

	for (int subStep = 0; subStep < sSubStepCount; subStep++)
	{
		GPU_LABEL(commandList, "WaterSim Sub Step");
		
		sParticleBuffer.MakeReadyToWriteAuto(commandList);
		sCellBuffer.MakeReadyToWriteAgain(commandList);
		sCellFaceBuffer.MakeReadyToWrite(commandList);
		gRenderer.RecordComputePass(sPassWaterSim[P2G], commandList, sParticleThreadGroupCount, 1, 1);

		sCellBuffer.MakeReadyToWriteAgain(commandList);
		sCellFaceBuffer.MakeReadyToWriteAgain(commandList);
		sCellFaceBufferTemp.MakeReadyToWrite(commandList);
		gRenderer.RecordComputePass(sPassWaterSim[PRE_UPDATE_GRID], commandList, sCellFaceThreadGroupCount, 1, 1);

		sCellFaceBufferTemp.MakeReadyToRead(commandList);
		sCellBuffer.MakeReadyToRead(commandList);
		sCellFaceBuffer.MakeReadyToWriteAgain(commandList);
		gRenderer.RecordComputePass(sPassWaterSim[UPDATE_GRID], commandList, sCellFaceThreadGroupCount, 1, 1);

		// use last iteration to update velocity
		sCellFaceBuffer.MakeReadyToWrite(commandList);
		sCellAuxBuffer.MakeReadyToWrite(commandList);
		sCellBuffer.MakeReadyToWrite(commandList);
		sCellBufferTemp.MakeReadyToWrite(commandList);
		for (int i = 0; i <= sJacobiIterationCount + 1; i++)
		{
			// second last iteration update velocity
			// last iteration stores remaining divergence
			SET_UNIFORM_VAR(sPassWaterSim[PROJECT], mPassUniform, mCurrentJacobiIteration, i);
			gRenderer.RecordComputePass(sPassWaterSim[PROJECT], commandList, sCellFaceThreadGroupCount, 1, 1);
			sCellBuffer.MakeReadyToWriteAgain(commandList);
			sCellBufferTemp.MakeReadyToWriteAgain(commandList);
		}

		sCellFaceBuffer.MakeReadyToRead(commandList);
		sParticleBuffer.MakeReadyToWrite(commandList);
		gRenderer.RecordComputePass(sPassWaterSim[G2P], commandList, sParticleThreadGroupCount, 1, 1);
	}

	if (PARAM_sanitizeWaterSim.Get())
	{
		// sanitizer
		sParticleBuffer.RecordPrepareToGetBufferData(commandList);
		sCellFaceBuffer.RecordPrepareToGetBufferData(commandList);
		sCellBuffer.RecordPrepareToGetBufferData(commandList);
	}
}

void WaterSim::WaterSimStepOnceMPM(CommandList commandList)
{
	GPU_LABEL(commandList, "WaterSim MPM Step Once");

	if (PARAM_sanitizeWaterSim.Get())
	{
		sCellBuffer.GetReadbackBufferData(sCells.data(), sCells.size() * sizeof(sCells[0]));
		for (int i = 0; i < sCells.size(); i++)
		{
			fatalAssertf(!XMVector3IsNaN(XMLoadFloat3(&sCells[i].mVelocity)), "cell nan velocity detected!");
			fatalAssertf(!XMVector3IsInfinite(XMLoadFloat3(&sCells[i].mVelocity)), "cell infinite velocity detected!");
		}
		sCellFaceBuffer.GetReadbackBufferData(sCellFaces.data(), sCellFaces.size() * sizeof(sCellFaces[0]));
		for (int i = 0; i < sCellFaces.size(); i++)
		{
			fatalAssertf(!isnan(sCellFaces[i].mVelocity), "cell face nan velocity detected!");
			fatalAssertf(!isinf(sCellFaces[i].mVelocity), "cell face infinite velocity detected!");
		}
		sParticleBuffer.GetReadbackBufferData(sParticles.data(), sParticles.size() * sizeof(sParticles[0]));
		for (int i = 0; i < sParticles.size(); i++)
		{
			fatalAssertf(!XMVector3IsNaN(XMLoadFloat3(&sParticles[i].mVelocity)), "particle nan velocity detected!");
			fatalAssertf(!XMVector3IsInfinite(XMLoadFloat3(&sParticles[i].mVelocity)), "particle infinite velocity detected!");
		}
	}

	for (int subStep = 0; subStep < sSubStepCount; subStep++)
	{
		GPU_LABEL(commandList, "WaterSim MPM Sub Step");
		
		if (sUseRasterizerP2G)
		{
			sParticleBuffer.MakeReadyToWriteAuto(commandList);
			sCellRenderTexture.MakeReadyToRender(commandList);
			gRenderer.RecordGraphicsPassInstanced(sPassWaterSim[P2G_RASTERIZER], commandList, sAliveParticleCount * 27); // 3x3x3 neighboring cells
		}
		else
		{
			sParticleBuffer.MakeReadyToWriteAuto(commandList);
			sCellBuffer.MakeReadyToWriteAgain(commandList);
			sCellFaceBuffer.MakeReadyToWrite(commandList);
			gRenderer.RecordComputePass(sPassWaterSim[P2G], commandList, sParticleThreadGroupCount, 1, 1);
		}

		sCellFaceBufferTemp.MakeReadyToRead(commandList);
		sCellBuffer.MakeReadyToWriteAgain(commandList);
		sCellFaceBuffer.MakeReadyToWriteAuto(commandList);
		sCellRenderTexture.MakeReadyToWrite(commandList);
		gRenderer.RecordComputePass(sPassWaterSim[UPDATE_GRID], commandList, sCellFaceThreadGroupCount, 1, 1);

		sCellFaceBuffer.MakeReadyToRead(commandList);
		sCellBuffer.MakeReadyToWriteAgain(commandList);
		sParticleBuffer.MakeReadyToWriteAgain(commandList);
		sCellRenderTexture.MakeReadyToWriteAgain(commandList);
		gRenderer.RecordComputePass(sPassWaterSim[G2P], commandList, sParticleThreadGroupCount, 1, 1);
	}

	if (PARAM_sanitizeWaterSim.Get())
	{
		// sanitizer
		sParticleBuffer.RecordPrepareToGetBufferData(commandList);
		sCellFaceBuffer.RecordPrepareToGetBufferData(commandList);
		sCellBuffer.RecordPrepareToGetBufferData(commandList);
	}
}

void WaterSim::WaterSimRasterize(CommandList commandList)
{
	GPU_LABEL(commandList, "WaterSim Rasterize");
	
	sDepthBufferMax.MakeReadyToWrite(commandList);
	sDepthBufferMin.MakeReadyToWrite(commandList);
	sDebugBackbuffer.MakeReadyToWrite(commandList);
	gRenderer.RecordComputePass(sPassWaterSim[CLEAR], commandList, ceil(sBackbufferWidth / WATERSIM_THREAD_PER_THREADGROUP_X), ceil(sBackbufferHeight / WATERSIM_THREAD_PER_THREADGROUP_Y), 1);

	if (sEnableDebugCell || sEnableDebugCellVelocity)
	{
		sCellBuffer.MakeReadyToRead(commandList);
		sCellFaceBuffer.MakeReadyToRead(commandList);
		sDebugBackbuffer.MakeReadyToRender(commandList);
		if (sEnableDebugCell)
			gRenderer.RecordGraphicsPassInstanced(sPassWaterSim[DEBUG_CUBE], commandList, sCellCountX * sCellCountY * sCellCountZ);
		if (sEnableDebugCellVelocity)
			gRenderer.RecordGraphicsPassInstanced(sPassWaterSim[DEBUG_LINE], commandList, sCellCountX * sCellCountY * sCellCountZ);
		sCellBuffer.MakeReadyToWrite(commandList);
	}

	if (sUseComputeRasterizer)
	{
		sParticleBuffer.MakeReadyToRead(commandList);
		sDepthBufferMax.MakeReadyToWriteAgain(commandList);
		sDepthBufferMin.MakeReadyToWriteAgain(commandList);
		gRenderer.RecordComputePass(sPassWaterSim[RASTERIZER_CS], commandList, sParticleThreadGroupCount, 1, 1);
	}
	else
	{
		sParticleBuffer.MakeReadyToRead(commandList);
		gRenderer.RecordGraphicsPassInstanced(sPassWaterSim[RASTERIZER], commandList, sAliveParticleCount);
	}

	sDepthBufferMax.MakeReadyToRead(commandList);
	sDepthBufferMin.MakeReadyToRead(commandList);
	sDebugBackbuffer.MakeReadyToRead(commandList);
	gRenderer.RecordGraphicsPass(sPassWaterSim[BLIT], commandList);
}

void WaterSim::SetTimeStepScale(float timeStepScale)
{
	sTimeStepScale = timeStepScale / sSubStepCount;
	sTimeStepScale = max(EPSILON, sTimeStepScale);
	for (int i = 0; i < WATERSIM_PASSTYPE_COUNT; i++)
	{
		SET_UNIFORM_VAR(sPassWaterSim[i], mPassUniform, mTimeStepScale, sTimeStepScale);
	}
}

void WaterSim::SetApplyForce(bool applyForce)
{
	sApplyForce = applyForce;
	for (int i = 0; i < WATERSIM_PASSTYPE_COUNT; i++)
	{
		SET_UNIFORM_VAR(sPassWaterSim[i], mPassUniform, mApplyForce, sApplyForce ? 1.0f : 0.0f);
	}
}

void WaterSim::SetG(float G)
{
	sGravitationalAccel = G;
	for (int i = 0; i < WATERSIM_PASSTYPE_COUNT; i++)
	{
		SET_UNIFORM_VAR(sPassWaterSim[i], mPassUniform, mGravitationalAccel, sGravitationalAccel);
	}
}

void WaterSim::SetWaterDensity(float density)
{
	sWaterDensity = max(EPSILON, density);
	for (int i = 0; i < WATERSIM_PASSTYPE_COUNT; i++)
	{
		SET_UNIFORM_VAR(sPassWaterSim[i], mPassUniform, mWaterDensity, sWaterDensity);
	}
}

void WaterSim::SetFlipScale(float flipScale)
{
	sFlipScale = flipScale;
	for (int i = 0; i < WATERSIM_PASSTYPE_COUNT; i++)
	{
		SET_UNIFORM_VAR(sPassWaterSim[i], mPassUniform, mFlipScale, sFlipScale);
	}
}

void WaterSim::SetAliveParticleCount(int particleCount)
{
	sAliveParticleCount = particleCount < sParticleCount ? particleCount : sParticleCount;
	for (int i = 0; i < WATERSIM_PASSTYPE_COUNT; i++)
	{
		SET_UNIFORM_VAR(sPassWaterSim[i], mPassUniform, mAliveParticleCount, sAliveParticleCount);
	}
}

void WaterSim::SetWarmStart(bool warmStart)
{
	sWarmStart = warmStart;
	for (int i = 0; i < WATERSIM_PASSTYPE_COUNT; i++)
	{
		SET_UNIFORM_VAR(sPassWaterSim[i], mPassUniform, mWarmStart, sWarmStart ? 1.0f : 0.0f);
	}
}

void WaterSim::SetJacobiIterationCount(int jacobiIterationCount)
{
	sJacobiIterationCount = jacobiIterationCount;
	fatalAssert(sJacobiIterationCount > 0);
	for (int i = 0; i < WATERSIM_PASSTYPE_COUNT; i++)
	{
		SET_UNIFORM_VAR(sPassWaterSim[i], mPassUniform, mJacobiIterationCount, sJacobiIterationCount);
	}
}

void WaterSim::SetExplosionPos(XMFLOAT3 explosionPos)
{
	sExplosionPos = explosionPos;
	for (int i = 0; i < WATERSIM_PASSTYPE_COUNT; i++)
	{
		SET_UNIFORM_VAR(sPassWaterSim[i], mPassUniform, mExplosionPos, sExplosionPos);
	}
}

void WaterSim::SetExplosionForceScale(XMFLOAT3 explosionForceScale)
{
	sExplosionForceScale = explosionForceScale;
	for (int i = 0; i < WATERSIM_PASSTYPE_COUNT; i++)
	{
		SET_UNIFORM_VAR(sPassWaterSim[i], mPassUniform, mExplosionForceScale, sExplosionForceScale);
	}
}

void WaterSim::SetApplyExplosion(bool applyExplosion)
{
	sApplyExplosion = applyExplosion;
	for (int i = 0; i < WATERSIM_PASSTYPE_COUNT; i++)
	{
		SET_UNIFORM_VAR(sPassWaterSim[i], mPassUniform, mApplyExplosion, sApplyExplosion);
	}
}

void WaterSim::SetWaterSimMode(int mode)
{
	assertf(mode < WATERSIM_MODE_COUNT, "invalid watersim mode");
	sWaterSimMode = (WaterSimMode)mode;
	for (int i = 0; i < WATERSIM_PASSTYPE_COUNT; i++)
	{
		SET_UNIFORM_VAR(sPassWaterSim[i], mPassUniform, mWaterSimMode, sWaterSimMode);
	}
}

void WaterSim::SetUseRasterizerP2G(bool useRasterizerP2G)
{
	sUseRasterizerP2G = useRasterizerP2G;
	for (int i = 0; i < WATERSIM_PASSTYPE_COUNT; i++)
	{
		SET_UNIFORM_VAR(sPassWaterSim[i], mPassUniform, mUseRasterizerP2G, sUseRasterizerP2G ? 1.0f : 0.0f);
	}
}
