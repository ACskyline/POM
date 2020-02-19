#pragma once

#include "GlobalInclude.h"

class Camera
{
public:
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
	void Release();

	XMFLOAT3 GetPosition();
	XMFLOAT3 GetTarget();
	Viewport GetViewport();
	ScissorRect GetScissorRect();
	XMFLOAT4X4 GetViewProjMatrix();
	XMFLOAT4X4 GetViewProjInvMatrix();

	XMFLOAT3 ScreenToWorld(XMFLOAT2 screenPos, bool nearClipPlane = false);

protected:
	float mWidth;
	float mHeight;
	float mFov;
	float mNearClipPlane;
	float mFarClipPlane;
	XMFLOAT3 mPosition;
	XMFLOAT3 mTarget;
	XMFLOAT3 mUp;
	XMFLOAT4X4 mViewProj;
	XMFLOAT4X4 mViewProjInv;
	Viewport mViewport;
	ScissorRect mScissorRect;

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
