#pragma once

#include "GlobalInclude.h"

class Texture;

class Mesh
{
public:
	enum class MeshType { PLANE, CUBE, FULLSCREEN_QUAD, COUNT };

	struct ObjectUniform
	{
		XMFLOAT4X4 mModel;
		XMFLOAT4X4 mModelInv;
	};

	Mesh(const wstring& debugName,
		const MeshType& type,
		const XMFLOAT3& position,
		const XMFLOAT3& rotation,
		const XMFLOAT3& scale);
	~Mesh();

	void ResetMesh(MeshType type,
		const XMFLOAT3&position,
		const XMFLOAT3&scale,
		const XMFLOAT3&rotation);

	void InitMesh(
		Renderer* renderer, 
		int frameCount, 
		DescriptorHeap& cbvSrvUavDescriptorHeap, 
		DescriptorHeap& samplerDescriptorHeap);

	void CreateUniformBuffer(int frameCount);
	void UpdateUniformBuffer(int frameIndex);

	void CreateVertexBuffer();
	void CreateIndexBuffer();
	void UpdateVertexBuffer();
	void UpdateIndexBuffer();

	void Release();

	D3D_PRIMITIVE_TOPOLOGY GetPrimitiveType();
	D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView();
	D3D12_INDEX_BUFFER_VIEW GetIndexBufferView();
	D3D12_GPU_VIRTUAL_ADDRESS GetUniformBufferGpuAddress(int frameIndex);
	D3D12_GPU_DESCRIPTOR_HANDLE GetCbvSrvUavDescriptorHeapTableHandle(int frameIndex);
	D3D12_GPU_DESCRIPTOR_HANDLE GetSamplerDescriptorHeapTableHandle(int frameIndex);

	void SetPosition(const XMFLOAT3&position);
	void SetRotation(const XMFLOAT3&rotation);
	XMFLOAT3 GetPosition();
	XMFLOAT3 GetRotation();
	int GetIndexCount();
	int GetTextureCount();
	
private:
	wstring mDebugName;
	MeshType mType;
	XMFLOAT3 mPosition;
	XMFLOAT3 mScale;
	XMFLOAT3 mRotation;

	vector<Vertex> mVertexVec;
	vector<uint32_t> mIndexVec;
	vector<Texture*> mTextureVec;

	Renderer* mRenderer;
	ObjectUniform mObjectUniform;

	UINT mVertexBufferSizeInBytes;
	UINT mIndexBufferSizeInBytes;

	// TODO: hide API specific implementation in Renderer
	ID3D12Resource* mVertexBuffer;
	ID3D12Resource* mIndexBuffer;
	D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;
	D3D12_INDEX_BUFFER_VIEW mIndexBufferView;
	D3D_PRIMITIVE_TOPOLOGY mPrimitiveType;
	vector<ID3D12Resource*> mUniformBufferVec;
	vector<D3D12_GPU_DESCRIPTOR_HANDLE> mCbvSrvUavDescriptorHeapTableHandleVec;
	vector<D3D12_GPU_DESCRIPTOR_HANDLE> mSamplerDescriptorHeapTableHandleVec;

	void SetCube();
	void SetPlane();
	void SetFullScreenQuad();
};
