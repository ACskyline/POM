#pragma once

#include "GlobalInclude.h"
#include "Renderer.h"

class Mesh
{
public:
	enum class MeshType
	{ 
		PLANE, 
		CUBE, 
		FULLSCREEN_QUAD, 
		FULLSCREEN_TRIANGLE, 
		SKY_FULLSCREEN_TRIANGLE, // z value is at the farthest
		MESH, 
		LINE,
		COUNT 
	};

	Mesh(const string& debugName,
		const MeshType& type,
		const XMFLOAT3& position,
		const XMFLOAT3& rotation,
		const XMFLOAT3& scale,
		const string& fileName = "null");
	~Mesh();

	void ResetMesh(MeshType type,
		const XMFLOAT3&position,
		const XMFLOAT3&scale,
		const XMFLOAT3&rotation);

	void InitMesh(
		Renderer* renderer, 
		int frameCount, 
		DescriptorHeap& cbvSrvUavDescriptorHeap);
	
	void CreateUniformBuffer(int frameCount);
	void UpdateUniformBuffer(int frameIndex);

	void CreateVertexBuffer();
	void CreateIndexBuffer();
	void UpdateVertexBuffer();
	void UpdateIndexBuffer();

	void Release(bool checkOnly = false);

	PrimitiveType GetPrimitiveType() const;
	D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const;
	D3D12_INDEX_BUFFER_VIEW GetIndexBufferView() const;
	D3D12_GPU_VIRTUAL_ADDRESS GetUniformBufferGpuAddress(int frameIndex) const; 
	DescriptorHeap::Handle GetCbvSrvUavDescriptorHeapTableHandle(int frameIndex) const;
	
	void AddTexture(Texture* texture);
	void SetPosition(const XMFLOAT3&position);
	void SetRotation(const XMFLOAT3&rotation);
	XMFLOAT3 GetPosition();
	XMFLOAT3 GetRotation();
	int GetIndexCount() const;
	int GetTextureCount() const;
	vector<Texture*>& GetTextures();

	void ConvertMeshToTrianglesPT(vector<TrianglePT>& outTriangles, u32 meshIndex);
	Vertex TransformVertexToWorldSpace(const Vertex& vertex, const XMFLOAT4X4& m, const XMFLOAT4X4& mInv);

	ObjectUniform mObjectUniform;

	inline static u32 GenerateMortonCode(XMFLOAT3 position);

private:
	struct Point
	{
		uint32_t VI;
		uint32_t TI;
		uint32_t NI;
	};

	string mFileName;
	string mDebugName;
	MeshType mType;
	XMFLOAT3 mPosition;
	XMFLOAT3 mScale;
	XMFLOAT3 mRotation;

	vector<Vertex> mVertexVec;
	vector<uint32_t> mIndexVec;
	vector<Texture*> mTextures;

	Renderer* mRenderer;

	UINT mVertexBufferSizeInBytes;
	UINT mIndexBufferSizeInBytes;

	// TODO: hide API specific implementation in Renderer
	ID3D12Resource* mVertexBuffer;
	ID3D12Resource* mIndexBuffer;
	D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;
	D3D12_INDEX_BUFFER_VIEW mIndexBufferView;
	PrimitiveType mPrimitiveType;
	vector<ID3D12Resource*> mUniformBufferVec;
	vector<DescriptorHeap::Handle> mCbvSrvUavDescriptorHeapTableHandleVec;

	void UpdateMatrix();
	void SetCube();
	void SetPlane();
	void SetFullscreenQuad();
	void SetFullscreenTriangleDepth(float d);
	void SetFullscreenTriangle();
	void SetSkyFullscreenTriangle();
	void SetMesh();
	void SetLine();

	void AssembleObjMesh(
		const vector<XMFLOAT3> &vecPos,
		const vector<XMFLOAT2> &vecUV,
		const vector<XMFLOAT3> &vecNor,
		const vector<Point> &vecPoint);
	
	void ParseObjFace(
		stringstream &ss, 
		vector<Point>& tempVecPoint);
};
