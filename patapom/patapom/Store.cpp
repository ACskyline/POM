#include "Store.h"

#include "Renderer.h"
#include "Camera.h"
#include "Texture.h"
#include "Buffer.h"
#include "Mesh.h"
#include "Pass.h"
#include "Scene.h"

Store::Store(const std::string& name) :
	mName(name)
{
}

Store::~Store()
{
	// this should not happen since these are all CPU resources which could be already deleted 
	// each class should only check their own GPU resources during destructor
	// Release(true);
}

void Store::AddPass(Pass* pass)
{
	mPasses.push_back(pass);
}

void Store::AddScene(Scene* scene)
{
	mScenes.push_back(scene);
}

void Store::AddMesh(Mesh* mesh)
{
	mMeshes.push_back(mesh);
}

void Store::AddTexture(Texture* texture)
{
	mTextures.push_back(texture);
}

void Store::AddBuffer(Buffer* buffer)
{
	mBuffers.push_back(buffer);
}

void Store::AddShader(Shader* shader)
{
	mShaders.push_back(shader);
}

void Store::AddCamera(Camera* camera)
{
	mCameras.push_back(camera);
}

std::vector<Pass*>& Store::GetPasses()
{
	return mPasses;
}

std::vector<Scene*>& Store::GetScenes()
{
	return mScenes;
}

std::vector<Mesh*>& Store::GetMeshes()
{
	return mMeshes;
}

std::vector<Texture*>& Store::GetTextures()
{
	return mTextures;
}

int Store::EstimateTotalCbvSrvUavCount(int frameCount)
{
	int totalCbvSrvUavCount = 0;
	for(auto scene : mScenes)
		totalCbvSrvUavCount += scene->GetTextureCount() + 1; // plus 1 for unbounded resources
	for(auto pass : mPasses)
		totalCbvSrvUavCount += pass->GetCbvSrvUavCount() + 1; // plus 1 for unbounded resources
	for (auto mesh : mMeshes)
		totalCbvSrvUavCount += mesh->GetTextureCount() + 1; // plus 1 for unbounded resources
	return totalCbvSrvUavCount * frameCount;
}

int Store::EstimateTotalSamplerCount(int frameCount)
{
	// assume one srv correspond to one sampler
	return EstimateTotalCbvSrvUavCount(frameCount);
}

int Store::EstimateTotalRtvCount(int frameCount)
{
	int totalRtvCount = 0;
	for(auto pass : mPasses)
		totalRtvCount += pass->GetShaderTargetCount();
	return totalRtvCount * frameCount;
}

int Store::EstimateTotalDsvCount(int frameCount)
{
	// assume one rtv correspond to one dsv
	return EstimateTotalRtvCount(frameCount);
}

void Store::InitStore(
	Renderer* renderer,
	int frameCount,
	DescriptorHeap& cbvSrvUavDescriptorHeap,
	DescriptorHeap& samplerDescriptorHeap,
	DescriptorHeap& rtvDescriptorHeap,
	DescriptorHeap& dsvDescriptorHeap)
{
	mRenderer = renderer;

	for (auto camera : mCameras)
		camera->InitCamera();

	for (auto shader : mShaders)
		shader->InitShader(renderer);

	for (auto texture : mTextures)
		texture->InitTexture(renderer);

	for (auto buffer : mBuffers)
		buffer->InitBuffer(renderer);

	for (auto mesh : mMeshes)
		mesh->InitMesh(
			renderer, 
			frameCount, 
			cbvSrvUavDescriptorHeap, 
			samplerDescriptorHeap);

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

void Store::Release(bool checkOnly)
{
	for (auto camera : mCameras)
		camera->Release(checkOnly);

	for (auto shader : mShaders)
		shader->Release(checkOnly);

	for (auto texture : mTextures)
		texture->Release(checkOnly);
	
	for (auto buffer : mBuffers)
		buffer->Release(checkOnly);

	for (auto mesh : mMeshes)
		mesh->Release(checkOnly);

	for (auto scene : mScenes)
		scene->Release(checkOnly);

	for (auto pass : mPasses)
		pass->Release(checkOnly);
}