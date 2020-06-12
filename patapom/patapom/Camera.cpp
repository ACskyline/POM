#include "Camera.h"
#include "Pass.h"

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
	mViewport.mTopLeftX = 0;
	mViewport.mTopLeftY = 0;
	mViewport.mWidth = mWidth;
	mViewport.mHeight = mHeight;
	mViewport.mMinDepth = 0.0f;
	mViewport.mMaxDepth = 1.0f;
}

void Camera::UpdateScissorRect()
{
	mScissorRect.mLeft = 0;
	mScissorRect.mTop = 0;
	mScissorRect.mRight = mWidth;
	mScissorRect.mBottom = mHeight;
}

void Camera::UpdateMatrices()
{
	XMMATRIX view = XMMatrixLookAtLH(
		XMLoadFloat3(&mPosition), 
		XMLoadFloat3(&mTarget), 
		XMLoadFloat3(&mUp));

	XMMATRIX proj = XMMatrixPerspectiveFovLH(
		XMConvertToRadians(mFov),
		mWidth / mHeight,
		mNearClipPlane,
		mFarClipPlane);

	// mView
	XMStoreFloat4x4(
		&mView,
		view);

	// mViewInv
	XMStoreFloat4x4(
		&mViewInv,
		XMMatrixInverse(
			nullptr, 
			view));

	// mProj
	XMStoreFloat4x4(
		&mProj,
		proj);

	// mProjInv
	XMStoreFloat4x4(
		&mProjInv,
		XMMatrixInverse(
			nullptr,
			proj));

	// mViewProj
	XMStoreFloat4x4(
		&mViewProj,
		view * proj);

	// mViewProjInv
	XMStoreFloat4x4(
		&mViewProjInv,
		XMMatrixInverse(
			nullptr,
			view * proj));
}

void Camera::InitCamera()
{
	Update();
}

void Camera::Update()
{
	UpdatePosition();
	UpdateViewport();
	UpdateScissorRect();
	UpdateMatrices();
}

void Camera::Release()
{
}

XMFLOAT3 Camera::GetPosition()
{
	return mPosition;
}

XMFLOAT3 Camera::GetTarget()
{
	return mTarget;
}

Viewport Camera::GetViewport()
{
	return mViewport;
}

ScissorRect Camera::GetScissorRect()
{
	return mScissorRect;
}

XMFLOAT4X4 Camera::GetViewMatrix()
{
	return mView;
}

XMFLOAT4X4 Camera::GetViewInvMatrix()
{
	return mViewInv;
}

XMFLOAT4X4 Camera::GetProjMatrix()
{
	return mProj;
}

XMFLOAT4X4 Camera::GetProjInvMatrix()
{
	return mProjInv;
}

XMFLOAT4X4 Camera::GetViewProjMatrix()
{
	return mViewProj;
}

XMFLOAT4X4 Camera::GetViewProjInvMatrix()
{
	return mViewProjInv;
}

XMFLOAT3 Camera::GetRight()
{
	XMFLOAT3 result = {};
	XMVECTOR forward = XMLoadFloat3(&GetForward());
	XMVECTOR up = XMLoadFloat3(&mUp);
	float length = 0.f;
	XMStoreFloat(&length, XMVector3Length(up));
	fatalAssertf(EQUALF(length, 1.f), "mUp is not unit length!");
	XMStoreFloat3(&result, XMVector3Cross(forward, up));
	return result;
}

XMFLOAT3 Camera::GetForward()
{
	XMFLOAT3 result = {};
	XMStoreFloat3(&result, XMVector3Normalize(XMLoadFloat3(&mTarget) - XMLoadFloat3(&mPosition)));
	return result;
}

XMFLOAT3 Camera::GetRealUp()
{
	XMFLOAT3 result = {};
	XMVECTOR forward = XMLoadFloat3(&GetForward());
	XMVECTOR right = XMLoadFloat3(&GetRight());
	XMStoreFloat3(&result, XMVector3Cross(right, forward));
	return result;
}

float Camera::GetNearClipPlane()
{
	return mNearClipPlane;
}

float Camera::GetFarClipPlane()
{
	return mFarClipPlane;
}

float Camera::GetWidth()
{
	return mWidth;
}

float Camera::GetHeight()
{
	return mHeight;
}

float Camera::GetFov()
{
	return mFov;
}

void Camera::SetTarget(XMFLOAT3 target)
{
	mTarget = target;
}

void Camera::AddPass(Pass* pass)
{
	mPasses.push_back(pass);
}

void Camera::UpdatePassUniformBuffer(int frameIndex)
{
	for(auto pass : mPasses)
	{
		pass->UpdateUniformBuffer(frameIndex, this);
	}
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

OrbitCamera::OrbitCamera(
	float distance,
	float horizontalAngle,
	float verticalAngle,
	const XMFLOAT3 &target,
	const XMFLOAT3 &up,
	float width,
	float height,
	float fov,
	float nearClipPlane,
	float farClipPlane) :
	Camera(XMFLOAT3{ 0,0,0 },
		target,
		up,
		width,
		height,
		fov,
		nearClipPlane,
		farClipPlane),
	mDistance(distance),
	mHorizontalAngle(horizontalAngle),
	mVerticalAngle(verticalAngle)
{
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
