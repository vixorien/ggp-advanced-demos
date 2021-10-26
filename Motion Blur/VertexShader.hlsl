
// Data that changes at most once per frame
cbuffer perFrame : register(b0)
{
	matrix view;
	matrix projection;
	matrix prevView;
	matrix prevProjection;
};

// Data that can change per material
cbuffer perMaterial : register(b1)
{
	float2 uvScale;
};

// Data that can change per object
cbuffer perObject : register(b2)
{
	matrix world;
	matrix prevWorld;
	matrix worldInverseTranspose;
};


// Struct representing a single vertex worth of data
struct VertexShaderInput
{
	float3 position		: POSITION;
	float2 uv			: TEXCOORD;
	float3 normal		: NORMAL;
	float3 tangent		: TANGENT;
};

// Out of the vertex shader (and eventually input to the PS)
struct VertexToPixel
{
	float4 screenPosition	: SV_POSITION;
	float4 screenPosCurrent : SCREEN_POS0;
	float4 screenPosPrev	: SCREEN_POS1;
	float2 uv				: TEXCOORD;
	float3 normal			: NORMAL;
	float3 tangent			: TANGENT;
	float3 worldPos			: POSITION; // The world position of this vertex
};

// --------------------------------------------------------
// The entry point (main method) for our vertex shader
// --------------------------------------------------------
VertexToPixel main(VertexShaderInput input)
{
	// Set up output
	VertexToPixel output;

	// Calculate output position
	matrix worldViewProj = mul(projection, mul(view, world));
	output.screenPosition = mul(worldViewProj, float4(input.position, 1.0f));
	output.screenPosCurrent = output.screenPosition;

	// Calculate prev frame's position
	matrix prevWVP = mul(prevProjection, mul(prevView, prevWorld));
	output.screenPosPrev = mul(prevWVP, float4(input.position, 1.0f));

	// Calculate the world position of this vertex (to be used
	// in the pixel shader when we do point/spot lights)
	output.worldPos = mul(world, float4(input.position, 1.0f)).xyz;

	// Make sure the normal is in WORLD space, not "local" space
	output.normal = normalize(mul((float3x3)worldInverseTranspose, input.normal));
	output.tangent = normalize(mul((float3x3)worldInverseTranspose, input.tangent));

	// Pass through the uv
	output.uv = input.uv * uvScale;

	return output;
}