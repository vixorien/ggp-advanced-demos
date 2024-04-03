#ifndef __GGP_SHADER_STRUCTS__
#define __GPP_SHADER_STRUCTS__

// Structs for various shaders

// Basic VS input for a standard Pos/UV/Normal vertex
struct VertexShaderInput
{
	float3 localPosition	: POSITION;
	float2 uv				: TEXCOORD;
	float3 normal			: NORMAL;
	float3 tangent			: TANGENT;
};

// Basic VS input for a standard Pos/UV/Normal vertex
struct VertexShaderInput_Particle
{
	float3 localPosition	: POSITION;
	float2 uv				: TEXCOORD;
	float4 color			: COLOR;
};



// VS Output / PS Input struct for basic lighting
struct VertexToPixel
{
	float4 screenPosition	: SV_POSITION;
	float2 uv				: TEXCOORD;
	float3 normal			: NORMAL;
	float3 tangent			: TANGENT;
	float3 worldPos			: POSITION;
	float4 posForShadow		: SHADOWPOS;
};


// VStoPS struct for sky box
struct VertexToPixel_Sky
{
	float4 screenPosition	: SV_POSITION;
	float3 sampleDir		: DIRECTION;
};


// VStoPS struct for shadow map creation
struct VertexToPixel_Shadow
{
	float4 screenPosition	: SV_POSITION;
};

// VStoPS struct for particles
struct VertexToPixel_Particle
{
	float4 screenPosition	: SV_POSITION;
	float2 uv				: TEXCOORD;
	float4 color			: COLOR;
};


#endif