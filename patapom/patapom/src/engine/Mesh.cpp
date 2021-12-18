#include "Mesh.h"
#include "Texture.h"
#include <fstream>
#include <sstream>
#include <limits>

Mesh::Mesh(const string& debugName,
	const MeshType& type,
	const XMFLOAT3& position,
	const XMFLOAT3& rotation,
	const XMFLOAT3& scale,
	const string& fileName) :
	mDebugName(debugName),
	mType(type),
	mPosition(position),
	mScale(scale),
	mRotation(rotation),
	mFileName(fileName)
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
		SetFullscreenQuad();
	}
	else if (mType == MeshType::FULLSCREEN_TRIANGLE)
	{
		SetFullscreenTriangle();
	}
	else if (mType == MeshType::SKY_FULLSCREEN_TRIANGLE)
	{
		SetSkyFullscreenTriangle();
	}
	else if (mType == MeshType::MESH)
	{
		SetMesh();
	}
	else if (mType == MeshType::LINE)
	{
		SetLine();
	}
	else
		fatalf("unsupported mesh type!");
	UpdateMatrix();
}

Mesh::~Mesh()
{
	Release(true);
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
		SetFullscreenQuad();
	}
	else if (mType == MeshType::FULLSCREEN_TRIANGLE)
	{
		SetFullscreenTriangle();
	}
	else if (mType == MeshType::SKY_FULLSCREEN_TRIANGLE)
	{
		SetSkyFullscreenTriangle();
	}
	else if (mType == MeshType::MESH)
	{
		SetMesh();
	}
	UpdateMatrix();
}

void Mesh::InitMesh(
	Renderer* renderer,
	int frameCount,
	DescriptorHeap& cbvSrvUavDescriptorHeap)
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
	for (int i = 0; i < frameCount; i++)
	{
		// object texture table
		mCbvSrvUavDescriptorHeapTableHandleVec[i] = cbvSrvUavDescriptorHeap.GetCurrentFreeHandle();
		for (auto texture : mTextures)
			cbvSrvUavDescriptorHeap.AllocateSrv(texture->GetTextureBuffer(), texture->GetSrvDesc());
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
			&CD3DX12_RESOURCE_DESC::Buffer(sizeof(ObjectUniform)),
			Renderer::TranslateResourceLayout(ResourceLayout::UPLOAD), // will be data that is read from so we keep it in the generic read state
			nullptr, // we do not use an optimized clear value for constant buffers
			IID_PPV_ARGS(&mUniformBufferVec[i]));

		fatalAssertf(SUCCEEDED(hr), "create object uniform buffer failed");
		mUniformBufferVec[i]->SetName(StringToWstring(mDebugName + ": object uniform buffer ").c_str());
	}
}

void Mesh::UpdateUniformBuffer(int frameIndex)
{
	UpdateMatrix();
	CD3DX12_RANGE readRange(0, 0);
	void* cpuAddress;
	mUniformBufferVec[frameIndex]->Map(0, &readRange, &cpuAddress);
	memcpy(cpuAddress, &mObjectUniform, sizeof(mObjectUniform));
	mUniformBufferVec[frameIndex]->Unmap(0, nullptr);
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
	mRenderer->WriteDataToBufferSync(mVertexBuffer, mVertexVec.data(), mVertexBufferSizeInBytes);
}

void Mesh::UpdateIndexBuffer()
{
	mRenderer->WriteDataToBufferSync(mIndexBuffer, mIndexVec.data(), mIndexBufferSizeInBytes);
}

void Mesh::Release(bool checkOnly)
{
	SAFE_RELEASE(mVertexBuffer, checkOnly);
	SAFE_RELEASE(mIndexBuffer, checkOnly);
	for(auto& uniformBuffer : mUniformBufferVec) // ref because we are resetting it to nullptr
		SAFE_RELEASE(uniformBuffer, checkOnly);
}

D3D12_VERTEX_BUFFER_VIEW Mesh::GetVertexBufferView() const
{
	return mVertexBufferView;
}

D3D12_INDEX_BUFFER_VIEW Mesh::GetIndexBufferView() const
{
	return mIndexBufferView;
}

D3D12_GPU_VIRTUAL_ADDRESS Mesh::GetUniformBufferGpuAddress(int frameIndex) const
{
	return mUniformBufferVec[frameIndex]->GetGPUVirtualAddress();
}

DescriptorHeap::Handle Mesh::GetCbvSrvUavDescriptorHeapTableHandle(int frameIndex) const
{
	return mCbvSrvUavDescriptorHeapTableHandleVec[frameIndex];
}

PrimitiveType Mesh::GetPrimitiveType() const
{
	return mPrimitiveType;
}

void Mesh::AddTexture(Texture* texture)
{
	mTextures.push_back(texture);
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

int Mesh::GetIndexCount() const
{
	return mIndexVec.size();
}

int Mesh::GetTextureCount() const
{
	return mTextures.size();
}

vector<Texture*>& Mesh::GetTextures()
{
	return mTextures;
}

inline u32 LeftShift3(float fx)
{
	u32 x = *((u32*)&fx);
	if (x == (1 << 10)) --x;
	x = (x | (x << 16)) & 0b00000011000000000000000011111111;
	x = (x | (x << 8)) & 0b00000011000000001111000000001111;
	x = (x | (x << 4)) & 0b00000011000011000011000011000011;
	x = (x | (x << 2)) & 0b00001001001001001001001001001001;
	return x;
}

u32 Mesh::GenerateMortonCode(XMFLOAT3 position)
{
	return LeftShift3(position.z) << 2 | LeftShift3(position.y) << 1 | LeftShift3(position.x);
}

void Mesh::ConvertMeshToTrianglesPT(vector<TrianglePT>& outTriangles, u32 meshIndex)
{
	fatalAssertf(mIndexVec.size() > 0 && mIndexVec.size() % 3 == 0, "number of indices is not a multiple of 3");
	outTriangles.resize(mIndexVec.size() / 3);
	fatalAssertf(outTriangles.size() < PT_TRIANGLE_PER_MESH_MAX, "this affects the size of prefix sum buffer and we don't want it to be too large");
	fatalAssertf(outTriangles.size() < (size_t)std::numeric_limits<int>::max, "entry index is int32 in shaders (for prefix sum and tree building) so exceeding that will cause issues!");
	fatalAssertf(outTriangles.size() < pow(2, PT_TRIANGLE_BVH_TREE_HEIGHT_MAX - 1), "a bvh tree of %d triangles might exceed the max height %d", outTriangles.size(), PT_TRIANGLE_BVH_TREE_HEIGHT_MAX);
	XMVECTOR minPos = XMLoadFloat3(&mVertexVec[mIndexVec[0]].pos);
	XMVECTOR maxPos = XMLoadFloat3(&mVertexVec[mIndexVec[0]].pos);
	for (int i = 0; i < outTriangles.size(); i++)
	{
		const Vertex& v0 = mVertexVec[mIndexVec[i * 3]];
		const Vertex& v1 = mVertexVec[mIndexVec[i * 3 + 1]];
		const Vertex& v2 = mVertexVec[mIndexVec[i * 3 + 2]];
		outTriangles[i].mVertices[0] = v0;
		outTriangles[i].mVertices[1] = v1;
		outTriangles[i].mVertices[2] = v2;
		outTriangles[i].mMeshIndex = meshIndex;
		minPos = XMVectorMin(minPos, XMVectorMin(XMLoadFloat3(&v0.pos), XMVectorMin(XMLoadFloat3(&v1.pos), XMLoadFloat3(&v2.pos))));
		maxPos = XMVectorMax(maxPos, XMVectorMax(XMLoadFloat3(&v0.pos), XMVectorMax(XMLoadFloat3(&v1.pos), XMLoadFloat3(&v2.pos))));
		outTriangles[i].mBvhIndexLocal = INVALID_UINT32;
	}
	// scale centroid to get a better distribution of mortoncode, hence a more balanced bvh tree
	for (int i = 0; i < outTriangles.size(); i++)
	{
		const Vertex& v0 = outTriangles[i].mVertices[0];
		const Vertex& v1 = outTriangles[i].mVertices[1];
		const Vertex& v2 = outTriangles[i].mVertices[2];
		XMFLOAT3 centroid;
		XMStoreFloat3(&centroid, ((XMLoadFloat3(&v0.pos) + XMLoadFloat3(&v1.pos) + XMLoadFloat3(&v2.pos)) / 3.0f - minPos) / (maxPos - minPos));
		outTriangles[i].mMortonCode = GenerateMortonCode(centroid);
	}
}

Vertex Mesh::TransformVertexToWorldSpace(const Vertex& vertex, const XMFLOAT4X4& m, const XMFLOAT4X4& mInv)
{
	Vertex result = vertex;
	XMFLOAT4 pos(vertex.pos.x, vertex.pos.y, vertex.pos.z, 1.0f);
	XMFLOAT4 nor(vertex.nor.x, vertex.nor.y, vertex.nor.z, 0.0f);
	XMFLOAT4 tan(vertex.tan.x, vertex.tan.y, vertex.tan.z, 0.0f);
	float tanW = vertex.tan.w;
	XMStoreFloat3(&result.pos, XMVector4Transform(XMLoadFloat4(&pos), XMLoadFloat4x4(&m)));
	XMStoreFloat3(&result.nor, XMVector3Normalize(XMVector4Transform(XMLoadFloat4(&nor), XMMatrixTranspose(XMLoadFloat4x4(&mInv)))));
	XMStoreFloat4(&result.tan, XMVector3Normalize(XMVector4Transform(XMLoadFloat4(&tan), XMLoadFloat4x4(&m))));
	result.tan.w = tanW;
	return result;
}

void Mesh::UpdateMatrix()
{
	XMStoreFloat4x4(&mObjectUniform.mModel,
		XMMatrixScaling(mScale.x, mScale.y, mScale.z) *
		XMMatrixRotationX(XMConvertToRadians(mRotation.x)) *
		XMMatrixRotationY(XMConvertToRadians(mRotation.y)) *
		XMMatrixRotationZ(XMConvertToRadians(mRotation.z)) *
		XMMatrixTranslation(mPosition.x, mPosition.y, mPosition.z));

	XMStoreFloat4x4(&mObjectUniform.mModelInv, XMMatrixInverse(nullptr, XMLoadFloat4x4(&mObjectUniform.mModel)));
}

void Mesh::SetCube()
{
	mPrimitiveType = PrimitiveType::TRIANGLE;

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
	mVertexVec[0] = { {-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, -1.0f} };
	mVertexVec[1] = { {-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, -1.0f} };
	mVertexVec[2] = { {0.5f,  0.5f, -0.5f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, -1.0f} };
	mVertexVec[3] = { {0.5f, -0.5f, -0.5f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, -1.0f} };

	// right side face
	mVertexVec[4] = { {0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, -1.0f} };
	mVertexVec[5] = { {0.5f,  0.5f, -0.5f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, -1.0f} };
	mVertexVec[6] = { {0.5f,  0.5f,  0.5f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, -1.0f} };
	mVertexVec[7] = { {0.5f, -0.5f,  0.5f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, -1.0f} };

	// left side face
	mVertexVec[8] = { {-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f, -1.0f} };
	mVertexVec[9] = { {-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f, -1.0f} };
	mVertexVec[10] = { {-0.5f,  0.5f, -0.5f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f, -1.0f} };
	mVertexVec[11] = { {-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f, -1.0f} };

	// back face
	mVertexVec[12] = { {0.5f, -0.5f,  0.5f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f, -1.0f} };
	mVertexVec[13] = { {0.5f,  0.5f,  0.5f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f, -1.0f} };
	mVertexVec[14] = { {-0.5f,  0.5f,  0.5f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f, -1.0f} };
	mVertexVec[15] = { {-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f, -1.0f} };

	// top face
	mVertexVec[16] = { {-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, -1.0f} };
	mVertexVec[17] = { {-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, -1.0f} };
	mVertexVec[18] = { {0.5f,  0.5f,  0.5f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, -1.0f} };
	mVertexVec[19] = { {0.5f,  0.5f, -0.5f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, -1.0f} };

	// bottom face
	mVertexVec[20] = { {-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, -1.0f} };
	mVertexVec[21] = { {-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, -1.0f} };
	mVertexVec[22] = { {0.5f, -0.5f, -0.5f}, {1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, -1.0f} };
	mVertexVec[23] = { {0.5f, -0.5f,  0.5f}, {1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, -1.0f} };

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
	mPrimitiveType = PrimitiveType::TRIANGLE;

	mVertexVec.resize(4);

	mVertexVec[0] = { {-0.5f,  0.0f, -0.5f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, -1.0f} };
	mVertexVec[1] = { {-0.5f, 0.0f, 0.5f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, -1.0f} };
	mVertexVec[2] = { {0.5f, 0.0f, 0.5f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, -1.0f} };
	mVertexVec[3] = { {0.5f,  0.0f, -0.5f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, -1.0f} };

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

void Mesh::SetFullscreenQuad()
{
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

	mPrimitiveType = PrimitiveType::TRIANGLE;

	mVertexVec.resize(4);

	mVertexVec[0] = { {-1.f, -1.f, 0.5f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, -1.0f} };
	mVertexVec[1] = { {-1.f, 1.f, 0.5f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, -1.0f} };
	mVertexVec[2] = { {1.f, 1.f, 0.5f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, -1.0f} };
	mVertexVec[3] = { {1.f, -1.f, 0.5f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, -1.0f} };

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

void Mesh::SetFullscreenTriangleDepth(float d)
{
	// CW
	// i+1
	// |  \
	// |    \    
	// |      \
	// i-------i+2

	// uv
	// 0,2
	//  |  \
	//  |    \   
	//  |      \
	// 0,0------2,0

	mPrimitiveType = PrimitiveType::TRIANGLE;

	mVertexVec.resize(3);

	mVertexVec[0] = { {-1.f, -1.f, d}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, -1.0f} };
	mVertexVec[1] = { {-1.f, 3.f, d}, {0.0f, 2.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, -1.0f} };
	mVertexVec[2] = { {3.f, -1.f, d}, {2.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, -1.0f} };
	
	mIndexVec.resize(3);

	mIndexVec[0] = 0;
	mIndexVec[1] = 1;
	mIndexVec[2] = 2;
}

void Mesh::SetFullscreenTriangle()
{
	SetFullscreenTriangleDepth(0.5f);
}

void Mesh::SetSkyFullscreenTriangle()
{
	SetFullscreenTriangleDepth(DEPTH_FAR_REVERSED_Z_SWITCH);
}

void Mesh::SetMesh()
{
	mPrimitiveType = PrimitiveType::TRIANGLE;

	string filePathName = AssetPath + mFileName;
	stringstream ss;
	auto lambda = [&](const char* buf) {
		string str(buf);
		ss << str;
	};
	ReadFile(filePathName, lambda, false);

	vector<XMFLOAT3> vecPos;
	vector<XMFLOAT3> vecNor;
	vector<XMFLOAT2> vecUV;
	vector<Point> vecPoint;

	string str;
	while (getline(ss, str))
	{
		if (str.substr(0, 2) == "v ")
		{
			stringstream ss;
			ss << str.substr(2);
			XMFLOAT3 v;
			ss >> v.x;
			ss >> v.y;
			ss >> v.z;
			vecPos.push_back(v);
		}
		else if (str.substr(0, 3) == "vt ")
		{
			stringstream ss;
			ss << str.substr(3);
			XMFLOAT2 v;
			ss >> v.x;
			ss >> v.y;
			vecUV.push_back(v);
		}
		else if (str.substr(0, 3) == "vn ")
		{
			stringstream ss;
			ss << str.substr(3);
			XMFLOAT3 v;
			ss >> v.x;
			ss >> v.y;
			ss >> v.z;
			vecNor.push_back(v);
		}
		else if (str.substr(0, 2) == "f ")
		{
			stringstream ss;
			vector<Point> tempVecPoint;
			ss << str.substr(2);

			//Parsing
			ParseObjFace(ss, tempVecPoint);

			//if there are more than 3 vertices for one face then split it in to several triangles
			for (int i = 0; i < tempVecPoint.size(); i++)
			{
				if (i >= 3)
				{
					vecPoint.push_back(tempVecPoint.at(0));
					vecPoint.push_back(tempVecPoint.at(i - 1));
				}
				vecPoint.push_back(tempVecPoint.at(i));
			}

		}
		else if (str[0] == '#')
		{
			//comment
		}
		else
		{
			//others
		}
	}

	AssembleObjMesh(vecPos, vecUV, vecNor, vecPoint);
}

void Mesh::ParseObjFace(stringstream &ss, vector<Point>& tempVecPoint)
{
	char discard;
	char peek;
	uint32_t data;
	Point tempPoint;
	bool next;

	//One vertex in one loop
	do
	{
		next = false;
		tempPoint = { 0, 0, 0 };

		if (!next)
		{
			ss >> peek;
			if (!ss.eof() && peek >= '0' && peek <= '9')
			{
				ss.putback(peek);
				ss >> data;
				tempPoint.VI = data;//index start at 1 in an .obj file but at 0 in an array
				ss >> discard;
				if (!ss.eof())
				{
					if (discard == '/')
					{
						//do nothing, this is the normal case, we move on to the next property
					}
					else
					{
						//this happens when the current property is the last property of this point on the face
						//the discard is actually the starting number of the next point, so we put it back and move on
						ss.putback(discard);
						next = true;
					}
				}
				else
					next = true;
			}
			else
				next = true;
		}

		if (!next)
		{
			ss >> peek;
			if (!ss.eof() && peek >= '0' && peek <= '9')
			{
				ss.putback(peek);
				ss >> data;
				tempPoint.TI = data;//index start at 1 in an .obj file but at 0 in an array
				ss >> discard;
				if (!ss.eof())
				{
					if (discard == '/')
					{
						//do nothing, this is the normal case, we move on to the next property
					}
					else
					{
						//this happens when the current property is the last property of this point on the face
						//the discard is actually the starting number of the next point, so we put it back and move on
						ss.putback(discard);
						next = true;
					}
				}
				else
					next = true;
			}
			else
				next = true;
		}

		if (!next)
		{
			ss >> peek;
			if (!ss.eof() && peek >= '0' && peek <= '9')
			{
				ss.putback(peek);
				ss >> data;
				tempPoint.NI = data;//index start at 1 in an .obj file but at 0 in an array
				//normally we don't need the code below because normal index usually is the last property, but hey, better safe than sorry
				ss >> discard;
				if (!ss.eof())
				{
					if (discard == '/')
					{
						//do nothing, this is the normal case, we move on to the next property
					}
					else
					{
						//this happens when the current property is the last property of this point on the face
						//the discard is actually the starting number of the next point, so we put it back and move on
						ss.putback(discard);
						next = true;
					}
				}
				else
					next = true;
			}
			else
				next = true;
		}

		tempVecPoint.push_back(tempPoint);
	} while (!ss.eof());
}

void Mesh::AssembleObjMesh(
	const vector<XMFLOAT3> &vecPos,
	const vector<XMFLOAT2> &vecUV,
	const vector<XMFLOAT3> &vecNor,
	const vector<Point> &vecPoint)
{
	uint32_t n = static_cast<uint32_t>(vecPoint.size());

	mVertexVec.resize(n);
	mIndexVec.resize(n);

	for (uint32_t i = 0; i < n; i += 3)
	{
		XMFLOAT3 p0 = vecPos[vecPoint[i].VI - 1];
		XMFLOAT3 p1 = vecPos[vecPoint[i + 1].VI - 1];
		XMFLOAT3 p2 = vecPos[vecPoint[i + 2].VI - 1];

		XMFLOAT2 w0 = vecUV[vecPoint[i].TI - 1];
		XMFLOAT2 w1 = vecUV[vecPoint[i + 1].TI - 1];
		XMFLOAT2 w2 = vecUV[vecPoint[i + 2].TI - 1];

		float x1 = p1.x - p0.x;
		float x2 = p2.x - p0.x;
		float y1 = p1.y - p0.y;
		float y2 = p2.y - p0.y;
		float z1 = p1.z - p0.z;
		float z2 = p2.z - p0.z;

		float s1 = w1.x - w0.x;
		float s2 = w2.x - w0.x;
		float t1 = w1.y - w0.y;
		float t2 = w2.y - w0.y;

		float r = 1.0f / (s1 * t2 - s2 * t1);
		XMFLOAT3 sdir(
			(t2 * x1 - t1 * x2) * r,
			(t2 * y1 - t1 * y2) * r,
			(t2 * z1 - t1 * z2) * r);
		XMFLOAT3 tdir(
			(s1 * x2 - s2 * x1) * r,
			(s1 * y2 - s2 * y1) * r,
			(s1 * z2 - s2 * z1) * r);

		XMStoreFloat3(&sdir, XMVector3Normalize(XMLoadFloat3(&sdir)));
		XMStoreFloat3(&tdir, XMVector3Normalize(XMLoadFloat3(&tdir)));

		for (uint32_t j = i; j < i + 3; j++)
		{
			XMFLOAT3 pos(0, 0, 0);
			XMFLOAT3 nor(0, 0, 0);
			XMFLOAT2 uv(0, 0);

			if (vecPoint[j].VI > 0) pos = vecPos[vecPoint[j].VI - 1];//index start at 1 in an .obj file but at 0 in an array, 0 was used to mark not-have-pos
			if (vecPoint[j].NI > 0) nor = vecNor[vecPoint[j].NI - 1];//index start at 1 in an .obj file but at 0 in an array, 0 was used to mark not-have-nor
			if (vecPoint[j].TI > 0) uv = vecUV[vecPoint[j].TI - 1];//index start at 1 in an .obj file but at 0 in an array, 0 was used to mark not-have-uv

			//The order is fixed, it is the same in the shaders. The only way to tell the direction when reconstructing tangent is the sign component.
			float dotProduct = 0;
			XMStoreFloat(&dotProduct, XMVector3Dot(XMVector3Cross(XMLoadFloat3(&nor), XMLoadFloat3(&sdir)), XMLoadFloat3(&tdir)));
			float sign = dotProduct > 0.0f ? 1.0f : -1.0f;
			XMFLOAT4 tan(sdir.x, sdir.y, sdir.z, sign);

			mVertexVec[j] = { pos, uv, nor, tan };
			mIndexVec[j] = j;//this way, all 3 vertices on every triangle are unique, even though they belong to the same polygon, which increase storing space but allow for finer control
		}
	}
}

void Mesh::SetLine()
{
	// start point is (0, 0, 0), end point is (0, 0, 1)
	mPrimitiveType = PrimitiveType::LINE;

	mVertexVec.resize(2);

	mVertexVec[0] = { {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f} };
	mVertexVec[1] = { {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f} };
	
	mIndexVec.resize(2);

	mIndexVec[0] = 0;
	mIndexVec[1] = 1;
}
