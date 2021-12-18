#include "Light.h"
#include "Camera.h"

Light::Light(
	const string& name,
	const XMFLOAT3& color,
	const XMFLOAT3& position,
	Camera* camera,
	RenderTexture* renderTexture)
	:
	mName(name),
	mColor(color),
	mPosition(position),
	mScene(nullptr),
	mCamera(camera),
	mRenderTexture(renderTexture),
	mTextureIndex(-1)
{
}

Light::~Light()
{
	Release();
}

void Light::InitLight()
{
	//do nothing
}

void Light::Release()
{
	//do nothing
}

float Light::GetNearClipPlane() const
{
	if (mCamera != nullptr)
		return mCamera->GetNearClipPlane();
	else
		return 0.0f;
}

float Light::GetFarClipPlane() const
{
	if (mCamera != nullptr)
		return mCamera->GetFarClipPlane();
	else
		return 0.0f;
}

const XMFLOAT3& Light::GetColor() const
{
	return mColor;
}

const XMFLOAT3& Light::GetPosition() const
{
	return mPosition;
}

XMFLOAT4X4 Light::GetViewMatrix() const
{
	if (mCamera != nullptr)
		return mCamera->GetViewMatrix();
	else
	{
		XMFLOAT4X4 result;
		XMStoreFloat4x4(&result, XMMatrixIdentity());
		return result;
	}
}

XMFLOAT4X4 Light::GetViewInvMatrix() const
{
	if (mCamera != nullptr)
		return mCamera->GetViewInvMatrix();
	else
	{
		XMFLOAT4X4 result;
		XMStoreFloat4x4(&result, XMMatrixIdentity());
		return result;
	}
}

XMFLOAT4X4 Light::GetProjMatrix() const
{
	if (mCamera != nullptr)
		return mCamera->GetProjMatrix();
	else
	{
		XMFLOAT4X4 result;
		XMStoreFloat4x4(&result, XMMatrixIdentity());
		return result;
	}
}

XMFLOAT4X4 Light::GetProjInvMatrix() const
{
	if (mCamera != nullptr)
		return mCamera->GetProjInvMatrix();
	else
	{
		XMFLOAT4X4 result;
		XMStoreFloat4x4(&result, XMMatrixIdentity());
		return result;
	}
}

RenderTexture* Light::GetRenderTexture() const
{
	return mRenderTexture;
}

int Light::GetTextureIndex() const
{
	return mTextureIndex;
}

LightData Light::CreateLightData() const
{
	LightData ld;
	ld.mView = GetViewMatrix();
	ld.mViewInv = GetViewInvMatrix();
	ld.mProj = GetProjMatrix();
	ld.mProjInv = GetProjInvMatrix();
	ld.mColor = GetColor();
	ld.mPosWorld = GetPosition();
	ld.mNearClipPlane = GetNearClipPlane();
	ld.mFarClipPlane = GetFarClipPlane();
	ld.mTextureIndex = GetTextureIndex();
	// TODO: create different light types from Light
	ld.mLightType = LIGHT_TYPE_POINT;
	return ld;
}

void Light::SetTextureIndex(int index)
{
	mTextureIndex = index;
}

void Light::SetScene(Scene* scene)
{
	mScene = scene;
}
