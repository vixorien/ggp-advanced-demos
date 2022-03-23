
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

	// The number of mip levels in the specular IBL map
	int SpecIBLTotalMipLevels;

	// Ambient color for the environment
	float3 AmbientNonPBR;

	// Motion blur data
	int MotionBlurMax;
	float2 ScreenSize;

	float pad;
};

// Data that can change per material
cbuffer perMaterial : register(b1)
{
	// Surface color
	float4 Color;
};


// Defines the input to this pixel shader
// - Should match the output of our corresponding vertex shader
struct VertexToPixel
{
	float4 screenPosition	: SV_POSITION;
	float4 screenPosCurrent : SCREEN_POS0;
	float4 screenPosPrev	: SCREEN_POS1;
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
	float2 velocity			: SV_TARGET4;
};


// Texture-related variables
Texture2D AlbedoTexture			: register(t0);
Texture2D NormalTexture			: register(t1);
Texture2D RoughnessTexture		: register(t2);
Texture2D MetalTexture			: register(t3);

// IBL (indirect PBR) textures
Texture2D BrdfLookUpMap			: register(t4);
TextureCube IrradianceIBLMap	: register(t5);
TextureCube SpecularIBLMap		: register(t6);

// Samplers
SamplerState BasicSampler		: register(s0);
SamplerState ClampSampler		: register(s1);

// Entry point for this pixel shader
PS_Output main(VertexToPixel input)
{
	// Always re-normalize interpolated direction vectors
	input.normal = normalize(input.normal);
	input.tangent = normalize(input.tangent);

	// Sample various textures
	input.normal = NormalMapping(NormalTexture, BasicSampler, input.uv, input.normal, input.tangent);
	float roughness = RoughnessTexture.Sample(BasicSampler, input.uv).r;
	float metal = MetalTexture.Sample(BasicSampler, input.uv).r;

	// Gamma correct the texture back to linear space and apply the color tint
	float4 surfaceColor = AlbedoTexture.Sample(BasicSampler, input.uv);
	surfaceColor.rgb = pow(surfaceColor.rgb, 2.2) * Color.rgb;

	// Specular color - Assuming albedo texture is actually holding specular color if metal == 1
	// Note the use of lerp here - metal is generally 0 or 1, but might be in between
	// because of linear texture sampling, so we want lerp the specular color to match
	float3 specColor = lerp(F0_NON_METAL.rrr, surfaceColor.rgb, metal);

	// Total color for this pixel
	float3 totalDirectLight = float3(0,0,0);

	// Loop through all lights this frame
	for(int i = 0; i < LightCount; i++)
	{
		// Which kind of light?
		switch (Lights[i].Type)
		{
		case LIGHT_TYPE_DIRECTIONAL:
			totalDirectLight += DirLightPBR(Lights[i], input.normal, input.worldPos, CameraPosition, roughness, metal, surfaceColor.rgb, specColor);
			break;

		case LIGHT_TYPE_POINT:
			totalDirectLight += PointLightPBR(Lights[i], input.normal, input.worldPos, CameraPosition, roughness, metal, surfaceColor.rgb, specColor);
			break;

		case LIGHT_TYPE_SPOT:
			totalDirectLight += SpotLightPBR(Lights[i], input.normal, input.worldPos, CameraPosition, roughness, metal, surfaceColor.rgb, specColor);
			break;
		}
	}

	// Calculate requisite reflection vectors
	float3 viewToCam = normalize(CameraPosition - input.worldPos);
	float3 viewRefl = normalize(reflect(-viewToCam, input.normal));
	float NdotV = saturate(dot(input.normal, viewToCam));

	// Indirect lighting
	float3 indirectDiffuse = IndirectDiffuse(IrradianceIBLMap, BasicSampler, input.normal);
	float3 indirectSpecular = IndirectSpecular(
		SpecularIBLMap, SpecIBLTotalMipLevels,
		BrdfLookUpMap, ClampSampler, // MUST use the clamp sampler here!
		viewRefl, NdotV,
		roughness, specColor);

	// Balance indirect diff/spec
	float3 balancedIndirectDiff = DiffuseEnergyConserve(indirectDiffuse, indirectSpecular, metal) * surfaceColor.rgb;

	// Calculate velocity
	float2 thisPos = input.screenPosCurrent.xy / input.screenPosCurrent.w;
	float2 prevPos = input.screenPosPrev.xy / input.screenPosPrev.w;
	float2 velocity = thisPos - prevPos;
	velocity.y *= -1;
	
	// Scale to pixels and clamp to max
	velocity *= ScreenSize / 2;
	float magnitude = length(velocity);
	if (magnitude > MotionBlurMax)
	{
		velocity = normalize(velocity) * MotionBlurMax;
	}

	// Multiple render target output
	PS_Output output;
	output.colorNoAmbient	= float4(totalDirectLight + indirectSpecular, 1); // No gamma correction yet!
	output.ambientColor		= float4(balancedIndirectDiff, 1); // Gamma correction
	output.normals			= float4(input.normal * 0.5f + 0.5f, 1);
	output.depths			= input.screenPosition.z;
	output.velocity			= velocity;
	return output;
}