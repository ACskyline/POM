#include "DeferredLighting.h"
#include "Texture.h"
#include "Pass.h"
#include "Shader.h"

RenderTexture DeferredLighting::gRenderTextureGbuffer0(TextureType::TEX_2D, "gbuffer0", gWidthDeferred, gHeightDeferred, 1, 1, ReadFrom::COLOR, Format::R16G16B16A16_UNORM, XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
RenderTexture DeferredLighting::gRenderTextureGbuffer1(TextureType::TEX_2D, "gbuffer1", gWidthDeferred, gHeightDeferred, 1, 1, ReadFrom::COLOR, Format::R16G16B16A16_UNORM, XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
RenderTexture DeferredLighting::gRenderTextureGbuffer2(TextureType::TEX_2D, "gbuffer2", gWidthDeferred, gHeightDeferred, 1, 1, ReadFrom::COLOR, Format::R16G16B16A16_UNORM, XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
RenderTexture DeferredLighting::gRenderTextureGbuffer3(TextureType::TEX_2D, "gbuffer3", gWidthDeferred, gHeightDeferred, 1, 1, ReadFrom::COLOR, Format::R16G16B16A16_UNORM, XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
RenderTexture DeferredLighting::gRenderTextureDbuffer(TextureType::TEX_2D, "dbuffer", gWidthDeferred, gHeightDeferred, 1, 1, ReadFrom::DEPTH, Format::D16_UNORM, DEPTH_FAR_REVERSED_Z_SWITCH, 0);
PassDefault DeferredLighting::gPassDeferred("deferred pass", true, false);
Shader DeferredLighting::gDeferredVS(Shader::ShaderType::VERTEX_SHADER, "vs_deferred");
Shader DeferredLighting::gDeferredPS(Shader::ShaderType::PIXEL_SHADER, "ps_deferred"); // using multisampled srv
