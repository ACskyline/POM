#pragma once

class RenderTexture;
class PassDefault;
class Shader;

namespace DeferredLighting 
{
	extern RenderTexture gRenderTextureGbuffer0;
	extern RenderTexture gRenderTextureGbuffer1;
	extern RenderTexture gRenderTextureGbuffer2;
	extern RenderTexture gRenderTextureGbuffer3;
	extern RenderTexture gRenderTextureDbuffer;
	extern PassDefault gPassDeferred;
	extern Shader gDeferredVS;
	extern Shader gDeferredPS;
}
