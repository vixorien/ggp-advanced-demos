
#include "Lighting.hlsli"

// How many lights could we handle?
#define MAX_LIGHTS 128

// Data that only changes once per frame
cbuffer perFrame : register(b0)
{
	// An array of light data
	Light Lights[MAX_LIGHTS];

	// The amount of lights THIS FRAME
	int LightCount;

	// Needed for specular (reflection) calculation
	float3 CameraPosition;

	// The number of mip levels in the specular IBL map (only used for PBR)
	int SpecIBLTotalMipLevels;

	// Ambient color for the environment
	float3 AmbientNonPBR;
};

// Data that can change per material
cbuffer perMaterial : register(b1)
{
	// Surface color
	float4 Color;

	// Material information
	float Shininess;
};


// Defines the input to this pixel shader
// - Should match the output of our corresponding vertex shader
struct VertexToPixel
{
	float4 screenPosition	: SV_POSITION;
	float2 uv				: TEXCOORD;
	float3 normal			: NORMAL;
	float3 tangent			: TANGENT;
	float3 worldPos			: POSITION; // The world position of this PIXEL
};

struct PS_Output
{
	float4 colorNoAmbient	: SV_TARGET0;
	float4 ambientColor		: SV_TARGET1;
	float4 normals			: SV_TARGET2;
	float depths			: SV_TARGET3;
};

// Texture-related variables
Texture2D AlbedoTexture			: register(t0);
Texture2D NormalTexture			: register(t1);
Texture2D RoughnessTexture		: register(t2);
SamplerState BasicSampler		: register(s0);


// Entry point for this pixel shader
PS_Output main(VertexToPixel input)
{
	// Always re-normalize interpolated direction vectors
	input.normal = normalize(input.normal);
	input.tangent = normalize(input.tangent);

	// Normal mapping
	input.normal = NormalMapping(NormalTexture, BasicSampler, input.uv, input.normal, input.tangent);
	
	// Treating roughness as a pseduo-spec map here, so applying it as
	// a modifier to the overall shininess value of the material
	float roughness = RoughnessTexture.Sample(BasicSampler, input.uv).r;
	float specPower = max(Shininess * (1.0f - roughness), 0.01f); // Ensure we never hit 0
	
	// Gamma correct the texture back to linear space and apply the color tint
	float4 surfaceColor = AlbedoTexture.Sample(BasicSampler, input.uv);
	surfaceColor.rgb = pow(surfaceColor.rgb, 2.2) * Color.rgb;

	// Total color for this pixel
	float3 totalColor = float3(0,0,0);

	// Loop through all lights this frame
	for(int i = 0; i < LightCount; i++)
	{
		// Which kind of light?
		switch (Lights[i].Type)
		{
		case LIGHT_TYPE_DIRECTIONAL:
			totalColor += DirLight(Lights[i], input.normal, input.worldPos, CameraPosition, specPower, surfaceColor.rgb);
			break;

		case LIGHT_TYPE_POINT:
			totalColor += PointLight(Lights[i], input.normal, input.worldPos, CameraPosition, specPower, surfaceColor.rgb);
			break;

		case LIGHT_TYPE_SPOT:
			totalColor += SpotLight(Lights[i], input.normal, input.worldPos, CameraPosition, specPower, surfaceColor.rgb);
			break;
		}
	}

	// Handle ambient
	float3 ambient = surfaceColor.rgb * AmbientNonPBR;

	// Multiple render target output
	float gammaPower = 1.0f / 2.2f;
	PS_Output output;
	output.colorNoAmbient	= float4(pow(totalColor, gammaPower), 1); // Gamma correction
	output.ambientColor		= float4(pow(ambient, gammaPower), 1);
	output.normals			= float4(input.normal * 0.5f + 0.5f, 1);
	output.depths			= input.screenPosition.z;
	return output;
}