#include "Game.h"
#include "Vertex.h"
#include "Input.h"
#include "VulkanHelper.h"
#include "Helpers.h"

// Needed for a helper function to read compiled shader files from the hard drive
#pragma comment(lib, "d3dcompiler.lib")
#include <d3dcompiler.h>

// For the DirectX Math library
using namespace DirectX;

// --------------------------------------------------------
// Constructor
//
// DXCore (base class) constructor will set up underlying fields.
// DirectX itself, and our window, are not ready yet!
//
// hInstance - the application's OS-level handle (unique ID)
// --------------------------------------------------------
Game::Game(HINSTANCE hInstance)
	: VKCore(
		hInstance,		// The application's handle
		L"Vulkan Game",	// Text for the window's title bar
		1280,			// Width of the window's client area
		720,			// Height of the window's client area
		false,			// Sync the framerate to the monitor refresh? (lock framerate)
		true),			// Show extra stats (fps) in title bar?
	vkPipeline(0),
	vkPipelineLayout(0),
	vertexBuffer(0),
	indexBuffer(0),
	vertexBufferMemory(0),
	indexBufferMemory(0)
{

#if defined(DEBUG) || defined(_DEBUG)
	// Do we want a console window?  Probably only in debug mode
	CreateConsoleWindow(500, 120, 32, 120);
	printf("Console window created successfully.  Feel free to printf() here.\n");
#endif

}

// --------------------------------------------------------
// Destructor - Clean up anything our game has created:
//  - Release all DirectX objects created here
//  - Delete any objects to prevent memory leaks
// --------------------------------------------------------
Game::~Game()
{
	// Note: Since we're using smart pointers (ComPtr),
	// we don't need to explicitly clean up those DirectX objects
	// - If we weren't using smart pointers, we'd need
	//   to call Release() on each DirectX object created in Game

	// However, we DO need to wait here until the GPU
	// is actually done with its work
	//VulkanHelper::GetInstance().WaitForGPU();

	// Resource cleanup
	vkDestroyBuffer(vkDevice, vertexBuffer, 0);
	vkDestroyBuffer(vkDevice, indexBuffer, 0);
	vkFreeMemory(vkDevice, vertexBufferMemory, 0);
	vkFreeMemory(vkDevice, indexBufferMemory, 0);

	// Pipeline cleanup
	vkDestroyPipeline(vkDevice, vkPipeline, 0);
	vkDestroyPipelineLayout(vkDevice, vkPipelineLayout, 0);

}

// --------------------------------------------------------
// Called once per program, after DirectX and the window
// are initialized but before the game loop.
// --------------------------------------------------------
void Game::Init()
{
	// Helper methods for loading shaders, creating some basic
	// geometry to draw and some simple camera matrices.
	//  - You'll be expanding and/or replacing these later
	CreateGraphicsPipeline();
	CreateBasicGeometry();
}


// --------------------------------------------------------
// Loads the two basic shaders, then creates the root signature 
// and pipeline state object for our very basic demo.
// --------------------------------------------------------
void Game::CreateGraphicsPipeline()
{
	// Load our shaders
	std::vector<char> vertBlob = ReadFileToCharBlob(FixPath(L"VertexShader.vert.spv"));
	std::vector<char> fragBlob = ReadFileToCharBlob(FixPath(L"FragmentShader.frag.spv"));

	// Describe vulkan shader modules for each shader
	VkShaderModuleCreateInfo vsModDesc = {};
	vsModDesc.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vsModDesc.codeSize = vertBlob.size();
	vsModDesc.pCode = reinterpret_cast<const uint32_t*>(vertBlob.data());

	VkShaderModuleCreateInfo fsModDesc = {};
	fsModDesc.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fsModDesc.codeSize = fragBlob.size();
	fsModDesc.pCode = reinterpret_cast<const uint32_t*>(fragBlob.data());

	// Create each module so we can later add it to the pipeline
	VkShaderModule vertModule;
	if (vkCreateShaderModule(vkDevice, &vsModDesc, 0, &vertModule) != VK_SUCCESS)
		printf("Error creating vertex shader module!\n");

	VkShaderModule fragModule;
	if (vkCreateShaderModule(vkDevice, &fsModDesc, 0, &fragModule) != VK_SUCCESS)
		printf("Error creating fragment shader module!\n");

	// Create shader pipeline stages
	VkPipelineShaderStageCreateInfo shaderStageDescs[2] = {};

	shaderStageDescs[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStageDescs[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderStageDescs[0].module = vertModule;
	shaderStageDescs[0].pName = "main";

	shaderStageDescs[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStageDescs[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderStageDescs[1].module = fragModule;
	shaderStageDescs[1].pName = "main";

	// Vertex binding details (input rate)
	VkVertexInputBindingDescription vertexBindingDesc = {};
	vertexBindingDesc.binding = 0;
	vertexBindingDesc.stride = sizeof(Vertex);
	vertexBindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	// Vertex element details
	VkVertexInputAttributeDescription vertexElements[2] = {};
	vertexElements[0].binding = 0;
	vertexElements[0].location = 0;
	vertexElements[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	vertexElements[0].offset = offsetof(Vertex, Position);
	vertexElements[1].binding = 0;
	vertexElements[1].location = 1;
	vertexElements[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	vertexElements[1].offset = offsetof(Vertex, Color);

	// Actual vertex input for the pipeline
	VkPipelineVertexInputStateCreateInfo vertexInputDesc = {};
	vertexInputDesc.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputDesc.vertexBindingDescriptionCount = 1;
	vertexInputDesc.pVertexBindingDescriptions = &vertexBindingDesc;
	vertexInputDesc.vertexAttributeDescriptionCount = ARRAYSIZE(vertexElements);
	vertexInputDesc.pVertexAttributeDescriptions = vertexElements;

	// Input assembler
	VkPipelineInputAssemblyStateCreateInfo iaDesc = {};
	iaDesc.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	iaDesc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	// We'll be keeping the viewport and scissor rect dynamic
	VkDynamicState dynamicStates[2] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};
	
	VkPipelineDynamicStateCreateInfo dynamicDesc = {};
	dynamicDesc.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicDesc.dynamicStateCount = ARRAYSIZE(dynamicStates);
	dynamicDesc.pDynamicStates = dynamicStates;

	VkPipelineViewportStateCreateInfo vpDesc = {};
	vpDesc.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	vpDesc.viewportCount = 1;
	vpDesc.scissorCount = 1;
	
	// Rasterizer
	VkPipelineRasterizationStateCreateInfo rsDesc = {};
	rsDesc.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rsDesc.cullMode = VK_CULL_MODE_BACK_BIT;
	rsDesc.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rsDesc.lineWidth = 1.0f;
	rsDesc.polygonMode = VK_POLYGON_MODE_FILL;

	// Multisampling options
	VkPipelineMultisampleStateCreateInfo msDesc = {};
	msDesc.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	msDesc.minSampleShading = 0;
	msDesc.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	msDesc.sampleShadingEnable = VK_FALSE;

	// Depth stencil state
	VkPipelineDepthStencilStateCreateInfo dsDesc = {};
	dsDesc.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	dsDesc.depthTestEnable = VK_TRUE;
	dsDesc.depthWriteEnable = VK_TRUE;
	dsDesc.depthCompareOp = VK_COMPARE_OP_LESS;

	// Blending
	VkPipelineColorBlendAttachmentState blendOff = {};
	blendOff.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;
	blendOff.blendEnable = false;

	VkPipelineColorBlendStateCreateInfo blendDesc = {};
	blendDesc.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blendDesc.logicOpEnable = VK_FALSE;
	blendDesc.attachmentCount = 1;
	blendDesc.pAttachments = &blendOff;
	blendDesc.blendConstants[0] = 1.0f;
	blendDesc.blendConstants[1] = 1.0f;
	blendDesc.blendConstants[2] = 1.0f;
	blendDesc.blendConstants[3] = 1.0f;

	// Pipeline layout (root signature) - empty for now
	VkPipelineLayoutCreateInfo pipelineLayoutDesc = {};
	pipelineLayoutDesc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutDesc.setLayoutCount = 0;
	if (vkCreatePipelineLayout(vkDevice, &pipelineLayoutDesc, 0, &vkPipelineLayout) != VK_SUCCESS)
	{
		printf("Error creating pipeline layout!\n");
	}

	// Dynamic rendering info
	VkPipelineRenderingCreateInfoKHR dynamicRenderingDesc = {};
	dynamicRenderingDesc.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
	dynamicRenderingDesc.colorAttachmentCount = 1;
	dynamicRenderingDesc.pColorAttachmentFormats = &backBufferColorFormat;

	// Actually make the pipeline state object!
	VkGraphicsPipelineCreateInfo pipeDesc = {};
	pipeDesc.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeDesc.layout = vkPipelineLayout;
	pipeDesc.pNext = &dynamicRenderingDesc;

	// Shaders
	{
		pipeDesc.stageCount = 2; // Shader count
		pipeDesc.pStages = shaderStageDescs;
	}

	// Other stages
	{
		pipeDesc.pColorBlendState = &blendDesc;
		pipeDesc.pDepthStencilState = &dsDesc;
		pipeDesc.pInputAssemblyState = &iaDesc;
		pipeDesc.pMultisampleState = &msDesc;
		pipeDesc.pRasterizationState = &rsDesc;
		pipeDesc.pVertexInputState = &vertexInputDesc;
		pipeDesc.pViewportState = &vpDesc;
		pipeDesc.pDynamicState = &dynamicDesc;
	}

	if (vkCreateGraphicsPipelines(vkDevice, 0, 1, &pipeDesc, 0, &vkPipeline) != VK_SUCCESS)
	{
		printf("Error creating pipeline!\n");
	}

	// All done, clean up shader modules
	vkDestroyShaderModule(vkDevice, vertModule, nullptr);
	vkDestroyShaderModule(vkDevice, fragModule, nullptr);
}



// --------------------------------------------------------
// Creates the geometry we're going to draw - a single triangle for now
// --------------------------------------------------------
void Game::CreateBasicGeometry()
{
	// Create some temporary variables to represent colors
	// - Not necessary, just makes things more readable
	XMFLOAT4 red = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
	XMFLOAT4 green = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);
	XMFLOAT4 blue = XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);

	// Set up the vertices of the triangle we would like to draw
	Vertex vertices[] =
	{
		{ XMFLOAT3(+0.0f, +0.5f, +0.0f), red },
		{ XMFLOAT3(+0.5f, -0.5f, +0.0f), blue },
		{ XMFLOAT3(-0.5f, -0.5f, +0.0f), green },
	};

	// Set up the indices, which tell us which vertices to use and in which order
	unsigned int indices[] = { 0, 1, 2 };

	// Create the two buffers
	VulkanHelper& VulkanHelper = VulkanHelper::GetInstance();
	VulkanHelper.CreateStaticBuffer(sizeof(Vertex), ARRAYSIZE(vertices), vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertexBuffer, vertexBufferMemory);
	VulkanHelper.CreateStaticBuffer(sizeof(unsigned int), ARRAYSIZE(indices), indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indexBuffer, indexBufferMemory);
}


// --------------------------------------------------------
// Handle resizing DirectX "stuff" to match the new window size.
// For instance, updating our projection matrix's aspect ratio.
// --------------------------------------------------------
void Game::OnResize()
{
	// Handle base-level DX resize stuff
	VKCore::OnResize();
}

// --------------------------------------------------------
// Update your game here - user input, move objects, AI, etc.
// --------------------------------------------------------
void Game::Update(float deltaTime, float totalTime)
{
	// Example input checking: Quit if the escape key is pressed
	if (Input::GetInstance().KeyDown(VK_ESCAPE))
		Quit();

}

// --------------------------------------------------------
// Clear the screen, redraw everything, present to the user
// --------------------------------------------------------
void Game::Draw(float deltaTime, float totalTime)
{
	// Grab the current back buffer for this frame
	VkImageView currentBackBufferView = vkBackBufferViews[currentSwapBuffer];

	// Reset command buffer for the frame
	vkResetCommandBuffer(vkCommandBuffer, 0);


	// Start commands
	VkCommandBufferBeginInfo beginDesc = {};
	beginDesc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	vkBeginCommandBuffer(vkCommandBuffer, &beginDesc);

	// Bind the pipeline state and set viewport & scissor
	vkCmdBindPipeline(vkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipeline);
	vkCmdSetViewport(vkCommandBuffer, 0, 1, &viewport);
	vkCmdSetScissor(vkCommandBuffer, 0, 1, &scissor);

	// Bind geometry
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(vkCommandBuffer, 0, 1, &vertexBuffer, offsets);
	vkCmdBindIndexBuffer(vkCommandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

	// Dynamic rendering attachment setup (instead of render pass)
	VkRenderingAttachmentInfo attachmentDesc = {};
	attachmentDesc.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
	attachmentDesc.imageView = currentBackBufferView;
	attachmentDesc.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
	attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDesc.clearValue = { 0.4f, 0.6f, 0.75f, 1.0f }; // Clear color!

	// Overall render info for dynamic rendering
	VkRenderingInfo renderDesc = {};
	renderDesc.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
	renderDesc.renderArea = { 0, 0, windowWidth, windowHeight };
	renderDesc.layerCount = 1;
	renderDesc.colorAttachmentCount = 1;
	renderDesc.pColorAttachments = &attachmentDesc;

	// Dynamic render begin
	vkCmdBeginRendering(vkCommandBuffer, &renderDesc);

	// Actual draw
	vkCmdDrawIndexed(vkCommandBuffer, 3, 1, 0, 0, 0);

	// End commands
	vkCmdEndRendering(vkCommandBuffer);
	vkEndCommandBuffer(vkCommandBuffer);

	// Submit command buffer to the queue
	VkSubmitInfo submitDesc = {};
	submitDesc.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitDesc.commandBufferCount = 1;
	submitDesc.pCommandBuffers = &vkCommandBuffer;

	vkQueueSubmit(vkGraphicsQueue, 1, &submitDesc, 0);

	// Present
	VkPresentInfoKHR presentDesc = {};
	presentDesc.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	// TODO: Sync
	presentDesc.swapchainCount = 1;
	presentDesc.pSwapchains = &vkSwapchain;
	presentDesc.pImageIndices = &currentSwapBuffer;
	
	vkQueuePresentKHR(vkGraphicsQueue, &presentDesc);

	// Figure out which buffer is next
	currentSwapBuffer++;
	if (currentSwapBuffer >= numBackBuffers)
		currentSwapBuffer = 0;
}