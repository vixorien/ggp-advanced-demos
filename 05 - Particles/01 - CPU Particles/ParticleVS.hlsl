
#include "ShaderStructs.hlsli"

// Constant buffer for C++ data being passed in
cbuffer externalData : register(b0)
{
    matrix world;
    matrix view;
    matrix projection;
};


// The entry point for our vertex shader
VertexToPixel_Particle main(VertexShaderInput_Particle input)
{
    // Set up output
    VertexToPixel_Particle output;

    // Calculate output position
    matrix wvp = mul(projection, mul(view, world));
    output.screenPosition = mul(wvp, float4(input.localPosition, 1.0f));

	// Pass other data through
	output.uv = input.uv;
    output.color = input.color;
   
    return output;
}