
// Defines the output data of our vertex shader
struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float2 uv           : TEXCOORD0;
};


// This vertex shader receives one piece of input: an id (ideally from 0 - 2, inclusive)
// 
// From those values (0, 1 or 2), it creates uv coords and a screen position.
// If done correctly, the triangle that arises from those three vertices will
// perfectly fill the screen, like so:
//  ________________
// |- - - - - - - - |- - - - - - - - uv = (2, 0)
// |- uv = (0,0)    |           -
// |-               |        -
// |-               |     -
// |-               | -
// |________________|
//  -           -
//  -       -
//  -   -
//  -
//  uv = (0, 2)
//

VertexToPixel main(uint id : SV_VertexID)
{
	// Set up output
	VertexToPixel output;

	// Calculate the UV (0,0) to (2,2) via the ID
	output.uv = float2(
		(id << 1) & 2,  // id % 2 * 2
		id & 2);

	// Adjust the position based on the UV
	output.position = float4(output.uv, 0, 1);
	output.position.x = output.position.x * 2 - 1;
	output.position.y = output.position.y * -2 + 1;

	return output;
}