#pragma once

#include "GlobalInclude.h"

class Scene;
class Mesh;
class Texture;
class Camera;

class Level
{
public:
	Level(const std::string& name = "unnamed level");
	~Level();

	void AddPass(Pass* pass);
	void AddScene(Scene* scene);
	void AddMesh(Mesh* mesh);
	void AddTexture(Texture* texture);
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

	void InitLevel(
		Renderer* renderer,
		int frameCount,
		DescriptorHeap& cbvSrvUavDescriptorHeap,
		DescriptorHeap& samplerDescriptorHeap,
		DescriptorHeap& rtvDescriptorHeap,
		DescriptorHeap& dsvDescriptorHeap);

	void Release();

private:
	std::string mName;
	Renderer* mRenderer;

	std::vector<Pass*> mPasses;
	std::vector<Scene*> mScenes;
	std::vector<Mesh*> mMeshes;
	std::vector<Texture*> mTextures;
	std::vector<Shader*> mShaders;
	std::vector<Camera*> mCameras;
};
