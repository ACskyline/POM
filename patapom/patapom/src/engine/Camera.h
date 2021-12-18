#pragma once

#include "GlobalInclude.h"
#include "Renderer.h"

class Pass;

class Camera
{
public:
	Camera();
	Camera(const XMFLOAT3 &position,
		const XMFLOAT3 &target, 
		const XMFLOAT3 &up,
		float width,
		float height,
		float fov,
		float nearClipPlane,
		float farClipPlane);
	virtual ~Camera();

	void ResetCamera(const XMFLOAT3 &position,
		const XMFLOAT3 &target,
		const XMFLOAT3 &up,
		float width,
		float height,
		float fov,
		float nearClipPlane,
		float farClipPlane);

	void InitCamera();
	void Update();
	void Release(bool checkOnly = false);

	XMFLOAT3 GetPosition();
	XMFLOAT3 GetTarget();
	Viewport GetViewport();
	ScissorRect GetScissorRect();
	XMFLOAT4X4 GetViewMatrix();
	XMFLOAT4X4 GetViewInvMatrix();
	XMFLOAT4X4 GetProjMatrix();
	XMFLOAT4X4 GetProjInvMatrix();
	XMFLOAT4X4 GetViewProjMatrix();
	XMFLOAT4X4 GetViewProjInvMatrix();
	XMFLOAT3 GetRight();
	XMFLOAT3 GetForward();
	XMFLOAT3 GetRealUp();
	float GetNearClipPlane();
	float GetFarClipPlane();
	float GetWidth();
	float GetHeight();
	float GetFov();
	void SetTarget(XMFLOAT3 target);
	void AddPass(Pass* pass);
	void UpdatePassUniform();
	XMFLOAT3 ScreenToWorld(XMFLOAT2 screenPos, bool useNearClipPlane = false);

protected:
	float mWidth;
	float mHeight;
	float mFov;
	float mNearClipPlane;
	float mFarClipPlane;
	XMFLOAT3 mPosition;
	XMFLOAT3 mTarget;
	XMFLOAT3 mUp;
	XMFLOAT4X4 mView;
	XMFLOAT4X4 mViewInv;
	XMFLOAT4X4 mProj;
	XMFLOAT4X4 mProjInv;
	XMFLOAT4X4 mViewProj;
	XMFLOAT4X4 mViewProjInv;
	Viewport mViewport;
	ScissorRect mScissorRect;
	vector<Pass*> mPasses;

	virtual void UpdatePosition();
	void UpdateViewport();
	void UpdateScissorRect();
	void UpdateMatrices();
};

class OrbitCamera :
	public Camera
{
public:
	OrbitCamera(float distance,
		float horizontalAngle,
		float verticalAngle,
		const XMFLOAT3 &target,
		const XMFLOAT3 &up,
		float width,
		float height,
		float fov,
		float nearClipPlane,
		float farClipPlane);
		
	virtual ~OrbitCamera();

	float GetDistance();
	float GetHorizontalAngle();
	float GetVerticalAngle();
	void SetDistance(float distance);
	void SetHorizontalAngle(float horizontalAngle);
	void SetVerticalAngle(float verticalAngle);

private:
	float mDistance;
	float mHorizontalAngle;
	float mVerticalAngle;

	virtual void UpdatePosition();
};
