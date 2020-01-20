#include "Shader.h"

Shader::Shader(const ShaderType& type, const wstring& fileName) :
	mType(type),
	mFileName(fileName)
{
}

Shader::~Shader()
{
	Release();
}

void Shader::InitShader()
{

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
	ID3DBlob* errorBuff; // a buffer holding the error data if any
	
	string shaderType;
	switch(mType)
	{
	case ShaderType::VERTEX_SHADER:
		shaderType = "vs_5_0";
		break;
	case ShaderType::HULL_SHADER:
		shaderType = "hs_5_0";
		break;
	case ShaderType::DOMAIN_SHADER:
		shaderType = "ds_5_0";
		break;
	case ShaderType::GEOMETRY_SHADER:
		shaderType = "gs_5_0";
		break;
	case ShaderType::PIXEL_SHADER:
		shaderType = "ps_5_0";
		break;
	default:
		fatalf("wrong shader type");
		break;
	}

	hr = D3DCompileFromFile(fileName.c_str(),
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main",
		shaderType.c_str(),
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&mShader,
		&errorBuff);
	
	if (FAILED(hr))
	{
		CheckError(hr, errorBuff);
		fatalf("shader compiling failed");
	}

	SAFE_RELEASE(errorBuff);
	// fill out a shader bytecode structure, which is basically just a pointer
	// to the shader bytecode and the size of the shader bytecode
	mShaderBytecode.BytecodeLength = mShader->GetBufferSize();
	mShaderBytecode.pShaderBytecode = mShader->GetBufferPointer();
}

D3D12_SHADER_BYTECODE Shader::GetShaderByteCode()
{
	return mShaderBytecode;
}

Shader::ShaderType Shader::GetShaderType()
{
	return mType;
}

void Shader::Release()
{
	SAFE_RELEASE(mShader);
}
