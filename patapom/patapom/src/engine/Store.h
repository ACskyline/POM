#pragma once

#include "GlobalInclude.h"

class Pass;
class Scene;
class Mesh;
class Texture;
class Buffer;
class Shader;
class Camera;
class DescriptorHeap;

class Store
{
public:
	Store(const std::string& name = "unnamed store");
	~Store();

	void AddPass(Pass* pass);
	void AddScene(Scene* scene);
	void AddMesh(Mesh* mesh);
	void AddTexture(Texture* texture);
	void AddBuffer(Buffer* buffer);
	void AddShader(Shader* shader);
	void AddCamera(Camera* camera);

	std::vector<Pass*>& GetPasses();
	std::vector<Scene*>& GetScenes();
	std::vector<Mesh*>& GetMeshes();
	std::vector<Texture*>& GetTextures();
	int EstimateTotalCbvSrvUavCount(int frameCount);
	int EstimateTotalSamplerCount(int frameCount);
	int EstimateTotalRtvCount(int frameCount);
	int EstimateTotalDsvCount(int frameCount);

	void InitStore(
		Renderer* renderer,
		int frameCount,
		DescriptorHeap& cbvSrvUavDescriptorHeap,
		DescriptorHeap& samplerDescriptorHeap,
		DescriptorHeap& rtvDescriptorHeap,
		DescriptorHeap& dsvDescriptorHeap);

	void Release(bool checkOnly = false);
	void RefreshStaticDescriptors(int frameIndex);

private:
	std::string mName;
	Renderer* mRenderer;

	std::vector<Pass*> mPasses;
	std::vector<Scene*> mScenes;
	std::vector<Mesh*> mMeshes;
	std::vector<Texture*> mTextures;
	std::vector<Buffer*> mBuffers;
	std::vector<Shader*> mShaders;
	std::vector<Camera*> mCameras;
};
