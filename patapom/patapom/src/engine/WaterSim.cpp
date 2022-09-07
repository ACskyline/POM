#include "WaterSim.h"
#include "SharedHeader.h"
#include "Store.h"
#include "Scene.h"
#include "Buffer.h"
#include "Texture.h"
#include "Mesh.h"
#include <cmath>

CommandLineArg PARAM_sanitizeWaterSim("-sanitizeWaterSim");

PassWaterSim WaterSim::sPassWaterSimP2G("p2g");
PassWaterSim WaterSim::sPassWaterSimUpdateGrid("update grid");
PassWaterSim WaterSim::sPassWaterSimAdvectCellFace("advect cellface");
PassWaterSim WaterSim::sPassWaterSimProject("project");
PassWaterSim WaterSim::sPassWaterSimG2P("g2p");
PassWaterSim WaterSim::sPassWaterSimClear("water sim clear");
PassWaterSim WaterSim::sPassWaterSimRasterizer("rasterizer");
PassWaterSim WaterSim::sPassWaterSimBlit("water sim blit", true, false);
PassWaterSim WaterSim::sPassWaterSimResetParticles("water sim reset particles");
PassWaterSim WaterSim::sPassWaterSimResetGrid("water sim reset grid");
PassWaterSim WaterSim::sPassWaterSimDebugLine("water sim debug line", true, false, false, PrimitiveType::LINE);
PassWaterSim WaterSim::sPassWaterSimDebugCube("water sim debug cube", true, false, false, PrimitiveType::TRIANGLE);
Shader WaterSim::sWaterSimP2GCS(Shader::ShaderType::COMPUTE_SHADER, "cs_watersim_p2g");
Shader WaterSim::sWaterSimUpdateGridCS(Shader::ShaderType::COMPUTE_SHADER, "cs_watersim_updategrid");
Shader WaterSim::sWaterSimAdvectCellFaceCS(Shader::ShaderType::COMPUTE_SHADER, "cs_watersim_advectcellface");
Shader WaterSim::sWaterSimProjectCS(Shader::ShaderType::COMPUTE_SHADER, "cs_watersim_project");
Shader WaterSim::sWaterSimG2PCS(Shader::ShaderType::COMPUTE_SHADER, "cs_watersim_g2p");
Shader WaterSim::sWaterSimClearMinMaxBufferCS(Shader::ShaderType::COMPUTE_SHADER, "cs_watersim_clearminmaxbuffer");
Shader WaterSim::sWaterSimRasterizerCS(Shader::ShaderType::COMPUTE_SHADER, "cs_watersim_rasterizer");
Shader WaterSim::sWaterSimBlitBackbufferPS(Shader::ShaderType::PIXEL_SHADER, "ps_watersim_blitbackbuffer");
Shader WaterSim::sWaterSimResetParticlesCS(Shader::ShaderType::COMPUTE_SHADER, "cs_watersim_resetparticles");
Shader WaterSim::sWaterSimResetGridCS(Shader::ShaderType::COMPUTE_SHADER, "cs_watersim_resetgrid");
Shader WaterSim::sWaterSimDebugLineVS(Shader::ShaderType::VERTEX_SHADER, "vs_watersim_debug_line");
Shader WaterSim::sWaterSimDebugLinePS(Shader::ShaderType::PIXEL_SHADER, "ps_watersim_debug_line");
Shader WaterSim::sWaterSimDebugCubeVS(Shader::ShaderType::VERTEX_SHADER, "vs_watersim_debug_cube");
Shader WaterSim::sWaterSimDebugCubePS(Shader::ShaderType::PIXEL_SHADER, "ps_watersim_debug_cube");
Mesh WaterSim::sWaterSimDebugMeshLine("water sim debug mesh line", Mesh::MeshType::LINE, XMFLOAT3(0, 0, 0), XMFLOAT3(0, 0, 0), XMFLOAT3(1, 1, 1));
Mesh WaterSim::sWaterSimDebugMeshCube("water sim debug mesh cube", Mesh::MeshType::CUBE, XMFLOAT3(0, 0, 0), XMFLOAT3(0, 0, 0), XMFLOAT3(1, 1, 1));
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
const float WaterSim::sCellSize = 1.0f;
XMFLOAT3 WaterSim::sParticleSpawnSourcePos;
XMFLOAT3 WaterSim::sParticleSpawnSourceSpan;
XMFLOAT3 WaterSim::sExplosionPos;
XMFLOAT3 WaterSim::sExplosionForceScale = {20.0f, 20.0f, 20.0f};
bool WaterSim::sApplyExplosion = false;
int WaterSim::sAliveParticleCount = 256;
float WaterSim::sTimeStepScale = 20.0f;
int WaterSim::sSubStepCount = 8;
float WaterSim::sApplyForce = 1.0f;
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
RenderTexture WaterSim::sDepthBufferMax(TextureType::TEX_2D, "water sim depth buffer max", sBackbufferWidth, sBackbufferHeight, 1, 1, ReadFrom::COLOR, Format::R32_UINT, XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
RenderTexture WaterSim::sDepthBufferMin(TextureType::TEX_2D, "water sim depth buffer min", sBackbufferWidth, sBackbufferHeight, 1, 1, ReadFrom::COLOR, Format::R32_UINT, XMFLOAT4(0xffffffff, 0.0f, 0.0f, 0.0f));
RenderTexture WaterSim::sDebugBackbuffer(TextureType::TEX_2D, "water sim debug backbuffer", sBackbufferWidth, sBackbufferHeight, 1, 1, ReadFrom::COLOR, Format::R16G16B16A16_UNORM, XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
std::vector<WaterSimParticle> WaterSim::sParticles;
std::vector<WaterSimCellFace> WaterSim::sCellFaces;
std::vector<WaterSimCell> WaterSim::sCells;

void WaterSim::InitWaterSim(Store& store, Scene& scene)
{
	sPassWaterSimP2G.AddShader(&sWaterSimP2GCS);
	sPassWaterSimP2G.AddWriteBuffer(&sParticleBuffer);
	sPassWaterSimP2G.AddWriteBuffer(&sCellBuffer);
	sPassWaterSimP2G.AddWriteBuffer(&sCellFaceBuffer);

	sPassWaterSimUpdateGrid.AddShader(&sWaterSimUpdateGridCS);
	sPassWaterSimUpdateGrid.AddWriteBuffer(&sCellBuffer);
	sPassWaterSimUpdateGrid.AddWriteBuffer(&sCellFaceBuffer);
	sPassWaterSimUpdateGrid.AddWriteBuffer(&sCellFaceBufferTemp);

	sPassWaterSimAdvectCellFace.AddShader(&sWaterSimAdvectCellFaceCS);
	sPassWaterSimAdvectCellFace.AddBuffer(&sCellBuffer);
	sPassWaterSimAdvectCellFace.AddBuffer(&sCellFaceBufferTemp);
	sPassWaterSimAdvectCellFace.AddWriteBuffer(&sCellFaceBuffer);

	sPassWaterSimProject.AddShader(&sWaterSimProjectCS);
	sPassWaterSimProject.AddWriteBuffer(&sCellBuffer);
	sPassWaterSimProject.AddWriteBuffer(&sCellBufferTemp);
	sPassWaterSimProject.AddWriteBuffer(&sCellFaceBuffer);
	sPassWaterSimProject.AddWriteBuffer(&sCellAuxBuffer);

	sPassWaterSimG2P.AddShader(&sWaterSimG2PCS);
	sPassWaterSimG2P.AddBuffer(&sCellFaceBuffer);
	sPassWaterSimG2P.AddWriteBuffer(&sCellBuffer);
	sPassWaterSimG2P.AddWriteBuffer(&sParticleBuffer);

	// clear min max buffer
	sPassWaterSimClear.AddShader(&sWaterSimClearMinMaxBufferCS);
	sPassWaterSimClear.AddWriteTexture(&sDepthBufferMax, 0);
	sPassWaterSimClear.AddWriteTexture(&sDepthBufferMin, 0);
	sPassWaterSimClear.AddWriteTexture(&sDebugBackbuffer, 0);

	// particle rasterizer
	sPassWaterSimRasterizer.AddShader(&sWaterSimRasterizerCS);
	sPassWaterSimRasterizer.AddBuffer(&sParticleBuffer);
	sPassWaterSimRasterizer.AddWriteTexture(&sDepthBufferMax, 0);
	sPassWaterSimRasterizer.AddWriteTexture(&sDepthBufferMin, 0);
	sPassWaterSimRasterizer.SetCamera(&gCameraMain);

	// blit water sim backbuffer
	sPassWaterSimBlit.SetCamera(&gCameraMain);
	sPassWaterSimBlit.AddMesh(&gFullscreenTriangle);
	sPassWaterSimBlit.AddShader(&gDeferredVS);
	sPassWaterSimBlit.AddShader(&sWaterSimBlitBackbufferPS);
	sPassWaterSimBlit.AddTexture(&sDepthBufferMax);
	sPassWaterSimBlit.AddTexture(&sDepthBufferMin);
	sPassWaterSimBlit.AddTexture(&sDebugBackbuffer);

	// reset grid
	sPassWaterSimResetGrid.AddShader(&sWaterSimResetGridCS);
	sPassWaterSimResetGrid.AddWriteBuffer(&sCellBuffer);
	sPassWaterSimResetGrid.AddWriteBuffer(&sCellBufferTemp);
	sPassWaterSimResetGrid.AddWriteBuffer(&sCellFaceBuffer);
	sPassWaterSimResetGrid.AddWriteBuffer(&sCellFaceBufferTemp);

	// reset particles
	sPassWaterSimResetParticles.AddShader(&sWaterSimResetParticlesCS);
	sPassWaterSimResetParticles.AddWriteBuffer(&sCellBuffer);
	sPassWaterSimResetParticles.AddWriteBuffer(&sParticleBuffer);

	// debug line
	sPassWaterSimDebugLine.AddMesh(&sWaterSimDebugMeshLine);
	sPassWaterSimDebugLine.AddShader(&sWaterSimDebugLineVS);
	sPassWaterSimDebugLine.AddShader(&sWaterSimDebugLinePS);
	sPassWaterSimDebugLine.AddBuffer(&sCellBuffer);
	sPassWaterSimDebugLine.AddBuffer(&sCellFaceBuffer);
	sPassWaterSimDebugLine.AddRenderTexture(&sDebugBackbuffer, 0, 0, BlendState::PremultipliedAlphaBlend());
	sPassWaterSimDebugLine.SetCamera(&gCameraMain);

	// debug cube
	sPassWaterSimDebugCube.AddMesh(&sWaterSimDebugMeshCube);
	sPassWaterSimDebugCube.AddShader(&sWaterSimDebugCubeVS);
	sPassWaterSimDebugCube.AddShader(&sWaterSimDebugCubePS);
	sPassWaterSimDebugCube.AddBuffer(&sCellBuffer);
	sPassWaterSimDebugCube.AddBuffer(&sCellFaceBuffer);
	sPassWaterSimDebugCube.AddRenderTexture(&sDebugBackbuffer, 0, 0, BlendState::PremultipliedAlphaBlend());
	sPassWaterSimDebugCube.SetCamera(&gCameraMain);

	scene.AddPass(&sPassWaterSimP2G);
	scene.AddPass(&sPassWaterSimUpdateGrid);
	scene.AddPass(&sPassWaterSimAdvectCellFace);
	scene.AddPass(&sPassWaterSimProject);
	scene.AddPass(&sPassWaterSimG2P);
	scene.AddPass(&sPassWaterSimClear);
	scene.AddPass(&sPassWaterSimRasterizer);
	scene.AddPass(&sPassWaterSimBlit);
	scene.AddPass(&sPassWaterSimResetGrid);
	scene.AddPass(&sPassWaterSimResetParticles);
	scene.AddPass(&sPassWaterSimDebugLine);
	scene.AddPass(&sPassWaterSimDebugCube);

	store.AddPass(&sPassWaterSimP2G);
	store.AddPass(&sPassWaterSimUpdateGrid);
	store.AddPass(&sPassWaterSimAdvectCellFace);
	store.AddPass(&sPassWaterSimProject);
	store.AddPass(&sPassWaterSimG2P);
	store.AddPass(&sPassWaterSimClear);
	store.AddPass(&sPassWaterSimRasterizer);
	store.AddPass(&sPassWaterSimBlit);
	store.AddPass(&sPassWaterSimResetGrid);
	store.AddPass(&sPassWaterSimResetParticles);
	store.AddPass(&sPassWaterSimDebugLine);
	store.AddPass(&sPassWaterSimDebugCube);
	store.AddShader(&sWaterSimP2GCS);
	store.AddShader(&sWaterSimUpdateGridCS);
	store.AddShader(&sWaterSimAdvectCellFaceCS);
	store.AddShader(&sWaterSimProjectCS);
	store.AddShader(&sWaterSimG2PCS);
	store.AddShader(&sWaterSimClearMinMaxBufferCS);
	store.AddShader(&sWaterSimRasterizerCS);
	store.AddShader(&sWaterSimBlitBackbufferPS);
	store.AddShader(&sWaterSimResetGridCS);
	store.AddShader(&sWaterSimResetParticlesCS);
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
	store.AddMesh(&sWaterSimDebugMeshLine);
	store.AddMesh(&sWaterSimDebugMeshCube);
}

void WaterSim::PrepareWaterSim(CommandList commandList)
{
	SET_UNIFORM_VAR(sPassWaterSimP2G, mPassUniform, mCellSize, sCellSize);
	SET_UNIFORM_VAR(sPassWaterSimUpdateGrid, mPassUniform, mCellSize, sCellSize);
	SET_UNIFORM_VAR(sPassWaterSimAdvectCellFace, mPassUniform, mCellSize, sCellSize);
	SET_UNIFORM_VAR(sPassWaterSimProject, mPassUniform, mCellSize, sCellSize);
	SET_UNIFORM_VAR(sPassWaterSimG2P, mPassUniform, mCellSize, sCellSize);
	SET_UNIFORM_VAR(sPassWaterSimClear, mPassUniform, mCellSize, sCellSize);
	SET_UNIFORM_VAR(sPassWaterSimRasterizer, mPassUniform, mCellSize, sCellSize);
	SET_UNIFORM_VAR(sPassWaterSimBlit, mPassUniform, mCellSize, sCellSize);
	SET_UNIFORM_VAR(sPassWaterSimResetGrid, mPassUniform, mCellSize, sCellSize);
	SET_UNIFORM_VAR(sPassWaterSimResetParticles, mPassUniform, mCellSize, sCellSize);
	SET_UNIFORM_VAR(sPassWaterSimDebugLine, mPassUniform, mCellSize, sCellSize);
	SET_UNIFORM_VAR(sPassWaterSimDebugCube, mPassUniform, mCellSize, sCellSize);

	SET_UNIFORM_VAR(sPassWaterSimP2G, mPassUniform, mParticleCount, sParticleCount);
	SET_UNIFORM_VAR(sPassWaterSimUpdateGrid, mPassUniform, mParticleCount, sParticleCount);
	SET_UNIFORM_VAR(sPassWaterSimAdvectCellFace, mPassUniform, mParticleCount, sParticleCount);
	SET_UNIFORM_VAR(sPassWaterSimProject, mPassUniform, mParticleCount, sParticleCount);
	SET_UNIFORM_VAR(sPassWaterSimG2P, mPassUniform, mParticleCount, sParticleCount);
	SET_UNIFORM_VAR(sPassWaterSimClear, mPassUniform, mParticleCount, sParticleCount);
	SET_UNIFORM_VAR(sPassWaterSimRasterizer, mPassUniform, mParticleCount, sParticleCount);
	SET_UNIFORM_VAR(sPassWaterSimBlit, mPassUniform, mParticleCount, sParticleCount);
	SET_UNIFORM_VAR(sPassWaterSimResetGrid, mPassUniform, mParticleCount, sParticleCount);
	SET_UNIFORM_VAR(sPassWaterSimResetParticles, mPassUniform, mParticleCount, sParticleCount);
	SET_UNIFORM_VAR(sPassWaterSimDebugLine, mPassUniform, mParticleCount, sParticleCount);
	SET_UNIFORM_VAR(sPassWaterSimDebugCube, mPassUniform, mParticleCount, sParticleCount);

	SET_UNIFORM_VAR(sPassWaterSimP2G, mPassUniform, mCellCount, XMUINT3(sCellCountX, sCellCountY, sCellCountZ));
	SET_UNIFORM_VAR(sPassWaterSimUpdateGrid, mPassUniform, mCellCount, XMUINT3(sCellCountX, sCellCountY, sCellCountZ));
	SET_UNIFORM_VAR(sPassWaterSimAdvectCellFace, mPassUniform, mCellCount, XMUINT3(sCellCountX, sCellCountY, sCellCountZ));
	SET_UNIFORM_VAR(sPassWaterSimProject, mPassUniform, mCellCount, XMUINT3(sCellCountX, sCellCountY, sCellCountZ));
	SET_UNIFORM_VAR(sPassWaterSimG2P, mPassUniform, mCellCount, XMUINT3(sCellCountX, sCellCountY, sCellCountZ));
	SET_UNIFORM_VAR(sPassWaterSimClear, mPassUniform, mCellCount, XMUINT3(sCellCountX, sCellCountY, sCellCountZ));
	SET_UNIFORM_VAR(sPassWaterSimRasterizer, mPassUniform, mCellCount, XMUINT3(sCellCountX, sCellCountY, sCellCountZ));
	SET_UNIFORM_VAR(sPassWaterSimBlit, mPassUniform, mCellCount, XMUINT3(sCellCountX, sCellCountY, sCellCountZ));
	SET_UNIFORM_VAR(sPassWaterSimResetGrid, mPassUniform, mCellCount, XMUINT3(sCellCountX, sCellCountY, sCellCountZ));
	SET_UNIFORM_VAR(sPassWaterSimResetParticles, mPassUniform, mCellCount, XMUINT3(sCellCountX, sCellCountY, sCellCountZ));
	SET_UNIFORM_VAR(sPassWaterSimDebugLine, mPassUniform, mCellCount, XMUINT3(sCellCountX, sCellCountY, sCellCountZ));
	SET_UNIFORM_VAR(sPassWaterSimDebugCube, mPassUniform, mCellCount, XMUINT3(sCellCountX, sCellCountY, sCellCountZ));

	SET_UNIFORM_VAR(sPassWaterSimP2G, mPassUniform, mParticleCountPerCell, WATERSIM_PARTICLE_COUNT_PER_CELL);
	SET_UNIFORM_VAR(sPassWaterSimUpdateGrid, mPassUniform, mParticleCountPerCell, WATERSIM_PARTICLE_COUNT_PER_CELL);
	SET_UNIFORM_VAR(sPassWaterSimAdvectCellFace, mPassUniform, mParticleCountPerCell, WATERSIM_PARTICLE_COUNT_PER_CELL);
	SET_UNIFORM_VAR(sPassWaterSimProject, mPassUniform, mParticleCountPerCell, WATERSIM_PARTICLE_COUNT_PER_CELL);
	SET_UNIFORM_VAR(sPassWaterSimG2P, mPassUniform, mParticleCountPerCell, WATERSIM_PARTICLE_COUNT_PER_CELL);
	SET_UNIFORM_VAR(sPassWaterSimClear, mPassUniform, mParticleCountPerCell, WATERSIM_PARTICLE_COUNT_PER_CELL);
	SET_UNIFORM_VAR(sPassWaterSimRasterizer, mPassUniform, mParticleCountPerCell, WATERSIM_PARTICLE_COUNT_PER_CELL);
	SET_UNIFORM_VAR(sPassWaterSimBlit, mPassUniform, mParticleCountPerCell, WATERSIM_PARTICLE_COUNT_PER_CELL);
	SET_UNIFORM_VAR(sPassWaterSimResetGrid, mPassUniform, mParticleCountPerCell, WATERSIM_PARTICLE_COUNT_PER_CELL);
	SET_UNIFORM_VAR(sPassWaterSimResetParticles, mPassUniform, mParticleCountPerCell, WATERSIM_PARTICLE_COUNT_PER_CELL);
	SET_UNIFORM_VAR(sPassWaterSimDebugLine, mPassUniform, mParticleCountPerCell, WATERSIM_PARTICLE_COUNT_PER_CELL);
	SET_UNIFORM_VAR(sPassWaterSimDebugCube, mPassUniform, mParticleCountPerCell, WATERSIM_PARTICLE_COUNT_PER_CELL);

	SET_UNIFORM_VAR(sPassWaterSimP2G, mPassUniform, mGridOffset, XMFLOAT3(0.0f, 0.0f, 0.0f));
	SET_UNIFORM_VAR(sPassWaterSimUpdateGrid, mPassUniform, mGridOffset, XMFLOAT3(0.0f, 0.0f, 0.0f));
	SET_UNIFORM_VAR(sPassWaterSimAdvectCellFace, mPassUniform, mGridOffset, XMFLOAT3(0.0f, 0.0f, 0.0f));
	SET_UNIFORM_VAR(sPassWaterSimProject, mPassUniform, mGridOffset, XMFLOAT3(0.0f, 0.0f, 0.0f));
	SET_UNIFORM_VAR(sPassWaterSimG2P, mPassUniform, mGridOffset, XMFLOAT3(0.0f, 0.0f, 0.0f));
	SET_UNIFORM_VAR(sPassWaterSimClear, mPassUniform, mGridOffset, XMFLOAT3(0.0f, 0.0f, 0.0f));
	SET_UNIFORM_VAR(sPassWaterSimRasterizer, mPassUniform, mGridOffset, XMFLOAT3(0.0f, 0.0f, 0.0f));
	SET_UNIFORM_VAR(sPassWaterSimBlit, mPassUniform, mGridOffset, XMFLOAT3(0.0f, 0.0f, 0.0f));
	SET_UNIFORM_VAR(sPassWaterSimResetGrid, mPassUniform, mGridOffset, XMFLOAT3(0.0f, 0.0f, 0.0f));
	SET_UNIFORM_VAR(sPassWaterSimResetParticles, mPassUniform, mGridOffset, XMFLOAT3(0.0f, 0.0f, 0.0f));
	SET_UNIFORM_VAR(sPassWaterSimDebugLine, mPassUniform, mGridOffset, XMFLOAT3(0.0f, 0.0f, 0.0f));
	SET_UNIFORM_VAR(sPassWaterSimDebugCube, mPassUniform, mGridOffset, XMFLOAT3(0.0f, 0.0f, 0.0f));

	SET_UNIFORM_VAR(sPassWaterSimP2G, mPassUniform, mWaterSimBackbufferSize, XMUINT2(sBackbufferWidth, sBackbufferHeight));
	SET_UNIFORM_VAR(sPassWaterSimUpdateGrid, mPassUniform, mWaterSimBackbufferSize, XMUINT2(sBackbufferWidth, sBackbufferHeight));
	SET_UNIFORM_VAR(sPassWaterSimAdvectCellFace, mPassUniform, mWaterSimBackbufferSize, XMUINT2(sBackbufferWidth, sBackbufferHeight));
	SET_UNIFORM_VAR(sPassWaterSimProject, mPassUniform, mWaterSimBackbufferSize, XMUINT2(sBackbufferWidth, sBackbufferHeight));
	SET_UNIFORM_VAR(sPassWaterSimG2P, mPassUniform, mWaterSimBackbufferSize, XMUINT2(sBackbufferWidth, sBackbufferHeight));
	SET_UNIFORM_VAR(sPassWaterSimClear, mPassUniform, mWaterSimBackbufferSize, XMUINT2(sBackbufferWidth, sBackbufferHeight));
	SET_UNIFORM_VAR(sPassWaterSimRasterizer, mPassUniform, mWaterSimBackbufferSize, XMUINT2(sBackbufferWidth, sBackbufferHeight));
	SET_UNIFORM_VAR(sPassWaterSimBlit, mPassUniform, mWaterSimBackbufferSize, XMUINT2(sBackbufferWidth, sBackbufferHeight));
	SET_UNIFORM_VAR(sPassWaterSimResetGrid, mPassUniform, mWaterSimBackbufferSize, XMUINT2(sBackbufferWidth, sBackbufferHeight));
	SET_UNIFORM_VAR(sPassWaterSimResetParticles, mPassUniform, mWaterSimBackbufferSize, XMUINT2(sBackbufferWidth, sBackbufferHeight));
	SET_UNIFORM_VAR(sPassWaterSimDebugLine, mPassUniform, mWaterSimBackbufferSize, XMUINT2(sBackbufferWidth, sBackbufferHeight));
	SET_UNIFORM_VAR(sPassWaterSimDebugCube, mPassUniform, mWaterSimBackbufferSize, XMUINT2(sBackbufferWidth, sBackbufferHeight));

	SET_UNIFORM_VAR(sPassWaterSimP2G, mPassUniform, mCurrentJacobiIteration, 0);
	SET_UNIFORM_VAR(sPassWaterSimUpdateGrid, mPassUniform, mCurrentJacobiIteration, 0);
	SET_UNIFORM_VAR(sPassWaterSimAdvectCellFace, mPassUniform, mCurrentJacobiIteration, 0);
	SET_UNIFORM_VAR(sPassWaterSimProject, mPassUniform, mCurrentJacobiIteration, 0);
	SET_UNIFORM_VAR(sPassWaterSimG2P, mPassUniform, mCurrentJacobiIteration, 0);
	SET_UNIFORM_VAR(sPassWaterSimClear, mPassUniform, mCurrentJacobiIteration, 0);
	SET_UNIFORM_VAR(sPassWaterSimRasterizer, mPassUniform, mCurrentJacobiIteration, 0);
	SET_UNIFORM_VAR(sPassWaterSimBlit, mPassUniform, mCurrentJacobiIteration, 0);
	SET_UNIFORM_VAR(sPassWaterSimResetGrid, mPassUniform, mCurrentJacobiIteration, 0);
	SET_UNIFORM_VAR(sPassWaterSimResetParticles, mPassUniform, mCurrentJacobiIteration, 0); 
	SET_UNIFORM_VAR(sPassWaterSimDebugLine, mPassUniform, mCurrentJacobiIteration, 0);
	SET_UNIFORM_VAR(sPassWaterSimDebugCube, mPassUniform, mCurrentJacobiIteration, 0);

	XMStoreFloat3(&sParticleSpawnSourcePos, WaterSim::sCellSize * XMLoadFloat3(&XMFLOAT3(WaterSim::sCellCountX / 2, 0.8f * WaterSim::sCellCountY, 0.2f * WaterSim::sCellCountZ)));
	XMStoreFloat3(&sParticleSpawnSourceSpan, 2.0f * WaterSim::sCellSize * XMLoadFloat3(&XMFLOAT3(5, 1, 5)));
	
	SET_UNIFORM_VAR(sPassWaterSimP2G, mPassUniform, mParticleSpawnSourcePos, sParticleSpawnSourcePos);
	SET_UNIFORM_VAR(sPassWaterSimUpdateGrid, mPassUniform, mParticleSpawnSourcePos, sParticleSpawnSourcePos);
	SET_UNIFORM_VAR(sPassWaterSimAdvectCellFace, mPassUniform, mParticleSpawnSourcePos, sParticleSpawnSourcePos);
	SET_UNIFORM_VAR(sPassWaterSimProject, mPassUniform, mParticleSpawnSourcePos, sParticleSpawnSourcePos);
	SET_UNIFORM_VAR(sPassWaterSimG2P, mPassUniform, mParticleSpawnSourcePos, sParticleSpawnSourcePos);
	SET_UNIFORM_VAR(sPassWaterSimClear, mPassUniform, mParticleSpawnSourcePos, sParticleSpawnSourcePos);
	SET_UNIFORM_VAR(sPassWaterSimRasterizer, mPassUniform, mParticleSpawnSourcePos, sParticleSpawnSourcePos);
	SET_UNIFORM_VAR(sPassWaterSimBlit, mPassUniform, mParticleSpawnSourcePos, sParticleSpawnSourcePos);
	SET_UNIFORM_VAR(sPassWaterSimResetParticles, mPassUniform, mParticleSpawnSourcePos, sParticleSpawnSourcePos);
	SET_UNIFORM_VAR(sPassWaterSimResetGrid, mPassUniform, mParticleSpawnSourcePos, sParticleSpawnSourcePos);
	SET_UNIFORM_VAR(sPassWaterSimDebugLine, mPassUniform, mParticleSpawnSourcePos, sParticleSpawnSourcePos);
	SET_UNIFORM_VAR(sPassWaterSimDebugCube, mPassUniform, mParticleSpawnSourcePos, sParticleSpawnSourcePos);

	SET_UNIFORM_VAR(sPassWaterSimP2G, mPassUniform, mParticleSpawnSourceSpan, sParticleSpawnSourceSpan);
	SET_UNIFORM_VAR(sPassWaterSimUpdateGrid, mPassUniform, mParticleSpawnSourceSpan, sParticleSpawnSourceSpan);
	SET_UNIFORM_VAR(sPassWaterSimAdvectCellFace, mPassUniform, mParticleSpawnSourceSpan, sParticleSpawnSourceSpan);
	SET_UNIFORM_VAR(sPassWaterSimProject, mPassUniform, mParticleSpawnSourceSpan, sParticleSpawnSourceSpan);
	SET_UNIFORM_VAR(sPassWaterSimG2P, mPassUniform, mParticleSpawnSourceSpan, sParticleSpawnSourceSpan);
	SET_UNIFORM_VAR(sPassWaterSimClear, mPassUniform, mParticleSpawnSourceSpan, sParticleSpawnSourceSpan);
	SET_UNIFORM_VAR(sPassWaterSimRasterizer, mPassUniform, mParticleSpawnSourceSpan, sParticleSpawnSourceSpan);
	SET_UNIFORM_VAR(sPassWaterSimBlit, mPassUniform, mParticleSpawnSourceSpan, sParticleSpawnSourceSpan);
	SET_UNIFORM_VAR(sPassWaterSimResetParticles, mPassUniform, mParticleSpawnSourceSpan, sParticleSpawnSourceSpan);
	SET_UNIFORM_VAR(sPassWaterSimResetGrid, mPassUniform, mParticleSpawnSourceSpan, sParticleSpawnSourceSpan);
	SET_UNIFORM_VAR(sPassWaterSimDebugLine, mPassUniform, mParticleSpawnSourceSpan, sParticleSpawnSourceSpan);
	SET_UNIFORM_VAR(sPassWaterSimDebugCube, mPassUniform, mParticleSpawnSourceSpan, sParticleSpawnSourceSpan);

	SetTimeStepScale(sTimeStepScale);
	SetApplyForce(sApplyForce > 0.0f);
	SetG(sGravitationalAccel);
	SetWaterDensity(sWaterDensity);
	SetFlipScale(sFlipScale);
	SetAliveParticleCount(sAliveParticleCount);
	SetWarmStart(sWarmStart > 0.0f);
	SetJacobiIterationCount(sJacobiIterationCount);
	SetExplosionPos(sExplosionPos);
	SetExplosionForceScale(sExplosionForceScale);
	SetApplyExplosion(sApplyExplosion);

	sParticles.resize(sParticleCount);
	sCellFaces.resize(sCellFaceCountX * sCellFaceCountY * sCellFaceCountZ * 3);
	sCells.resize(sCellCountX * sCellCountY * sCellCountZ);
}

void WaterSim::WaterSimResetParticles(CommandList commandList)
{
	GPU_LABEL(commandList, "WaterSim Reset Particles");

	sCellBuffer.MakeReadyToWriteAgain(commandList);
	sParticleBuffer.MakeReadyToWrite(commandList);
	gRenderer.RecordComputePass(sPassWaterSimResetParticles, commandList, sParticleThreadGroupCount, 1, 1);
}

void WaterSim::WaterSimResetGrid(CommandList commandList)
{
	GPU_LABEL(commandList, "WaterSim Reset Grid");

	sCellBuffer.MakeReadyToWriteAgain(commandList);
	sCellBufferTemp.MakeReadyToWriteAgain(commandList);
	sCellFaceBuffer.MakeReadyToWrite(commandList);
	sCellFaceBufferTemp.MakeReadyToWrite(commandList);
	gRenderer.RecordComputePass(sPassWaterSimResetGrid, commandList, sCellFaceThreadGroupCount, 1, 1);
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
		gRenderer.RecordComputePass(sPassWaterSimP2G, commandList, sParticleThreadGroupCount, 1, 1);

		sCellBuffer.MakeReadyToWriteAgain(commandList);
		sCellFaceBuffer.MakeReadyToWriteAgain(commandList);
		sCellFaceBufferTemp.MakeReadyToWrite(commandList);
		gRenderer.RecordComputePass(sPassWaterSimUpdateGrid, commandList, sCellFaceThreadGroupCount, 1, 1);

		sCellBuffer.MakeReadyToRead(commandList);
		sCellFaceBufferTemp.MakeReadyToRead(commandList);
		sCellFaceBuffer.MakeReadyToWriteAgain(commandList);
		gRenderer.RecordComputePass(sPassWaterSimAdvectCellFace, commandList, sCellFaceThreadGroupCount, 1, 1);

		// use last iteration to update velocity
		sCellFaceBuffer.MakeReadyToWrite(commandList);
		sCellAuxBuffer.MakeReadyToWrite(commandList);
		sCellBuffer.MakeReadyToWrite(commandList);
		sCellBufferTemp.MakeReadyToWrite(commandList);
		for (int i = 0; i <= sJacobiIterationCount + 1; i++)
		{
			// second last iteration update velocity
			// last iteration stores remaining divergence
			SET_UNIFORM_VAR(sPassWaterSimProject, mPassUniform, mCurrentJacobiIteration, i);
			gRenderer.RecordComputePass(sPassWaterSimProject, commandList, sCellFaceThreadGroupCount, 1, 1);
			sCellBuffer.MakeReadyToWriteAgain(commandList);
			sCellBufferTemp.MakeReadyToWriteAgain(commandList);
		}

		sCellFaceBuffer.MakeReadyToRead(commandList);
		sParticleBuffer.MakeReadyToWrite(commandList);
		gRenderer.RecordComputePass(sPassWaterSimG2P, commandList, sParticleThreadGroupCount, 1, 1);
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
	gRenderer.RecordComputePass(sPassWaterSimClear, commandList, ceil(sBackbufferWidth / WATERSIM_THREAD_PER_THREADGROUP_X), ceil(sBackbufferHeight / WATERSIM_THREAD_PER_THREADGROUP_Y), 1);

	if (sEnableDebugCell || sEnableDebugCellVelocity)
	{
		sCellBuffer.MakeReadyToRead(commandList);
		sCellFaceBuffer.MakeReadyToRead(commandList);
		sDebugBackbuffer.MakeReadyToRender(commandList);
		if (sEnableDebugCell)
			gRenderer.RecordGraphicsPassInstanced(sPassWaterSimDebugCube, commandList, sCellCountX * sCellCountY * sCellCountZ);
		if (sEnableDebugCellVelocity)
			gRenderer.RecordGraphicsPassInstanced(sPassWaterSimDebugLine, commandList, sCellCountX * sCellCountY * sCellCountZ);
		sCellBuffer.MakeReadyToWrite(commandList);
	}

	sParticleBuffer.MakeReadyToRead(commandList);
	sDepthBufferMax.MakeReadyToWriteAgain(commandList);
	sDepthBufferMin.MakeReadyToWriteAgain(commandList);
	gRenderer.RecordComputePass(sPassWaterSimRasterizer, commandList, sParticleThreadGroupCount, 1, 1);
	
	sDepthBufferMax.MakeReadyToRead(commandList);
	sDepthBufferMin.MakeReadyToRead(commandList);
	sDebugBackbuffer.MakeReadyToRead(commandList);
	gRenderer.RecordGraphicsPass(sPassWaterSimBlit, commandList, true);
}

void WaterSim::SetTimeStepScale(float timeStepScale)
{
	sTimeStepScale = timeStepScale / sSubStepCount;
	sTimeStepScale = max(EPSILON, sTimeStepScale);
	SET_UNIFORM_VAR(sPassWaterSimP2G, mPassUniform, mTimeStepScale, sTimeStepScale);
	SET_UNIFORM_VAR(sPassWaterSimUpdateGrid, mPassUniform, mTimeStepScale, sTimeStepScale);
	SET_UNIFORM_VAR(sPassWaterSimAdvectCellFace, mPassUniform, mTimeStepScale, sTimeStepScale);
	SET_UNIFORM_VAR(sPassWaterSimProject, mPassUniform, mTimeStepScale, sTimeStepScale);
	SET_UNIFORM_VAR(sPassWaterSimG2P, mPassUniform, mTimeStepScale, sTimeStepScale);
	SET_UNIFORM_VAR(sPassWaterSimClear, mPassUniform, mTimeStepScale, sTimeStepScale);
	SET_UNIFORM_VAR(sPassWaterSimRasterizer, mPassUniform, mTimeStepScale, sTimeStepScale);
	SET_UNIFORM_VAR(sPassWaterSimBlit, mPassUniform, mTimeStepScale, sTimeStepScale);
	SET_UNIFORM_VAR(sPassWaterSimResetParticles, mPassUniform, mTimeStepScale, sTimeStepScale);
	SET_UNIFORM_VAR(sPassWaterSimResetGrid, mPassUniform, mTimeStepScale, sTimeStepScale);
	SET_UNIFORM_VAR(sPassWaterSimDebugLine, mPassUniform, mTimeStepScale, sTimeStepScale);
	SET_UNIFORM_VAR(sPassWaterSimDebugCube, mPassUniform, mTimeStepScale, sTimeStepScale);
}

void WaterSim::SetApplyForce(bool applyForce)
{
	sApplyForce = applyForce ? 1.0f : 0.0f;
	SET_UNIFORM_VAR(sPassWaterSimP2G, mPassUniform, mApplyForce, sApplyForce);
	SET_UNIFORM_VAR(sPassWaterSimUpdateGrid, mPassUniform, mApplyForce, sApplyForce);
	SET_UNIFORM_VAR(sPassWaterSimAdvectCellFace, mPassUniform, mApplyForce, sApplyForce);
	SET_UNIFORM_VAR(sPassWaterSimProject, mPassUniform, mApplyForce, sApplyForce);
	SET_UNIFORM_VAR(sPassWaterSimG2P, mPassUniform, mApplyForce, sApplyForce);
	SET_UNIFORM_VAR(sPassWaterSimClear, mPassUniform, mApplyForce, sApplyForce);
	SET_UNIFORM_VAR(sPassWaterSimRasterizer, mPassUniform, mApplyForce, sApplyForce);
	SET_UNIFORM_VAR(sPassWaterSimBlit, mPassUniform, mApplyForce, sApplyForce);
	SET_UNIFORM_VAR(sPassWaterSimResetParticles, mPassUniform, mApplyForce, sApplyForce);
	SET_UNIFORM_VAR(sPassWaterSimResetGrid, mPassUniform, mApplyForce, sApplyForce);
	SET_UNIFORM_VAR(sPassWaterSimDebugLine, mPassUniform, mApplyForce, sApplyForce);
	SET_UNIFORM_VAR(sPassWaterSimDebugCube, mPassUniform, mApplyForce, sApplyForce);
}

void WaterSim::SetG(float G)
{
	sGravitationalAccel = G;
	SET_UNIFORM_VAR(sPassWaterSimP2G, mPassUniform, mGravitationalAccel, sGravitationalAccel);
	SET_UNIFORM_VAR(sPassWaterSimUpdateGrid, mPassUniform, mGravitationalAccel, sGravitationalAccel);
	SET_UNIFORM_VAR(sPassWaterSimAdvectCellFace, mPassUniform, mGravitationalAccel, sGravitationalAccel);
	SET_UNIFORM_VAR(sPassWaterSimProject, mPassUniform, mGravitationalAccel, sGravitationalAccel);
	SET_UNIFORM_VAR(sPassWaterSimG2P, mPassUniform, mGravitationalAccel, sGravitationalAccel);
	SET_UNIFORM_VAR(sPassWaterSimClear, mPassUniform, mGravitationalAccel, sGravitationalAccel);
	SET_UNIFORM_VAR(sPassWaterSimRasterizer, mPassUniform, mGravitationalAccel, sGravitationalAccel);
	SET_UNIFORM_VAR(sPassWaterSimBlit, mPassUniform, mGravitationalAccel, sGravitationalAccel);
	SET_UNIFORM_VAR(sPassWaterSimResetParticles, mPassUniform, mGravitationalAccel, sGravitationalAccel);
	SET_UNIFORM_VAR(sPassWaterSimResetGrid, mPassUniform, mGravitationalAccel, sGravitationalAccel);
	SET_UNIFORM_VAR(sPassWaterSimDebugLine, mPassUniform, mGravitationalAccel, sGravitationalAccel);
	SET_UNIFORM_VAR(sPassWaterSimDebugCube, mPassUniform, mGravitationalAccel, sGravitationalAccel);
}

void WaterSim::SetWaterDensity(float density)
{
	sWaterDensity = max(EPSILON, density);
	SET_UNIFORM_VAR(sPassWaterSimP2G, mPassUniform, mWaterDensity, sWaterDensity);
	SET_UNIFORM_VAR(sPassWaterSimUpdateGrid, mPassUniform, mWaterDensity, sWaterDensity);
	SET_UNIFORM_VAR(sPassWaterSimAdvectCellFace, mPassUniform, mWaterDensity, sWaterDensity);
	SET_UNIFORM_VAR(sPassWaterSimProject, mPassUniform, mWaterDensity, sWaterDensity);
	SET_UNIFORM_VAR(sPassWaterSimG2P, mPassUniform, mWaterDensity, sWaterDensity);
	SET_UNIFORM_VAR(sPassWaterSimClear, mPassUniform, mWaterDensity, sWaterDensity);
	SET_UNIFORM_VAR(sPassWaterSimRasterizer, mPassUniform, mWaterDensity, sWaterDensity);
	SET_UNIFORM_VAR(sPassWaterSimBlit, mPassUniform, mWaterDensity, sWaterDensity);
	SET_UNIFORM_VAR(sPassWaterSimResetParticles, mPassUniform, mWaterDensity, sWaterDensity);
	SET_UNIFORM_VAR(sPassWaterSimResetGrid, mPassUniform, mWaterDensity, sWaterDensity);
	SET_UNIFORM_VAR(sPassWaterSimDebugLine, mPassUniform, mWaterDensity, sWaterDensity);
	SET_UNIFORM_VAR(sPassWaterSimDebugCube, mPassUniform, mWaterDensity, sWaterDensity);
}

void WaterSim::SetFlipScale(float flipScale)
{
	sFlipScale = flipScale;
	SET_UNIFORM_VAR(sPassWaterSimP2G, mPassUniform, mFlipScale, sFlipScale);
	SET_UNIFORM_VAR(sPassWaterSimUpdateGrid, mPassUniform, mFlipScale, sFlipScale);
	SET_UNIFORM_VAR(sPassWaterSimAdvectCellFace, mPassUniform, mFlipScale, sFlipScale);
	SET_UNIFORM_VAR(sPassWaterSimProject, mPassUniform, mFlipScale, sFlipScale);
	SET_UNIFORM_VAR(sPassWaterSimG2P, mPassUniform, mFlipScale, sFlipScale);
	SET_UNIFORM_VAR(sPassWaterSimClear, mPassUniform, mFlipScale, sFlipScale);
	SET_UNIFORM_VAR(sPassWaterSimRasterizer, mPassUniform, mFlipScale, sFlipScale);
	SET_UNIFORM_VAR(sPassWaterSimBlit, mPassUniform, mFlipScale, sFlipScale);
	SET_UNIFORM_VAR(sPassWaterSimResetParticles, mPassUniform, mFlipScale, sFlipScale);
	SET_UNIFORM_VAR(sPassWaterSimResetGrid, mPassUniform, mFlipScale, sFlipScale);
	SET_UNIFORM_VAR(sPassWaterSimDebugLine, mPassUniform, mFlipScale, sFlipScale);
	SET_UNIFORM_VAR(sPassWaterSimDebugCube, mPassUniform, mFlipScale, sFlipScale);
}

void WaterSim::SetAliveParticleCount(int particleCount)
{
	sAliveParticleCount = particleCount;
	SET_UNIFORM_VAR(sPassWaterSimP2G, mPassUniform, mAliveParticleCount, sAliveParticleCount);
	SET_UNIFORM_VAR(sPassWaterSimUpdateGrid, mPassUniform, mAliveParticleCount, sAliveParticleCount);
	SET_UNIFORM_VAR(sPassWaterSimAdvectCellFace, mPassUniform, mAliveParticleCount, sAliveParticleCount);
	SET_UNIFORM_VAR(sPassWaterSimProject, mPassUniform, mAliveParticleCount, sAliveParticleCount);
	SET_UNIFORM_VAR(sPassWaterSimG2P, mPassUniform, mAliveParticleCount, sAliveParticleCount);
	SET_UNIFORM_VAR(sPassWaterSimClear, mPassUniform, mAliveParticleCount, sAliveParticleCount);
	SET_UNIFORM_VAR(sPassWaterSimRasterizer, mPassUniform, mAliveParticleCount, sAliveParticleCount);
	SET_UNIFORM_VAR(sPassWaterSimBlit, mPassUniform, mAliveParticleCount, sAliveParticleCount);
	SET_UNIFORM_VAR(sPassWaterSimResetParticles, mPassUniform, mAliveParticleCount, sAliveParticleCount);
	SET_UNIFORM_VAR(sPassWaterSimResetGrid, mPassUniform, mAliveParticleCount, sAliveParticleCount);
	SET_UNIFORM_VAR(sPassWaterSimDebugLine, mPassUniform, mAliveParticleCount, sAliveParticleCount);
	SET_UNIFORM_VAR(sPassWaterSimDebugCube, mPassUniform, mAliveParticleCount, sAliveParticleCount);
}

void WaterSim::SetWarmStart(bool warmStart)
{
	sWarmStart = warmStart ? 1.0f : 0.0f;
	SET_UNIFORM_VAR(sPassWaterSimP2G, mPassUniform, mWarmStart, sWarmStart);
	SET_UNIFORM_VAR(sPassWaterSimUpdateGrid, mPassUniform, mWarmStart, sWarmStart);
	SET_UNIFORM_VAR(sPassWaterSimAdvectCellFace, mPassUniform, mWarmStart, sWarmStart);
	SET_UNIFORM_VAR(sPassWaterSimProject, mPassUniform, mWarmStart, sWarmStart);
	SET_UNIFORM_VAR(sPassWaterSimG2P, mPassUniform, mWarmStart, sWarmStart);
	SET_UNIFORM_VAR(sPassWaterSimClear, mPassUniform, mWarmStart, sWarmStart);
	SET_UNIFORM_VAR(sPassWaterSimRasterizer, mPassUniform, mWarmStart, sWarmStart);
	SET_UNIFORM_VAR(sPassWaterSimBlit, mPassUniform, mWarmStart, sWarmStart);
	SET_UNIFORM_VAR(sPassWaterSimResetParticles, mPassUniform, mWarmStart, sWarmStart);
	SET_UNIFORM_VAR(sPassWaterSimResetGrid, mPassUniform, mWarmStart, sWarmStart);
	SET_UNIFORM_VAR(sPassWaterSimDebugLine, mPassUniform, mWarmStart, sWarmStart);
	SET_UNIFORM_VAR(sPassWaterSimDebugCube, mPassUniform, mWarmStart, sWarmStart);
}

void WaterSim::SetJacobiIterationCount(int jacobiIterationCount)
{
	sJacobiIterationCount = jacobiIterationCount;
	fatalAssert(sJacobiIterationCount > 0);
	SET_UNIFORM_VAR(sPassWaterSimP2G, mPassUniform, mJacobiIterationCount, sJacobiIterationCount);
	SET_UNIFORM_VAR(sPassWaterSimUpdateGrid, mPassUniform, mJacobiIterationCount, sJacobiIterationCount);
	SET_UNIFORM_VAR(sPassWaterSimAdvectCellFace, mPassUniform, mJacobiIterationCount, sJacobiIterationCount);
	SET_UNIFORM_VAR(sPassWaterSimProject, mPassUniform, mJacobiIterationCount, sJacobiIterationCount);
	SET_UNIFORM_VAR(sPassWaterSimG2P, mPassUniform, mJacobiIterationCount, sJacobiIterationCount);
	SET_UNIFORM_VAR(sPassWaterSimClear, mPassUniform, mJacobiIterationCount, sJacobiIterationCount);
	SET_UNIFORM_VAR(sPassWaterSimRasterizer, mPassUniform, mJacobiIterationCount, sJacobiIterationCount);
	SET_UNIFORM_VAR(sPassWaterSimBlit, mPassUniform, mJacobiIterationCount, sJacobiIterationCount);
	SET_UNIFORM_VAR(sPassWaterSimResetGrid, mPassUniform, mJacobiIterationCount, sJacobiIterationCount);
	SET_UNIFORM_VAR(sPassWaterSimResetParticles, mPassUniform, mJacobiIterationCount, sJacobiIterationCount);
	SET_UNIFORM_VAR(sPassWaterSimDebugLine, mPassUniform, mJacobiIterationCount, sJacobiIterationCount);
	SET_UNIFORM_VAR(sPassWaterSimDebugCube, mPassUniform, mJacobiIterationCount, sJacobiIterationCount);
}
void WaterSim::SetExplosionPos(XMFLOAT3 explosionPos)
{
	sExplosionPos = explosionPos;
	SET_UNIFORM_VAR(sPassWaterSimP2G, mPassUniform, mExplosionPos, sExplosionPos);
	SET_UNIFORM_VAR(sPassWaterSimUpdateGrid, mPassUniform, mExplosionPos, sExplosionPos);
	SET_UNIFORM_VAR(sPassWaterSimAdvectCellFace, mPassUniform, mExplosionPos, sExplosionPos);
	SET_UNIFORM_VAR(sPassWaterSimProject, mPassUniform, mExplosionPos, sExplosionPos);
	SET_UNIFORM_VAR(sPassWaterSimG2P, mPassUniform, mExplosionPos, sExplosionPos);
	SET_UNIFORM_VAR(sPassWaterSimClear, mPassUniform, mExplosionPos, sExplosionPos);
	SET_UNIFORM_VAR(sPassWaterSimRasterizer, mPassUniform, mExplosionPos, sExplosionPos);
	SET_UNIFORM_VAR(sPassWaterSimBlit, mPassUniform, mExplosionPos, sExplosionPos);
	SET_UNIFORM_VAR(sPassWaterSimResetGrid, mPassUniform, mExplosionPos, sExplosionPos);
	SET_UNIFORM_VAR(sPassWaterSimResetParticles, mPassUniform, mExplosionPos, sExplosionPos);
	SET_UNIFORM_VAR(sPassWaterSimDebugLine, mPassUniform, mExplosionPos, sExplosionPos);
	SET_UNIFORM_VAR(sPassWaterSimDebugCube, mPassUniform, mExplosionPos, sExplosionPos);
}

void WaterSim::SetExplosionForceScale(XMFLOAT3 explosionForceScale)
{
	sExplosionForceScale = explosionForceScale;
	SET_UNIFORM_VAR(sPassWaterSimP2G, mPassUniform, mExplosionForceScale, sExplosionForceScale);
	SET_UNIFORM_VAR(sPassWaterSimUpdateGrid, mPassUniform, mExplosionForceScale, sExplosionForceScale);
	SET_UNIFORM_VAR(sPassWaterSimAdvectCellFace, mPassUniform, mExplosionForceScale, sExplosionForceScale);
	SET_UNIFORM_VAR(sPassWaterSimProject, mPassUniform, mExplosionForceScale, sExplosionForceScale);
	SET_UNIFORM_VAR(sPassWaterSimG2P, mPassUniform, mExplosionForceScale, sExplosionForceScale);
	SET_UNIFORM_VAR(sPassWaterSimClear, mPassUniform, mExplosionForceScale, sExplosionForceScale);
	SET_UNIFORM_VAR(sPassWaterSimRasterizer, mPassUniform, mExplosionForceScale, sExplosionForceScale);
	SET_UNIFORM_VAR(sPassWaterSimBlit, mPassUniform, mExplosionForceScale, sExplosionForceScale);
	SET_UNIFORM_VAR(sPassWaterSimResetGrid, mPassUniform, mExplosionForceScale, sExplosionForceScale);
	SET_UNIFORM_VAR(sPassWaterSimResetParticles, mPassUniform, mExplosionForceScale, sExplosionForceScale);
	SET_UNIFORM_VAR(sPassWaterSimDebugLine, mPassUniform, mExplosionForceScale, sExplosionForceScale);
	SET_UNIFORM_VAR(sPassWaterSimDebugCube, mPassUniform, mExplosionForceScale, sExplosionForceScale);
}

void WaterSim::SetApplyExplosion(bool applyExplosion)
{
	sApplyExplosion = applyExplosion;
	SET_UNIFORM_VAR(sPassWaterSimP2G, mPassUniform, mApplyExplosion, sApplyExplosion);
	SET_UNIFORM_VAR(sPassWaterSimUpdateGrid, mPassUniform, mApplyExplosion, sApplyExplosion);
	SET_UNIFORM_VAR(sPassWaterSimAdvectCellFace, mPassUniform, mApplyExplosion, sApplyExplosion);
	SET_UNIFORM_VAR(sPassWaterSimProject, mPassUniform, mApplyExplosion, sApplyExplosion);
	SET_UNIFORM_VAR(sPassWaterSimG2P, mPassUniform, mApplyExplosion, sApplyExplosion);
	SET_UNIFORM_VAR(sPassWaterSimClear, mPassUniform, mApplyExplosion, sApplyExplosion);
	SET_UNIFORM_VAR(sPassWaterSimRasterizer, mPassUniform, mApplyExplosion, sApplyExplosion);
	SET_UNIFORM_VAR(sPassWaterSimBlit, mPassUniform, mApplyExplosion, sApplyExplosion);
	SET_UNIFORM_VAR(sPassWaterSimResetGrid, mPassUniform, mApplyExplosion, sApplyExplosion);
	SET_UNIFORM_VAR(sPassWaterSimResetParticles, mPassUniform, mApplyExplosion, sApplyExplosion);
	SET_UNIFORM_VAR(sPassWaterSimDebugLine, mPassUniform, mApplyExplosion, sApplyExplosion);
	SET_UNIFORM_VAR(sPassWaterSimDebugCube, mPassUniform, mApplyExplosion, sApplyExplosion);
}