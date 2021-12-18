#pragma once

#include "GlobalInclude.h"
#include "Renderer.h"

class Buffer : public ShaderResource
{
public:
	Buffer(const string& debugName);
	Buffer(const string& debugName, u32 elementSizeInByte, u32 elementCount);
	virtual ~Buffer();

	ID3D12Resource* GetBuffer();
	D3D12_SHADER_RESOURCE_VIEW_DESC GetSrvDesc() const;
	u32 GetElementCount() const { return mElementCount; }

	void InitBuffer(Renderer* renderer);
	void SetElementSizeAndCount(int sizeInByte, int count);

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
	WriteBuffer(const string& debugName);
	WriteBuffer(const string& debugName, u32 elementSizeInByte, u32 elementCount);
	virtual ~WriteBuffer();

	D3D12_UNORDERED_ACCESS_VIEW_DESC GetUavDesc() const;
	void RecordSetBufferData(CommandList commandList, void* data, int sizeInByte);
	void RecordPrepareToGetBufferData(CommandList commandList, int sizeInByte);
	virtual void SetBufferData(void* data, int sizeInByte);
	virtual void Release(bool checkOnly = false);
	void GetReadbackBufferData(void* data, int sizeInByte);
	void GetBufferData(void* data, int sizeInByte);
	void MakeReadyToRead(CommandList commandList);
	void MakeReadyToWrite(CommandList commandList);
	void MakeReadyToWriteAgain(CommandList commandList);
	
protected:
	virtual void CreateBuffer();
	virtual void CreateView();
	bool TransitionLayout(CommandList commandList, ResourceLayout newLayout);

private:
	void SetBufferDataInternal(CommandList commandList, void* data, int sizeInByte);
	void GetBufferDataInternal(CommandList commandList, void* data, int sizeInByte);
	void CreateSrv();
	void CreateUav();

	View mUav;
	ResourceLayout mLayout;
	Resource* mUploadBuffer;
	Resource* mReadbackBuffer;
};
