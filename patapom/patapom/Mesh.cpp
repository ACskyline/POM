#include "Mesh.h"
#include "Texture.h"

Mesh::Mesh(const wstring& debugName,
	const MeshType& type,
	const XMFLOAT3& position,
	const XMFLOAT3& rotation,
	const XMFLOAT3& scale) :
	mDebugName(debugName),
	mType(type),
	mPosition(position),
	mScale(scale),
	mRotation(rotation)
{
	if (mType == MeshType::PLANE)
	{
		SetPlane();
	}
	else if (mType == MeshType::CUBE)
	{
		SetCube();
	}
	else if (mType == MeshType::FULLSCREEN_QUAD)
	{
		SetFullScreenQuad();
	}
}

Mesh::~Mesh()
{
	Release();
}

void Mesh::ResetMesh(MeshType type,
	const XMFLOAT3& position,
	const XMFLOAT3& scale,
	const XMFLOAT3& rotation)
{
	mType = type;
	mPosition = position;
	mScale = scale;
	mRotation = rotation;
	if (mType == MeshType::PLANE)
	{
		SetPlane();
	}
	else if (mType == MeshType::CUBE)
	{
		SetCube();
	}
	else if(mType == MeshType::FULLSCREEN_QUAD)
	{
		SetFullScreenQuad();
	}
}

void Mesh::InitMesh(
	Renderer* renderer,
	int frameCount,
	DescriptorHeap& cbvSrvUavDescriptorHeap,
	DescriptorHeap& samplerDescriptorHeap)
{
	mRenderer = renderer;

	// create uniform buffers
	CreateUniformBuffer(frameCount);
	for(int i = 0;i<frameCount;i++)
		UpdateUniformBuffer(i);

	// object table
	// CAUTION: assuming the texture srv and sampler of meshes in a pass are compatible, allocate for the mesh containing the most textures
	// TODO: add a drawable class so that the above assumption is implicit
	mCbvSrvUavDescriptorHeapTableHandleVec.resize(frameCount);
	mSamplerDescriptorHeapTableHandleVec.resize(frameCount);
	for (int i = 0; i < frameCount; i++)
	{
		// object texture table
		mCbvSrvUavDescriptorHeapTableHandleVec[i] = cbvSrvUavDescriptorHeap.GetCurrentFreeGpuAddress();
		for (auto texture : mTextureVec)
			cbvSrvUavDescriptorHeap.AllocateSrv(texture->GetTextureBuffer(), texture->GetSrvDesc(), 1);
		
		// object sampler table
		mSamplerDescriptorHeapTableHandleVec[i] = samplerDescriptorHeap.GetCurrentFreeGpuAddress();
		for (auto texture : mTextureVec)
			samplerDescriptorHeap.AllocateSampler(texture->GetSamplerDesc(), 1);
	}

	CreateVertexBuffer();
	CreateIndexBuffer();
	UpdateVertexBuffer();
	UpdateIndexBuffer();
}

void Mesh::CreateUniformBuffer(int frameCount)
{
	mUniformBufferVec.resize(frameCount);
	for (int i = 0; i < frameCount; i++)
	{
		HRESULT hr = mRenderer->mDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // this heap will be used to upload the constant buffer data
			D3D12_HEAP_FLAG_NONE, // no flags
			&CD3DX12_RESOURCE_DESC::Buffer(sizeof(ObjectUniform)), // size of the resource heap. Must be a multiple of 64KB for single-textures and constant buffers
			Renderer::TranslateResourceLayout(ResourceLayout::UPLOAD), // will be data that is read from so we keep it in the generic read state
			nullptr, // we do not have use an optimized clear value for constant buffers
			IID_PPV_ARGS(&mUniformBufferVec[i]));

		fatalAssertf(SUCCEEDED(hr), "create object uniform buffer failed");
		wstring name(L": object uniform buffer ");
		mUniformBufferVec[i]->SetName((mDebugName + name + to_wstring(i)).data());
	}
}

void Mesh::UpdateUniformBuffer(int frameIndex)
{
	XMStoreFloat4x4(&mObjectUniform.mModel,
		XMMatrixScaling(mScale.x, mScale.y, mScale.z) *
		XMMatrixRotationX(XMConvertToRadians(mRotation.x)) *
		XMMatrixRotationY(XMConvertToRadians(mRotation.y)) *
		XMMatrixRotationZ(XMConvertToRadians(mRotation.z)) *
		XMMatrixTranslation(mPosition.x, mPosition.y, mPosition.z));

	XMStoreFloat4x4(&mObjectUniform.mModelInv, XMMatrixInverse(nullptr, XMLoadFloat4x4(&mObjectUniform.mModel)));
	
	CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU. (so end is less than or equal to begin)
	void* cpuAddress;
	mUniformBufferVec[frameIndex]->Map(0, &readRange, &cpuAddress);
	memcpy(cpuAddress, &mObjectUniform, sizeof(mObjectUniform));
	mUniformBufferVec[frameIndex]->Unmap(0, &readRange);
}

void Mesh::CreateVertexBuffer()
{
	mVertexBufferSizeInBytes = sizeof(Vertex) * mVertexVec.size();

	HRESULT hr = mRenderer->mDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), // a default heap
		D3D12_HEAP_FLAG_NONE, // no flags
		&CD3DX12_RESOURCE_DESC::Buffer(mVertexBufferSizeInBytes), // resource description for a buffer
		Renderer::TranslateResourceLayout(ResourceLayout::COPY_DST), // we will start this heap in the copy destination state since we will copy data from the upload heap to this heap
		nullptr, // optimized clear value must be null for this type of resource. used for render targets and depth/stencil buffers
		IID_PPV_ARGS(&mVertexBuffer));

	fatalAssertf(SUCCEEDED(hr), "create vertex buffer failed");
	mVertexBuffer->SetName(L"mesh vertex buffer");
	mVertexBufferView.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
	mVertexBufferView.StrideInBytes = sizeof(Vertex);
	mVertexBufferView.SizeInBytes = mVertexBufferSizeInBytes;
}

void Mesh::CreateIndexBuffer()
{
	mIndexBufferSizeInBytes = sizeof(uint32_t) * mIndexVec.size();

	HRESULT hr = mRenderer->mDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), // a default heap
		D3D12_HEAP_FLAG_NONE, // no flags
		&CD3DX12_RESOURCE_DESC::Buffer(mIndexBufferSizeInBytes), // resource description for a buffer
		Renderer::TranslateResourceLayout(ResourceLayout::COPY_DST), // start in the copy destination state
		nullptr, // optimized clear value must be null for this type of resource
		IID_PPV_ARGS(&mIndexBuffer));

	fatalAssertf(SUCCEEDED(hr), "create index buffer failed");
	mIndexBuffer->SetName(L"mesh index buffer");
	mIndexBufferView.BufferLocation = mIndexBuffer->GetGPUVirtualAddress();
	mIndexBufferView.Format = DXGI_FORMAT_R32_UINT; // 32-bit unsigned integer
	mIndexBufferView.SizeInBytes = mIndexBufferSizeInBytes;
}

void Mesh::UpdateVertexBuffer()
{
	mRenderer->UploadDataToBuffer(mVertexVec.data(), mVertexBufferSizeInBytes, mVertexBufferSizeInBytes, mVertexBufferSizeInBytes, mVertexBuffer);
}

void Mesh::UpdateIndexBuffer()
{
	mRenderer->UploadDataToBuffer(mIndexVec.data(), mIndexBufferSizeInBytes, mIndexBufferSizeInBytes, mIndexBufferSizeInBytes, mIndexBuffer);
}

void Mesh::Release()
{
	SAFE_RELEASE(mVertexBuffer);
	SAFE_RELEASE(mIndexBuffer);
	for(auto uniformBuffer : mUniformBufferVec)
		SAFE_RELEASE(uniformBuffer);
}

D3D12_VERTEX_BUFFER_VIEW Mesh::GetVertexBufferView()
{
	return mVertexBufferView;
}

D3D12_INDEX_BUFFER_VIEW Mesh::GetIndexBufferView()
{
	return mIndexBufferView;
}

D3D12_GPU_VIRTUAL_ADDRESS Mesh::GetUniformBufferGpuAddress(int frameIndex)
{
	return mUniformBufferVec[frameIndex]->GetGPUVirtualAddress();
}

D3D12_GPU_DESCRIPTOR_HANDLE Mesh::GetCbvSrvUavDescriptorHeapTableHandle(int frameIndex)
{
	return mCbvSrvUavDescriptorHeapTableHandleVec[frameIndex];
}

D3D12_GPU_DESCRIPTOR_HANDLE Mesh::GetSamplerDescriptorHeapTableHandle(int frameIndex)
{
	return mSamplerDescriptorHeapTableHandleVec[frameIndex];
}

D3D_PRIMITIVE_TOPOLOGY Mesh::GetPrimitiveType()
{
	return mPrimitiveType;
}

void Mesh::SetPosition(const XMFLOAT3& position)
{
	mPosition = position;
}

void Mesh::SetRotation(const XMFLOAT3& rotation)
{
	mRotation = rotation;
}

XMFLOAT3 Mesh::GetPosition()
{
	return mPosition;
}

XMFLOAT3 Mesh::GetRotation()
{
	return mRotation;
}

int Mesh::GetIndexCount()
{
	return mIndexVec.size();
}

int Mesh::GetTextureCount()
{
	return mTextureVec.size();
}

void Mesh::SetCube()
{
	mPrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	mVertexVec.resize(24);

	// CW
	// i+1-----i+2
	// |        |
	// |        |
	// i-------i+3

	// uv
	// 0,1------1,1
	//  |        |
	//  |        |
	//  |        |
	// 0.0------1,0

	// front face
	mVertexVec[0] = { {-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, 1.0f} };
	mVertexVec[1] = { {-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, 1.0f} };
	mVertexVec[2] = { {0.5f,  0.5f, -0.5f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, 1.0f} };
	mVertexVec[3] = { {0.5f, -0.5f, -0.5f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, 1.0f} };

	// right side face
	mVertexVec[4] = { {0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f} };
	mVertexVec[5] = { {0.5f,  0.5f, -0.5f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f} };
	mVertexVec[6] = { {0.5f,  0.5f,  0.5f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f} };
	mVertexVec[7] = { {0.5f, -0.5f,  0.5f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f} };

	// left side face
	mVertexVec[8] = { {-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f, 1.0f} };
	mVertexVec[9] = { {-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f, 1.0f} };
	mVertexVec[10] = { {-0.5f,  0.5f, -0.5f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f, 1.0f} };
	mVertexVec[11] = { {-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f, 1.0f} };

	// back face
	mVertexVec[12] = { {0.5f, -0.5f,  0.5f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f} };
	mVertexVec[13] = { {0.5f,  0.5f,  0.5f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f} };
	mVertexVec[14] = { {-0.5f,  0.5f,  0.5f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f} };
	mVertexVec[15] = { {-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f} };

	// top face
	mVertexVec[16] = { {-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} };
	mVertexVec[17] = { {-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} };
	mVertexVec[18] = { {0.5f,  0.5f,  0.5f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} };
	mVertexVec[19] = { {0.5f,  0.5f, -0.5f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} };

	// bottom face
	mVertexVec[20] = { {-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} };
	mVertexVec[21] = { {-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} };
	mVertexVec[22] = { {0.5f, -0.5f, -0.5f}, {1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} };
	mVertexVec[23] = { {0.5f, -0.5f,  0.5f}, {1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} };

	mIndexVec.resize(36);

	// front face 
	// first triangle
	mIndexVec[0] = 0;
	mIndexVec[1] = 1;
	mIndexVec[2] = 2;
	// second triangle
	mIndexVec[3] = 0;
	mIndexVec[4] = 2;
	mIndexVec[5] = 3;

	// left face 
	// first triangle
	mIndexVec[6] = 4;
	mIndexVec[7] = 5;
	mIndexVec[8] = 6;
	// second triangle
	mIndexVec[9] = 4;
	mIndexVec[10] = 6;
	mIndexVec[11] = 7;

	// right face 
	// first triangle
	mIndexVec[12] = 8;
	mIndexVec[13] = 9;
	mIndexVec[14] = 10;
	// second triangle
	mIndexVec[15] = 8;
	mIndexVec[16] = 10;
	mIndexVec[17] = 11;

	// back face 
	// first triangle
	mIndexVec[18] = 12;
	mIndexVec[19] = 13;
	mIndexVec[20] = 14;
	// second triangle
	mIndexVec[21] = 12;
	mIndexVec[22] = 14;
	mIndexVec[23] = 15;

	// top face 
	// first triangle
	mIndexVec[24] = 16;
	mIndexVec[25] = 17;
	mIndexVec[26] = 18;
	// second triangle
	mIndexVec[27] = 16;
	mIndexVec[28] = 18;
	mIndexVec[29] = 19;

	// bottom face
	// first triangle
	mIndexVec[30] = 20;
	mIndexVec[31] = 21;
	mIndexVec[32] = 22;
	// second triangle
	mIndexVec[33] = 20;
	mIndexVec[34] = 22;
	mIndexVec[35] = 23;
}

void Mesh::SetPlane()
{
	mPrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	mVertexVec.resize(4);

	mVertexVec[0] = { {-0.5f,  0.0f, -0.5f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} };
	mVertexVec[1] = { {-0.5f, 0.0f, 0.5f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} };
	mVertexVec[2] = { {0.5f, 0.0f, 0.5f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} };
	mVertexVec[3] = { {0.5f,  0.0f, -0.5f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} };

	mIndexVec.resize(6);

	// first triangle
	mIndexVec[0] = 0;
	mIndexVec[1] = 1;
	mIndexVec[2] = 2;
	// second triangle
	mIndexVec[3] = 0;
	mIndexVec[4] = 2;
	mIndexVec[5] = 3;
}

void Mesh::SetFullScreenQuad()
{
	mPrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	mVertexVec.resize(4);

	mVertexVec[0] = { {-1.f,  -1.f, 0.5f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, 1.0f} };
	mVertexVec[1] = { {-1.f, 1.f, 0.5f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, 1.0f} };
	mVertexVec[2] = { {1.f, 1.f, 0.5f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, 1.0f} };
	mVertexVec[3] = { {1.f,  -1.f, 0.5f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, 1.0f} };

	mIndexVec.resize(6);

	// first triangle
	mIndexVec[0] = 0;
	mIndexVec[1] = 1;
	mIndexVec[2] = 2;
	// second triangle
	mIndexVec[3] = 0;
	mIndexVec[4] = 2;
	mIndexVec[5] = 3;
}
