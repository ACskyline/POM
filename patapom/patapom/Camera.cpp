#include "Camera.h"

Camera::Camera(const XMFLOAT3 &position,
	const XMFLOAT3 &target,
	const XMFLOAT3 &up,
	float width,
	float height,
	float fov,
	float nearClipPlane,
	float farClipPlane) :
	mPosition(position),
	mTarget(target),
	mUp(up),
	mWidth(width),
	mHeight(height),
	mFov(fov),
	mNearClipPlane(nearClipPlane),
	mFarClipPlane(farClipPlane)
{
	Update();
}

Camera::~Camera()
{
}

void Camera::ResetCamera(const XMFLOAT3 &position,
	const XMFLOAT3 &target,
	const XMFLOAT3 &up,
	float width,
	float height,
	float fov,
	float nearClipPlane,
	float farClipPlane)
{
	mPosition = position;
	mTarget = target;
	mUp = up;
	mWidth = width;
	mHeight = height;
	mFov = fov;
	mNearClipPlane = nearClipPlane;
	mFarClipPlane = farClipPlane;
}

void Camera::UpdatePosition()
{
}

void Camera::UpdateViewport()
{
	mViewport.TopLeftX = 0;
	mViewport.TopLeftY = 0;
	mViewport.Width = mWidth;
	mViewport.Height = mHeight;
	mViewport.MinDepth = 0.0f;
	mViewport.MaxDepth = 1.0f;
}

void Camera::UpdateScissorRect()
{
	mScissorRect.left = 0;
	mScissorRect.top = 0;
	mScissorRect.right = mWidth;
	mScissorRect.bottom = mHeight;
}

void Camera::UpdateMatrices()
{
	XMStoreFloat4x4(
		&mViewProj,
		XMMatrixLookAtLH(
			XMLoadFloat3(&mPosition),
			XMLoadFloat3(&mTarget),
			XMLoadFloat3(&mUp)) *
		XMMatrixPerspectiveFovLH(
			XMConvertToRadians(mFov),
			mWidth / mHeight,
			mNearClipPlane,
			mFarClipPlane));

	XMStoreFloat4x4(
		&mViewProjInv,
		XMMatrixInverse(
			nullptr,
			XMLoadFloat4x4(&mViewProj)));
}

void Camera::Update()
{
	UpdatePosition();
	UpdateViewport();
	UpdateScissorRect();
	UpdateMatrices();
}

XMFLOAT3 Camera::GetPosition()
{
	return mPosition;
}

XMFLOAT3 Camera::GetTarget()
{
	return mTarget;
}

D3D12_VIEWPORT Camera::GetViewport()
{
	return mViewport;
}

D3D12_RECT Camera::GetScissorRect()
{
	return mScissorRect;
}

XMFLOAT4X4 Camera::GetViewProjMatrix()
{
	return mViewProj;
}

XMFLOAT4X4 Camera::GetViewProjInvMatrix()
{
	return mViewProjInv;
}

XMFLOAT3 Camera::ScreenToWorld(XMFLOAT2 screenPos, bool useNearClipPlane)
{
	//assuming viewport.MaxDepth is 1 and viewport.MinDepth is 0
	float z = useNearClipPlane ? 0.f : 1.f;
	float clipPlane = useNearClipPlane ? mNearClipPlane : mFarClipPlane;
	XMFLOAT4 clipPos((screenPos.x / (float)mWidth * 2.f - 1.f) * clipPlane, ((1.f - screenPos.y / (float)mHeight) * 2.f - 1.f) * clipPlane, z * clipPlane, 1.f * clipPlane);
	XMFLOAT4 worldPos = {};
	XMStoreFloat4(&worldPos, XMVector4Transform(XMLoadFloat4(&clipPos), XMLoadFloat4x4(&mViewProjInv)));
	return XMFLOAT3(worldPos.x, worldPos.y, worldPos.z);
}

////////////////////////////////////////////
/////////////// Orbit Camera ///////////////
////////////////////////////////////////////

OrbitCamera::OrbitCamera(float distance,
	float horizontalAngle,
	float verticalAngle,
	const XMFLOAT3 &target,
	const XMFLOAT3 &up,
	float width,
	float height,
	float fov,
	float nearClipPlane,
	float farClipPlane) :
	mDistance(distance),
	mHorizontalAngle(horizontalAngle),
	mVerticalAngle(verticalAngle),
	Camera(XMFLOAT3{0,0,0}, 
		target, 
		up, 
		width, 
		height,
		fov, 
		nearClipPlane, 
		farClipPlane)
{
	UpdatePosition();
}

OrbitCamera::~OrbitCamera()
{
}

void OrbitCamera::UpdatePosition()
{
	float y = mDistance * XMScalarSin(XMConvertToRadians(mVerticalAngle));
	float x = mDistance * XMScalarCos(XMConvertToRadians(mVerticalAngle)) * XMScalarSin(XMConvertToRadians(mHorizontalAngle));
	float z = -mDistance * XMScalarCos(XMConvertToRadians(mVerticalAngle)) * XMScalarCos(XMConvertToRadians(mHorizontalAngle));

	mPosition = XMFLOAT3(mTarget.x + x, mTarget.y + y, mTarget.z + z);
}

float OrbitCamera::GetDistance()
{
	return mDistance;
}

float OrbitCamera::GetHorizontalAngle()
{
	return mHorizontalAngle;
}

float OrbitCamera::GetVerticalAngle()
{
	return mVerticalAngle;
}

void OrbitCamera::SetDistance(float _distance)
{
	mDistance = _distance;
	UpdatePosition();
}

void OrbitCamera::SetHorizontalAngle(float _horizontalAngle)
{
	mHorizontalAngle = _horizontalAngle;
	UpdatePosition();
}

void OrbitCamera::SetVerticalAngle(float _verticalAngle)
{
	mVerticalAngle = _verticalAngle;
	UpdatePosition();
}
