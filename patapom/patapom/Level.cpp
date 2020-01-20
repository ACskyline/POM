#include "Level.h"

#include "Renderer.h"
#include "Camera.h"
#include "Texture.h"
#include "Mesh.h"
#include "Pass.h"
#include "Scene.h"

Level::Level(const std::string& name) :
	mName(name)
{
}

Level::~Level()
{
}

void Level::AddPass(Pass* pass)
{
	mPasses.push_back(pass);
}

void Level::AddScene(Scene* scene)
{
	mScenes.push_back(scene);
}

void Level::AddMesh(Mesh* mesh)
{
	mMeshes.push_back(mesh);
}

void Level::AddTexture(Texture* texture)
{
	mTextures.push_back(texture);
}

void Level::AddShader(Shader* shader)
{
	mShaders.push_back(shader);
}

void Level::AddCamera(Camera* camera)
{
	mCameras.push_back(camera);
}

std::vector<Pass*>& Level::GetPasses()
{
	return mPasses;
}

std::vector<Scene*>& Level::GetScenes()
{
	return mScenes;
}

std::vector<Mesh*>& Level::GetMeshes()
{
	return mMeshes;
}

std::vector<Texture*>& Level::GetTextures()
{
	return mTextures;
}

int Level::EstimateTotalCbvSrvUavCount()
{
	int totalCbvSrvUavCount = 0;

	for(auto scene : mScenes)
		totalCbvSrvUavCount += scene->GetTextureCount();

	for(auto pass : mPasses)
		totalCbvSrvUavCount += pass->GetTextureCount();

	for (auto mesh : mMeshes)
		totalCbvSrvUavCount += mesh->GetTextureCount();

	totalCbvSrvUavCount *= mRenderer->mFrameCount;

	for (auto& frame : mRenderer->GetFrames())
		totalCbvSrvUavCount += frame.GetTextureCount();

	return totalCbvSrvUavCount;
}

int Level::EstimateTotalSamplerCount()
{
	// assume one srv correspond to one sampler
	return EstimateTotalCbvSrvUavCount();
}

int Level::EstimateTotalRtvCount()
{
	int totalRtvCount = 0;
	for(auto pass : mPasses)
		totalRtvCount += pass->GetRenderTextureCount();

	totalRtvCount *= mRenderer->mFrameCount;

	totalRtvCount += mRenderer->mFrameCount; // swap chain frame buffer

	return totalRtvCount;
}

int Level::EstimateTotalDsvCount()
{
	// assume one rtv correspond to one dsv
	return EstimateTotalRtvCount();
}

void Level::InitLevel(
	Renderer* renderer,
	int frameCount,
	DescriptorHeap& cbvSrvUavDescriptorHeap,
	DescriptorHeap& samplerDescriptorHeap,
	DescriptorHeap& rtvDescriptorHeap,
	DescriptorHeap& dsvDescriptorHeap)
{
	mRenderer = renderer;

	for (auto shader : mShaders)
		shader->InitShader();

	for (auto mesh : mMeshes)
		mesh->InitMesh(
			renderer, 
			frameCount, 
			cbvSrvUavDescriptorHeap, 
			samplerDescriptorHeap);

	for (auto texture : mTextures)
		texture->InitTexture(renderer);

	for (auto pass : mPasses)
		pass->InitPass(
			renderer,
			frameCount,
			cbvSrvUavDescriptorHeap,
			samplerDescriptorHeap,
			rtvDescriptorHeap,
			dsvDescriptorHeap);

	for (auto scene : mScenes)
		scene->InitScene(
			renderer,
			frameCount,
			cbvSrvUavDescriptorHeap,
			samplerDescriptorHeap);
}

void Level::Release()
{
	for (auto shader : mShaders)
		shader->Release();

	for (auto texture : mTextures)
		texture->Release();

	for (auto mesh : mMeshes)
		mesh->Release();

	for (auto scene : mScenes)
		scene->Release();

	for (auto pass : mPasses)
		pass->Release();
}