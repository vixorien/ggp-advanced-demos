
#include "ShaderStructs.hlsli"

cbuffer ExternalData : register(b0)
{
	matrix view;
	matrix projection;
}

// --------------------------------------------------------
// The entry point (main method) for our vertex shader
// --------------------------------------------------------
VertexToPixel_Sky main(VertexShaderInput input)
{
	// Set up output struct
	VertexToPixel_Sky output;

	// Modify the view matrix and remove the translation portion
	matrix viewNoTranslation = view;
	viewNoTranslation._14 = 0;
	viewNoTranslation._24 = 0;
	viewNoTranslation._34 = 0;

	// Multiply the view (without translation) and the projection
	matrix vp = mul(projection, viewNoTranslation);
	output.screenPosition = mul(vp, float4(input.localPosition, 1.0f));

	// For the sky vertex to be ON the far clip plane
	// (a.k.a. as far away as possible but still visible),
	// we can simply set the Z = W, since the xyz will 
	// automatically be divided by W in the rasterizer
	output.screenPosition.z = output.screenPosition.w;

	// Use the vert's position as the sample direction for the cube map!
	output.sampleDir = input.localPosition;

	// Whatever we return will make its way through the pipeline to the
	// next programmable stage we're using (the pixel shader for now)
	return output;
}