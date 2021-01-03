#pragma once

#include "GlobalInclude.h"
#include "Renderer.h"

class Buffer : public ShaderResource
{
public:
	Buffer(const wstring& debugName, u32 elementSizeInByte, u32 elementCount);
	virtual ~Buffer();

	ID3D12Resource* GetBuffer();
	D3D12_SHADER_RESOURCE_VIEW_DESC GetSrvDesc() const;

	void InitBuffer(Renderer* renderer);
	virtual void SetBufferData(void* data, int sizeInByte);
	virtual void Release(bool checkOnly = false);

protected:
	virtual void CreateBuffer();
	virtual void CreateView();
	View mSrv;
	u32 mElementSizeInByte;
	u32 mElementCount;
	Renderer* mRenderer;
	Resource* mBuffer;
};

class WriteBuffer : public Buffer
{
public:
	WriteBuffer(const wstring& debugName, u32 elementSizeInByte, u32 elementCount);
	virtual ~WriteBuffer();

	D3D12_UNORDERED_ACCESS_VIEW_DESC GetUavDesc() const;
	virtual void SetBufferData(void* data, int sizeInByte);
	virtual void Release(bool checkOnly = false);

protected:
	virtual void CreateBuffer();
	virtual void CreateView();

private:
	void CreateSrv();
	void CreateUav();

	View mUav;
};
