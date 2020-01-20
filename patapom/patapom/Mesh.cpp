#include "Mesh.h"
#include "Renderer.h"

Mesh::Mesh(const MeshType& type, 
	const XMFLOAT3& position,
	const XMFLOAT3& rotation,
	const XMFLOAT3& scale) :
	mType(type),
	mPosition(position),
	mScale(scale),
	mRotation(rotation)
{
	if (mType == MeshType::PLANE)
	{
		InitPlane();
	}
	else if (mType == MeshType::CUBE)
	{
		InitCube();
	}
	else if (mType == MeshType::FULLSCREEN_QUAD)
	{
		InitFullScreenQuad();
	}
}

Mesh::Mesh(const MeshType& type,
	int waveParticleCountOrSegment,
	const XMFLOAT3& position,
	const XMFLOAT3& rotation,
	const XMFLOAT3& scale) :
	mType(type),
	mPosition(position),
	mScale(scale),
	mRotation(rotation)
{
	if (mType == MeshType::WAVE_PARTICLE)
	{
		InitWaveParticles(waveParticleCountOrSegment);
	}
	else if (mType == MeshType::CIRCLE)
	{
		InitCircle(waveParticleCountOrSegment);
	}
}

Mesh::Mesh(const MeshType& type,
	int cellCountX,
	int cellCountZ,
	const XMFLOAT3& position,
	const XMFLOAT3& rotation,
	const XMFLOAT3& scale) :
	mType(type),
	mPosition(position),
	mScale(scale),
	mRotation(rotation)
{
	if (mType == MeshType::TILEABLE_SURFACE)
	{
		InitWaterSurface(cellCountX, cellCountZ);
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

	}
	else if (mType == MeshType::CUBE)
	{
		InitCube();
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
			D3D12_RESOURCE_STATE_GENERIC_READ, // will be data that is read from so we keep it in the generic read state
			nullptr, // we do not have use an optimized clear value for constant buffers
			IID_PPV_ARGS(&mUniformBufferVec[i]));

		fatalAssertf(SUCCEEDED(hr), "create object uniform buffer failed");

		mUniformBufferVec[i]->SetName(L"object uniform buffer " + i);
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
		D3D12_RESOURCE_STATE_COPY_DEST, // we will start this heap in the copy destination state since we will copy data
										// from the upload heap to this heap
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
		D3D12_RESOURCE_STATE_COPY_DEST, // start in the copy destination state
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

void Mesh::InitCube()
{
	mPrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	mVertexVec.resize(24);

	// front face
	mVertexVec[0] = { -0.5f,  0.5f, -0.5f, 0.0f, 0.0f };
	mVertexVec[1] = { 0.5f, -0.5f, -0.5f, 1.0f, 1.0f };
	mVertexVec[2] = { -0.5f, -0.5f, -0.5f, 0.0f, 1.0f };
	mVertexVec[3] = { 0.5f,  0.5f, -0.5f, 1.0f, 0.0f };

	// right side face
	mVertexVec[4] = { 0.5f, -0.5f, -0.5f, 0.0f, 1.0f };
	mVertexVec[5] = { 0.5f,  0.5f,  0.5f, 1.0f, 0.0f };
	mVertexVec[6] = { 0.5f, -0.5f,  0.5f, 1.0f, 1.0f };
	mVertexVec[7] = { 0.5f,  0.5f, -0.5f, 0.0f, 0.0f };

	// left side face
	mVertexVec[8] = { -0.5f,  0.5f,  0.5f, 0.0f, 0.0f };
	mVertexVec[9] = { -0.5f, -0.5f, -0.5f, 1.0f, 1.0f };
	mVertexVec[10] = { -0.5f, -0.5f,  0.5f, 0.0f, 1.0f };
	mVertexVec[11] = { -0.5f,  0.5f, -0.5f, 1.0f, 0.0f };

	// back face
	mVertexVec[12] = { 0.5f,  0.5f,  0.5f, 0.0f, 0.0f };
	mVertexVec[13] = { -0.5f, -0.5f,  0.5f, 1.0f, 1.0f };
	mVertexVec[14] = { 0.5f, -0.5f,  0.5f, 0.0f, 1.0f };
	mVertexVec[15] = { -0.5f,  0.5f,  0.5f, 1.0f, 0.0f };

	// top face
	mVertexVec[16] = { -0.5f,  0.5f, -0.5f, 0.0f, 1.0f };
	mVertexVec[17] = { 0.5f,  0.5f,  0.5f, 1.0f, 0.0f };
	mVertexVec[18] = { 0.5f,  0.5f, -0.5f, 1.0f, 1.0f };
	mVertexVec[19] = { -0.5f,  0.5f,  0.5f, 0.0f, 0.0f };

	// bottom face
	mVertexVec[20] = { 0.5f, -0.5f,  0.5f, 0.0f, 0.0f };
	mVertexVec[21] = { -0.5f, -0.5f, -0.5f, 1.0f, 1.0f };
	mVertexVec[22] = { 0.5f, -0.5f, -0.5f, 0.0f, 1.0f };
	mVertexVec[23] = { -0.5f, -0.5f,  0.5f, 1.0f, 0.0f };

	mIndexVec.resize(36);

	// front face 
	// first triangle
	mIndexVec[0] = 0;
	mIndexVec[1] = 1;
	mIndexVec[2] = 2;
	// second triangle
	mIndexVec[3] = 0;
	mIndexVec[4] = 3;
	mIndexVec[5] = 1;

	// left face 
	// first triangle
	mIndexVec[6] = 4;
	mIndexVec[7] = 5;
	mIndexVec[8] = 6;
	// second triangle
	mIndexVec[9] = 4;
	mIndexVec[10] = 7;
	mIndexVec[11] = 5;

	// right face 
	// first triangle
	mIndexVec[12] = 8;
	mIndexVec[13] = 9;
	mIndexVec[14] = 10;
	// second triangle
	mIndexVec[15] = 8;
	mIndexVec[16] = 11;
	mIndexVec[17] = 9;

	// back face 
	// first triangle
	mIndexVec[18] = 12;
	mIndexVec[19] = 13;
	mIndexVec[20] = 14;
	// second triangle
	mIndexVec[21] = 12;
	mIndexVec[22] = 15;
	mIndexVec[23] = 13;

	// top face 
	// first triangle
	mIndexVec[24] = 16;
	mIndexVec[25] = 17;
	mIndexVec[26] = 18;
	// second triangle
	mIndexVec[27] = 16;
	mIndexVec[28] = 19;
	mIndexVec[29] = 17;

	// bottom face
	// first triangle
	mIndexVec[30] = 20;
	mIndexVec[31] = 21;
	mIndexVec[32] = 22;
	// second triangle
	mIndexVec[33] = 20;
	mIndexVec[34] = 23;
	mIndexVec[35] = 21;
}

void Mesh::InitPlane()
{
	mPrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	mVertexVec.resize(4);

	// front face
	mVertexVec[0] = { -0.5f,  0.f, -0.5f, 0.0f, 1.0f };
	mVertexVec[1] = { -0.5f, 0.f, 0.5f, 0.0f, 0.0f };
	mVertexVec[2] = { 0.5f, 0.f, 0.5f, 1.0f, 0.0f };
	mVertexVec[3] = { 0.5f,  0.f, -0.5f, 1.0f, 1.0f };

	mIndexVec.resize(6);

	// front face 
	// first triangle
	mIndexVec[0] = 0;
	mIndexVec[1] = 1;
	mIndexVec[2] = 2;
	// second triangle
	mIndexVec[3] = 0;
	mIndexVec[4] = 2;
	mIndexVec[5] = 3;
}

void Mesh::InitWaveParticles(int waveParticleCount)
{
	mPrimitiveType = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;

	mVertexVec.resize(waveParticleCount);
	mIndexVec.resize(waveParticleCount);

	for (int i = 0; i < waveParticleCount; i++)
	{
		int index = i;
		XMFLOAT2 position = { rand() / (float)RAND_MAX * 2.f - 1.f, rand() / (float)RAND_MAX * 2.f - 1.f };
		XMFLOAT2 direction = { rand() / (float)RAND_MAX * 2.f - 1.f, rand() / (float)RAND_MAX * 2.f - 1.f };
		XMStoreFloat2(&direction, XMVector2Normalize(XMLoadFloat2(&direction)));
		float height = rand() / (float)RAND_MAX * 0.1 + 0.2;
		float radius = rand() / (float)RAND_MAX * 0.05 + 0.1;
		float beta = rand() / (float)RAND_MAX;
		float speed = rand() / (float)RAND_MAX;
		mVertexVec[index] = { position.x, position.y, height, direction.x, direction.y, radius, beta, speed };
		mIndexVec[index] = index;
	}
}

void Mesh::InitWaterSurface(int cellCountX, int cellCountZ)
{
	mPrimitiveType = D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST;

	int cellCount = cellCountX * cellCountZ;
	int vertexCount = (cellCountX + 1) * (cellCountZ + 1);
	mVertexVec.resize(vertexCount);
	mIndexVec.resize(cellCount * 4);

	for (int i = 0; i <= cellCountX; i++)
	{
		for (int j = 0; j <= cellCountZ; j++)
		{
			int vIndex = i * (cellCountZ + 1) + j;
			// make the border wider so that it can sample outside of [0, 1] and return border color which is set to transparent black
			float ui = i;
			float vj = j;
			if (i == 0) ui-= EPSILON;
			else if (i == cellCountX) ui+= EPSILON;
			if (j == 0) vj-= EPSILON;
			else if (j == cellCountZ) vj+= EPSILON;
			mVertexVec[vIndex] = { i / (float)cellCountX, 0, j / (float)cellCountZ, ui / (float)cellCountX, vj / (float)cellCountZ };
		}
	}

	for (int i = 0; i < cellCountX; i++)
	{
		for (int j = 0; j < cellCountZ; j++)
		{
			int cIndex = i * cellCountZ + j;
			mIndexVec[cIndex * 4 + 0] = i * (cellCountZ + 1) + j;
			mIndexVec[cIndex * 4 + 1] = i * (cellCountZ + 1) + j + 1;
			mIndexVec[cIndex * 4 + 2] = (i + 1) * (cellCountZ + 1) + j + 1;
			mIndexVec[cIndex * 4 + 3] = (i + 1) * (cellCountZ + 1) + j;
		}
	}
}

void Mesh::InitFullScreenQuad()
{
	mPrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	mVertexVec.resize(4);

	// front face
	mVertexVec[0] = { -1.f,  -1.f, 0.5f, 0.0f, 1.0f };
	mVertexVec[1] = { -1.f, 1.f, 0.5f, 0.0f, 0.0f };
	mVertexVec[2] = { 1.f, 1.f, 0.5f, 1.0f, 0.0f };
	mVertexVec[3] = { 1.f,  -1.f, 0.5f, 1.0f, 1.0f };

	mIndexVec.resize(6);

	// front face 
	// first triangle
	mIndexVec[0] = 0;
	mIndexVec[1] = 1;
	mIndexVec[2] = 2;
	// second triangle
	mIndexVec[3] = 0;
	mIndexVec[4] = 2;
	mIndexVec[5] = 3;
}

void Mesh::InitCircle(int segment)
{
	mPrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	float angle = 360.0 / segment;

	mVertexVec.resize(3 * segment);

	for (int i = 0; i < segment; i++)
	{
		float sini = 0, cosi = 0, sinj = 0, cosj = 0;
		XMScalarSinCos(&sini, &cosi, XMConvertToRadians(angle * i));
		XMScalarSinCos(&sinj, &cosj, XMConvertToRadians(angle * (i + 1)));
		mVertexVec[i * 3 + 0] = { 0.f, 0.5f, 0.f, 0.5f, 0.5f };
		mVertexVec[i * 3 + 1] = { cosj, sinj, 0.5f, cosj * 0.5f + 0.5f, sinj * 0.5f + 0.5f };
		mVertexVec[i * 3 + 2] = { cosi, sini, 0.5f, cosi * 0.5f + 0.5f, sini * 0.5f + 0.5f };
	}

	mIndexVec.resize(3 * segment);

	for (int i = 0; i < segment; i++)
	{
		mIndexVec[i * 3 + 0] = i * 3 + 0;
		mIndexVec[i * 3 + 1] = i * 3 + 1;
		mIndexVec[i * 3 + 2] = i * 3 + 2;
	}
}
