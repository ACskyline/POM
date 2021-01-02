#pragma once

#include <D3Dcompiler.h>
#include "GlobalInclude.h"
#include "Renderer.h"

class Shader
{
public:
	enum ShaderType { INVALID, VERTEX_SHADER, HULL_SHADER, DOMAIN_SHADER, GEOMETRY_SHADER, PIXEL_SHADER, COMPUTE_SHADER, COUNT };

	Shader(const ShaderType& type, const wstring& fileName);
	~Shader();

	void InitShader(Renderer* renderer);
	void CreateShaderFromFile(const wstring& fileName);
	D3D12_SHADER_BYTECODE GetShaderByteCode();
	ShaderType GetShaderType();

	void Release(bool checkOnly = false);

private:
	Renderer* mRenderer;
	ID3DBlob* mShader;
	ShaderType mType;
	wstring mFileName;
	D3D12_SHADER_BYTECODE mShaderBytecode;
};

