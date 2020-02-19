/////////////// UNIFORM ///////////////
//vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv//

// TODO: add support for shader space

cbuffer SceneUniform : register(b0, space0)
{
    uint mode;
};

cbuffer FrameUniform : register(b0, space1)
{
    uint frameIndex;
};

cbuffer PassUniform : register(b0, space2)
{
    uint passIndex;
	float vx;
	float vy;
	float vz;
    float4x4 viewProj;
    float4x4 viewProjInv;
};

cbuffer ObjectUniform : register(b0, space3)
{
    float4x4 model;
    float4x4 modelInv;
};

//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^//
/////////////// UNIFORM ///////////////

///////////////// VS /////////////////
//vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv//
struct VS_INPUT
{
    float3 pos : POSITION;
    float2 texCoord : TEXCOORD;
    float3 nor : NORMAL;
};

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float2 texCoord : TEXCOORD;
};
//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^//
///////////////// VS /////////////////

///////////////// PS /////////////////
//vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv//
struct PS_OUTPUT
{
    float4 col1 : SV_TARGET0;
};
//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^//
///////////////// PS /////////////////
