
// The variables defined in this cbuffer will pull their data from the 
// constant buffer (ID3D11Buffer) bound to "vertex shader constant buffer slot 0"
// It was bound using context->VSSetConstantBuffers() over in C++.
cbuffer ExternalData : register(b0)
{
	matrix view;
	matrix projection;
}

// Struct representing a single vertex worth of data
struct VertexShaderInput
{
	float3 position		: POSITION;     // XYZ position
	float2 uv			: TEXCOORD;
	float3 normal		: NORMAL;
	float3 tangent		: TANGENT;
};

// Struct representing the data we're sending down the pipeline
struct VertexToPixel
{
	float4 position		: SV_POSITION;	// XYZW position (System Value Position)
	float3 sampleDir	: DIRECTION;
};

// --------------------------------------------------------
// The entry point (main method) for our vertex shader
// --------------------------------------------------------
VertexToPixel main(VertexShaderInput input)
{
	// Set up output struct
	VertexToPixel output;

	// Modify the view matrix and remove the translation portion
	matrix viewNoTranslation = view;
	viewNoTranslation._14 = 0;
	viewNoTranslation._24 = 0;
	viewNoTranslation._34 = 0;

	// Multiply the view (without translation) and the projection
	matrix vp = mul(projection, viewNoTranslation);
	output.position = mul(vp, float4(input.position, 1.0f));

	// For the sky vertex to be ON the far clip plane
	// (a.k.a. as far away as possible but still visible),
	// we can simply set the Z = W, since the xyz will 
	// automatically be divided by W in the rasterizer
	output.position.z = output.position.w;

	// Use the vert's position as the sample direction for the cube map!
	output.sampleDir = input.position;

	// Whatever we return will make its way through the pipeline to the
	// next programmable stage we're using (the pixel shader for now)
	return output;
}