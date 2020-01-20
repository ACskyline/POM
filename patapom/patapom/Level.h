#pragma once

#include "GlobalInclude.h"

class Renderer;
class Scene;
class Pass;
class Mesh;
class Texture;
class Shader;
class Camera;

class Level
{
public:
	Level(const std::string& name);
	~Level();

	void AddPass(Pass* pPass);
	void AddScene(Scene* pScene);
	void AddMesh(Mesh* pMesh);
	void AddTexture(Texture* pTexture);
	void AddShader(Shader* pShader);
	void AddCamera(Camera* pCamera);

	std::vector<Pass*>& GetPasses();
	std::vector<Scene*>& GetScenes();
	std::vector<Mesh*>& GetMeshes();
	std::vector<Texture*>& GetTextures();
	int EstimateTotalCbvSrvUavCount();
	int EstimateTotalSamplerCount();
	int EstimateTotalRtvCount();
	int EstimateTotalDsvCount();

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
