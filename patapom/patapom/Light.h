#pragma once
#include "GlobalInclude.h"

class Scene;
class Camera;
class RenderTexture;

class Light
{
public:
	Light(
		const string& name,
		const XMFLOAT3& color,
		const XMFLOAT3& position,
		Camera* camera,
		RenderTexture* renderTexture);
	~Light();

	float GetNearClipPlane() const;
	float GetFarClipPlane() const;
	const XMFLOAT3& GetColor() const;
	const XMFLOAT3& GetPosition() const;
	XMFLOAT4X4 GetViewMatrix() const;
	XMFLOAT4X4 GetViewInvMatrix() const;
	XMFLOAT4X4 GetProjMatrix() const;
	XMFLOAT4X4 GetProjInvMatrix() const;
	RenderTexture* GetRenderTexture() const;
	int GetTextureIndex() const;

	void SetTextureIndex(int index);
	void SetScene(Scene* scene);

	void InitLight();
	void Release();

private:
	int mTextureIndex; // corresponding texture index in a scene
	bool mUseShadow;
	string mName;
	XMFLOAT3 mColor;
	XMFLOAT3 mPosition;
	Scene* mScene; // a light is bound to a scene
	Camera* mCamera;
	RenderTexture* mRenderTexture;
};
