

// Defines the output data of our vertex shader
struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float2 uv			: TEXCOORD0;
};

// The entry point for our vertex shader
VertexToPixel main(uint id : SV_VertexID)
{
	// Set up output
	VertexToPixel output;

	// Calculate the UV (0,0) to (2,2) via the ID
	float2 uv = float2(
		(id << 1) & 2,  // id % 2 * 2
		id & 2);
	output.uv = uv;

	// Adjust the position based on the UV
	output.position = float4(uv, 0, 1);
	output.position.x = output.position.x * 2 - 1;
	output.position.y = output.position.y * -2 + 1;

	// Setting depth to one, assuming depth state comparison function is "GREATER",
	// which means the pixel shader should only run where the full screen triangle
	// rasterizes to a depth "behind" an existing surface (a.k.a. our scene geometry,
	// but not the "sky")
	output.position.z = 1; 
	return output;
}