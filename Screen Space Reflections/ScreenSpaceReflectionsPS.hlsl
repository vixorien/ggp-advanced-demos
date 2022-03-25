
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
	float roughnessThreshold;
	int maxMajorSteps;
	int maxRefinementSteps;
	int linearDepth;
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
float PerspectiveInterpolation(float depthStart, float depthEnd, float t)
{
	return 1.0f / lerp(1.0f / depthStart, 1.0f / depthEnd, t);
}

bool OutsideScreen(float2 uv)
{
	return any(uv < 0) || any(uv > 1);
}

// Not needed currently
static const float FLOAT32_MAX = 3.402823466e+38f;


float3 ScreenSpaceReflection(float2 thisUV, float thisDepth, float3 pixelPositionViewSpace, float3 reflViewSpace, float2 windowSize, out bool validHit, out float sceneDepthAtHit, out float4 debug)
{
	// Assume no hit yet
	validHit = false;
	sceneDepthAtHit = 0.0f;
	debug = float4(1,1,1,1);

	// The origin is just this UV and its depth
	float3 originUVSpace = float3(thisUV, thisDepth);
	float3 endUVSpace = UVandDepthFromViewSpacePosition(pixelPositionViewSpace + reflViewSpace * maxSearchDistance);

	if (linearDepth)
	{
		originUVSpace.z = LinearDepth(originUVSpace.z, nearClip, farClip);
		endUVSpace.z = LinearDepth(endUVSpace.z, nearClip, farClip);
	}

	float3 rayUVSpace = endUVSpace - originUVSpace;

	// Prepare to loop
	float t = 0;
	float3 lastFailedPos = originUVSpace;
	for (int i = 1; i <= maxMajorSteps; i++)
	{
		// Calculate how much to advance and adjust
		t = (float)i / maxMajorSteps;
		float3 posUVSpace = originUVSpace + rayUVSpace * t;
		posUVSpace.z = PerspectiveInterpolation(originUVSpace.z, endUVSpace.z, t);

		// Check depth here and compare
		float sampleDepth = Depths.SampleLevel(ClampSampler, posUVSpace.xy, 0).r;
		if (linearDepth)
		{
			sampleDepth = LinearDepth(sampleDepth, nearClip, farClip);
		}

		float minDepthHit = 0.01f;

		float depthDiff = posUVSpace.z - sampleDepth;
		if (depthDiff > depthThickness)
		{
			// No hit, too far
			validHit = false;
			debug.rgb = float3(0, 1, 1);
			return originUVSpace;
		}
		else if (depthDiff >= 0 && depthDiff <= minDepthHit)
		{
			// Hit, no need for refinement
			validHit = true;
			sceneDepthAtHit = sampleDepth;
			debug.rgb = float3(t, 0, 0);
			return posUVSpace;
		}
		else if (depthDiff > minDepthHit)
		{
			// Successful hit, but we're too deep into the depth buffer,
			// so binary search our way back towards the surface
			float3 midPosUVSpace = posUVSpace; // Set to something in case we never end loop
			for (int r = 0; r < maxRefinementSteps; r++)
			{
				// Check mid-way between the last two spots
				midPosUVSpace = lerp(lastFailedPos, posUVSpace, 0.5f);
				midPosUVSpace.z = PerspectiveInterpolation(lastFailedPos.z, posUVSpace.z, 0.5f);

				sampleDepth = Depths.SampleLevel(ClampSampler, midPosUVSpace.xy, 0).r;

				if (linearDepth)
				{
					sampleDepth = LinearDepth(sampleDepth, nearClip, farClip);
				}

				depthDiff = midPosUVSpace.z - sampleDepth;

				// What's our relationship to the surface here?
				if (depthDiff >= 0 && depthDiff <= minDepthHit)
				{
					// Found it
					validHit = true;
					sceneDepthAtHit = sampleDepth;
					debug.rgb = float3(t, 0, 0);
					return midPosUVSpace;
				}
				else if (depthDiff < 0) // We're in front of the surface
				{
					lastFailedPos = midPosUVSpace;
				}
				else // We're behind the surface
				{
					posUVSpace = midPosUVSpace;
				}
			}

			// The refinement failed, so we call it quits
			validHit = false;
			debug.rgb = float3(0, 1, 0);
			return originUVSpace;
		}

		// If we're here, we had a failed hit
		lastFailedPos = posUVSpace;
	}

	// Nothing useful found
	validHit = false;
	debug.rgb = float3(0, 0, 1);
	return originUVSpace;
}


float FadeReflections(bool validHit, float3 hitPos, float3 reflViewSpace, float3 pixelPositionViewSpace, float sceneDepthAtHit, float2 windowSize)
{
	// How strong are the SSR's?
	float fade = 1.0f;
	
	// Are we within the screen?
	if (!validHit || OutsideScreen(hitPos.xy))
	{
		fade = 0.0f;
		// Should return here, but getting a warning if I do
		// Probably safe to ignore, but should look into it
	}

	// Check normal of hit to ensure we ignore backsides
	//float3 hitNormal = Normals.Sample(ClampSampler, hitPos.xy).xyz;
	//hitNormal = normalize(mul((float3x3)viewMatrix, hitNormal));
	//if (dot(hitNormal, reflViewSpace) > 0)
	//{
	//	fade = 0.0f;
	//	// Should return here, but getting a warning if I do
	//	// Probably safe to ignore, but should look into it
	//}
	
	// Fade as the ray comes back towards the camera
	float backTowardsCamera = 1.0f - saturate(dot(reflViewSpace, -normalize(pixelPositionViewSpace)));
	fade *= backTowardsCamera;

	// Fade as we get further from intersection point
	//float3 scenePosViewSpace = ViewSpaceFromDepth(hitPos.xy, sceneDepthAtHit); // Broken if we're using linear depth!
	//float3 hitPosViewSpace = ViewSpaceFromDepth(hitPos.xy, hitPos.z);
	//float distance = length(scenePosViewSpace - hitPosViewSpace);
	//float depthFade = 1.0f - smoothstep(0, depthThickness, distance);//1.0f - saturate(distance / depthThickness);
	//fade *= depthFade;

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

	// Get the specular color and roughness of this pixel,
	// then test to see if this pixel is too rough for reflections
	float4 specColorRough = SpecColorRoughness.Sample(ClampSampler, input.uv);
	if (specColorRough.a > roughnessThreshold)
		return float4(0, 0, 0, 0);

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
	float4 debug = float4(0,0,0,1);
	float3 hitPos = ScreenSpaceReflection(input.uv, pixelDepth, pixelPositionViewSpace, reflViewSpace, windowSize, validHit, sceneDepthAtHit, debug);
	//return debug;
	
	// Determine how much to fade (or completely cut) the reflections
	float fade = FadeReflections(validHit, hitPos, reflViewSpace, pixelPositionViewSpace, sceneDepthAtHit, windowSize);

	// Get the color at the hit position and interpolate as necessary
	float3 colorDirect = SceneDirectLight.Sample(ClampSampler, hitPos.xy).rgb;
	float4 colorIndirect = SceneIndirectSpecular.Sample(ClampSampler, hitPos.xy);
	float4 colorAmbient = SceneAmbient.Sample(ClampSampler, hitPos.xy);
	float isPBR = colorAmbient.a;

	float3 indirectTotal = colorIndirect.rgb + DiffuseEnergyConserve(colorAmbient.rgb, specColorRough.rgb, colorIndirect.a);
	float3 reflectedColor = colorDirect + indirectTotal;

	// Handle tinting the reflection
	float3 viewWorldSpace = -normalize(mul(invViewMatrix, float4(pixelPositionViewSpace, 1.0f)).xyz);
	reflectedColor = isPBR ? ApplyPBRToReflection(specColorRough.a, normal, viewWorldSpace, specColorRough.rgb, reflectedColor) : reflectedColor;
	
	// Combine colors
	float3 finalColor = reflectedColor;
	return float4(finalColor, fade);
}