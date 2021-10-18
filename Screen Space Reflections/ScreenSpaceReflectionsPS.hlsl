
#include "Lighting.hlsli"

cbuffer externalData : register(b0)
{
	matrix invViewMatrix;
	matrix invProjMatrix;
	matrix viewMatrix;
	matrix projectionMatrix;

	float maxSearchDistance;
	float depthThickness;
	float edgeFadeThreshold;
	int maxMajorSteps;
	int maxRefinementSteps;
	float nearClip;
	float farClip;
};

struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float2 uv           : TEXCOORD0;
};

Texture2D SceneDirectLight		: register(t0);
Texture2D SceneIndirectSpecular	: register(t1);
Texture2D SceneAmbient			: register(t2);
Texture2D Normals				: register(t3);
Texture2D SpecColorRoughness	: register(t4);
Texture2D Depths				: register(t5);
Texture2D BRDFLookUp			: register(t6);
SamplerState BasicSampler		: register(s0);
SamplerState ClampSampler		: register(s1);

float3 ViewSpaceFromDepth(float2 uv, float depth)
{
	// Back to NDCs
	uv.y = 1.0f - uv.y; // Gotta do it this way
	uv = uv * 2.0f - 1.0f;
	//uv.y *= -1.0f; // Flip y due to UV <--> NDC 
	float4 screenPos = float4(uv, depth, 1.0f);

	// Back to view space
	float4 viewPos = mul(invProjMatrix, screenPos);
	return viewPos.xyz / viewPos.w;
}

float3 UVandDepthFromViewSpacePosition(float3 positionViewSpace)
{
	// Convert to float4, apply projection matrix
	// and perspective divide
	float4 screenPos = mul(projectionMatrix, float4(positionViewSpace, 1));
	screenPos /= screenPos.w;

	// Convert to UV from NDC
	screenPos.xy = screenPos.xy * 0.5f + 0.5f;
	screenPos.y = 1.0f - screenPos.y;
	return screenPos.xyz;
}

float3 ApplyPBRToReflection(float roughness, float3 normal, float3 view, float3 specColor, float3 reflectionColor)
{
	// Calculate half of the split-sum approx
	float NdotV = saturate(dot(normal, view));
	float2 indirectBRDF = BRDFLookUp.Sample(BasicSampler, float2(NdotV, roughness)).rg;
	float3 indSpecFresnel = specColor * indirectBRDF.x + indirectBRDF.y;

	// Adjust environment sample by fresnel
	return reflectionColor * indSpecFresnel;
}



// Might need: https://www.comp.nus.edu.sg/~lowkl/publications/lowk_persp_interp_techrep.pdf

// Not needed currently
static const float FLOAT32_MAX = 3.402823466e+38f;


float3 ScreenSpaceReflection(float2 thisUV, float thisDepth, float3 pixelPositionViewSpace, float3 reflViewSpace, float2 windowSize, out bool validHit, out float sceneDepthAtHit)
{
	// Assume no hit yet
	validHit = false;
	sceneDepthAtHit = 0.0f;

	// The origin is just this UV and its depth
	float3 originUVSpace = float3(thisUV, thisDepth);
	float3 endUVSpace = UVandDepthFromViewSpacePosition(pixelPositionViewSpace + reflViewSpace * maxSearchDistance);
	float3 deltaUVSpace = endUVSpace - originUVSpace;
	
	// The ray direction, also in UV/Depth space
	float3 rayDirUVSpace = normalize(deltaUVSpace);

	// Prepare to loop
	float t = 0;
	float stepSize = length(deltaUVSpace) / maxMajorSteps;
	float3 lastFailedPos = originUVSpace;
	for (int i = 0; i < maxMajorSteps; i++)
	{
		// Calculate how much to advance and adjust
		t += stepSize;
		float3 posUVSpace = originUVSpace + rayDirUVSpace * t;

		// Check depth here and compare
		float sampleDepth = Depths.SampleLevel(ClampSampler, posUVSpace.xy, 0).r;
		float depthDiff = posUVSpace.z - sampleDepth;
		if (depthDiff > 0)
		{
			// Successful hit, but we may be too deep into the depth buffer,
			// so binary search our way back towards the surface
			for (int r = 0; r < maxRefinementSteps; r++)
			{
				// Check mid-way between the last two spots
				float3 midPosUVSpace = lerp(lastFailedPos, posUVSpace, 0.5f);
				sampleDepth = Depths.SampleLevel(ClampSampler, midPosUVSpace.xy, 0).r;
				depthDiff = midPosUVSpace.z - sampleDepth;

				// What's our relationship to the surface here?
				if (depthDiff == 0) // Found the surface!
				{
					// Found it
					validHit = true;
					sceneDepthAtHit = sampleDepth;
					return posUVSpace;
				}
				else if (depthDiff < 0) // We're in front
				{
					lastFailedPos = midPosUVSpace;
				}
				else // We're behind
				{
					posUVSpace = midPosUVSpace;
				}
			}

			// The refinement failed, so we call it quits
			validHit = false;
			return posUVSpace;
		}

		// If we're here, we had a failed hit
		lastFailedPos = posUVSpace;
	}

	// Nothing useful found
	validHit = false;
	return originUVSpace;
}


float FadeReflections(bool validHit, float3 hitPos, float3 reflViewSpace, float3 pixelPositionViewSpace, float sceneDepthAtHit, float2 windowSize)
{
	// How strong are the SSR's?
	float fade = 1.0f;
	
	// Are we within the screen?
	if (!validHit || any(hitPos.xy < 0) || any(hitPos.xy > 1))
	{
		fade = 0.0f;
		// Should return here, but getting a warning if I do
		// Probably safe to ignore, but should look into it
	}

	// Check normal of hit to ensure we ignore backsides
	float3 hitNormal = Normals.Sample(ClampSampler, hitPos.xy).xyz;
	hitNormal = normalize(mul((float3x3)viewMatrix, hitNormal));
	if (dot(hitNormal, reflViewSpace) > 0)
	{
		fade = 0.0f;
		// Should return here, but getting a warning if I do
		// Probably safe to ignore, but should look into it
	}
	
	// Fade as the ray comes back towards the camera
	float backTowardsCamera = 1.0f - saturate(dot(reflViewSpace, -normalize(pixelPositionViewSpace)));
	//fade *= backTowardsCamera;

	// Fade as we get further from intersection point
	float3 scenePosViewSpace = ViewSpaceFromDepth(hitPos.xy, sceneDepthAtHit);
	float3 hitPosViewSpace = ViewSpaceFromDepth(hitPos.xy, hitPos.z);
	float distance = length(scenePosViewSpace - hitPosViewSpace);
	float depthFade = 1.0f - smoothstep(0, depthThickness, distance);//1.0f - saturate(distance / depthThickness);
	fade *= depthFade;

	// Fade as we approach max distance
	float maxDistFade = 1.0f - smoothstep(0, maxSearchDistance, length(pixelPositionViewSpace - hitPosViewSpace));// length(pixelPositionViewSpace - hitPosViewSpace) / maxSearchDistance;
	//fade *= maxDistFade;

	// Fade on screen edges
	float2 aspectRatio = float2(windowSize.y / windowSize.x, 1);
	float2 fadeThreshold = aspectRatio * edgeFadeThreshold;
	float2 topLeft = smoothstep(0, fadeThreshold, hitPos.xy); // Smooth fade between 0 coord and top/left fade edge
	float2 bottomRight = (1 - smoothstep(1 - fadeThreshold, 1, hitPos.xy)); // Smooth fade between bottom/right fade edge and 1 coord
	float2 screenEdgeFade = topLeft * bottomRight;
	fade *= screenEdgeFade.x * screenEdgeFade.y;

	// Return the final fade amount
	return fade;
}


float4 main(VertexToPixel input) : SV_TARGET
{
	// Sample depth first and early out for sky box
	float pixelDepth = Depths.Sample(ClampSampler, input.uv).r;
	if (pixelDepth == 1.0f)
		return float4(0,0,0,0);

	// Get the size of the window
	float2 windowSize = 0;
	SceneDirectLight.GetDimensions(windowSize.x, windowSize.y);

	// Get the view space position of this pixel
	float3 pixelPositionViewSpace = ViewSpaceFromDepth(input.uv, pixelDepth);
	
	// Sample normal and convert to view space
	float3 normal = Normals.Sample(BasicSampler, input.uv).xyz;// *2 - 1;
	float3 normalViewSpace = normalize(mul((float3x3)viewMatrix, normal));

	// Reflection in view space (treating position as a vector from camera to the pixel)
	float3 reflViewSpace = normalize(reflect(pixelPositionViewSpace, normalViewSpace));

	// Run the reflection to get the UV coord (and depth) of the hit
	bool validHit = false;
	float sceneDepthAtHit = 0;
	float3 hitPos = ScreenSpaceReflection(input.uv, pixelDepth, pixelPositionViewSpace, reflViewSpace, windowSize, validHit, sceneDepthAtHit);

	// Determine how much to fade (or completely cut) the reflections
	float fade = FadeReflections(validHit, hitPos, reflViewSpace, pixelPositionViewSpace, sceneDepthAtHit, windowSize);

	// Get the color at the hit position and interpolate as necessary
	float3 colorDirect = SceneDirectLight.Sample(ClampSampler, hitPos.xy).rgb;
	float4 colorIndirect = SceneIndirectSpecular.Sample(ClampSampler, hitPos.xy);
	float4 colorAmbient = SceneAmbient.Sample(ClampSampler, hitPos.xy);
	float isPBR = colorAmbient.a;

	float3 indirectTotal = colorIndirect.rgb + DiffuseEnergyConserve(colorAmbient.rgb, colorIndirect.rgb, colorIndirect.a);
	float3 reflectedColor = colorDirect + indirectTotal;

	// Handle tinting the reflection
	float4 specColorRough = SpecColorRoughness.Sample(ClampSampler, input.uv);
	float3 viewWorldSpace = -normalize(mul(invViewMatrix, float4(pixelPositionViewSpace, 1.0f)).xyz);
	reflectedColor = isPBR ? ApplyPBRToReflection(specColorRough.a, normal, viewWorldSpace, specColorRough.rgb, reflectedColor) : reflectedColor;
	
	// Combine colors
	float3 finalColor = reflectedColor * fade;
	return float4(finalColor, fade);
}