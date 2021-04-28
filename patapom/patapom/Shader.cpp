#include "Shader.h"

Shader::Shader(const ShaderType& type, const wstring& fileName) :
	mType(type),
	mFileName(fileName)
{
}

Shader::~Shader()
{
	Release(true);
}

void Shader::InitShader(Renderer* renderer)
{
	mRenderer = renderer;
	CreateShaderFromFile(mFileName);
}

void Shader::CreateShaderFromFile(const wstring& fileName)
{
	HRESULT hr;
	// create vertex and pixel shaders

	// when debugging, we can compile the shader files at runtime.
	// but for release versions, we can compile the hlsl shaders
	// with fxc.exe to create .cso files, which contain the shader
	// bytecode. We can load the .cso files at runtime to get the
	// shader bytecode, which of course is faster than compiling
	// them at runtime

	// compile vertex shader
	ID3DBlob* errorBuff = nullptr; // a buffer holding the error data if any
	
	string shaderType;
	// using 5_1 instead of 5_0 to use shader space
	switch(mType)
	{
	case ShaderType::VERTEX_SHADER:
		shaderType = "vs_5_1";
		break;
	case ShaderType::HULL_SHADER:
		shaderType = "hs_5_1";
		break;
	case ShaderType::DOMAIN_SHADER:
		shaderType = "ds_5_1";
		break;
	case ShaderType::GEOMETRY_SHADER:
		shaderType = "gs_5_1";
		break;
	case ShaderType::PIXEL_SHADER:
		shaderType = "ps_5_1";
		break;
	case ShaderType::COMPUTE_SHADER:
		shaderType = "cs_5_1";
		break;
	default:
		fatalf("wrong shader type");
		break;
	}

	UINT compileFlag = D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES;
	if (mRenderer->mDebugMode != DebugMode::OFF)
		compileFlag |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;

	hr = D3DCompileFromFile(fileName.c_str(),
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main",
		shaderType.c_str(),
		compileFlag,
		0,
		&mShader,
		&errorBuff);
	
	if (FAILED(hr))
	{
		CheckError(hr, nullptr, errorBuff);
		fatalf("shader compiling failed");
	}

	SAFE_RELEASE_NO_CHECK(errorBuff);
	// fill out a shader bytecode structure, which is basically just a pointer
	// to the shader bytecode and the size of the shader bytecode
	mShaderBytecode.BytecodeLength = mShader->GetBufferSize();
	mShaderBytecode.pShaderBytecode = mShader->GetBufferPointer();
}

D3D12_SHADER_BYTECODE Shader::GetShaderByteCode()
{
	fatalAssertf(mShaderBytecode.pShaderBytecode, "shader is not loaded");
	return mShaderBytecode;
}

Shader::ShaderType Shader::GetShaderType()
{
	return mType;
}

void Shader::Release(bool checkOnly)
{
	SAFE_RELEASE(mShader, checkOnly);
}
