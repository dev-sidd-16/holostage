/*
* Vulkan Example - Using different pipelines in one single renderpass
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vulkan.h>
#include "vulkanexamplebase.h"
#include "VulkanModel.hpp"
#include "VulkanBuffer.hpp"
#include "TraceTime.hpp"
#include "DataStream.hpp"
#include "../imagetargets/ShareData.h"

#define VERTEX_BUFFER_BIND_ID 0
#define ENABLE_VALIDATION false

class VulkanExample: public VulkanExampleBase 
{
public:
	// Vertex layout for the models
	vks::VertexLayout vertexLayout = vks::VertexLayout({
		vks::VERTEX_COMPONENT_POSITION,
		vks::VERTEX_COMPONENT_NORMAL,
		vks::VERTEX_COMPONENT_UV,
		vks::VERTEX_COMPONENT_COLOR,
	});

	struct {
		vks::Model cube;
	} models;

	vks::Buffer uniformBuffer;

	// Same uniform buffer layout as shader
	struct UBOVS {
		glm::mat4 projection;
		glm::mat4 modelView;
		glm::vec4 lightPos = glm::vec4(0.0f, 2.0f, 1.0f, 0.0f);
	} uboVS;

	VkPipelineLayout pipelineLayout;
	VkDescriptorSet descriptorSet;
	VkDescriptorSetLayout descriptorSetLayout;

	struct {
		VkPipeline phong;
	} pipelines;

	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		zoom = -4.5f;
		rotation = glm::vec3(10.0f, 5.0f, 0.0f);
		enableTextOverlay = true;
		title = "Holostage";
	}

	~VulkanExample()
	{
		// Clean up used Vulkan resources 
		// Note : Inherited destructor cleans up resources stored in base class
		vkDestroyPipeline(device, pipelines.phong, nullptr);
		
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

		models.cube.destroy();
		uniformBuffer.destroy();
	}

	// Enable physical device features required for this example				
	virtual void getEnabledFeatures()
	{
		// Fill mode non solid is required for wireframe display
		if (deviceFeatures.fillModeNonSolid) {
			enabledFeatures.fillModeNonSolid = VK_TRUE;
			// Wide lines must be present for line width > 1.0f
			if (deviceFeatures.wideLines) {
				enabledFeatures.wideLines = VK_TRUE;
			}
		};
	}

	void buildCommandBuffers()
	{		 
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		VkClearValue clearValues[2];
		clearValues[0].color = defaultClearColor;
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.renderArea.offset.x = 0;
		renderPassBeginInfo.renderArea.offset.y = 0;
		renderPassBeginInfo.renderArea.extent.width = width;
		renderPassBeginInfo.renderArea.extent.height = height;
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.pClearValues = clearValues;

		for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
		{
			// Set target frame buffer
			renderPassBeginInfo.framebuffer = frameBuffers[i];

			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

			VkRect2D scissor = vks::initializers::rect2D(width, height,	0, 0);
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);

			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(drawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &models.cube.vertices.buffer, offsets);
			vkCmdBindIndexBuffer(drawCmdBuffers[i], models.cube.indices.buffer, 0, VK_INDEX_TYPE_UINT32);

			// Left : Solid colored 
			viewport.width = (float)width;
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.phong);
			
			vkCmdDrawIndexed(drawCmdBuffers[i], models.cube.indexCount, 1, 0, 0, 0);

			vkCmdEndRenderPass(drawCmdBuffers[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}

	void loadAssets()
	{
        models.cube.loadFromFile(getAssetPath() + "models/cube.dae", vertexLayout, 40.0f, vulkanDevice, queue);
	}

	void setupDescriptorPool()
	{
		std::vector<VkDescriptorPoolSize> poolSizes =
		{
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1)
		};

		VkDescriptorPoolCreateInfo descriptorPoolInfo =
			vks::initializers::descriptorPoolCreateInfo(
				poolSizes.size(),
				poolSizes.data(),
				2);

		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));
	}

	void setupDescriptorSetLayout()
	{
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings =
		{
			// Binding 0 : Vertex shader uniform buffer
			vks::initializers::descriptorSetLayoutBinding(
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				VK_SHADER_STAGE_VERTEX_BIT,
				0)
		};

		VkDescriptorSetLayoutCreateInfo descriptorLayout =
			vks::initializers::descriptorSetLayoutCreateInfo(
				setLayoutBindings.data(),
				setLayoutBindings.size());

		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout));

		VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
			vks::initializers::pipelineLayoutCreateInfo(
				&descriptorSetLayout,
				1);

		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayout));
	}

	void setupDescriptorSet()
	{
		VkDescriptorSetAllocateInfo allocInfo =
			vks::initializers::descriptorSetAllocateInfo(
				descriptorPool,
				&descriptorSetLayout,
				1);

		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));

		std::vector<VkWriteDescriptorSet> writeDescriptorSets =
		{
			// Binding 0 : Vertex shader uniform buffer
			vks::initializers::writeDescriptorSet(
				descriptorSet,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				0,
				&uniformBuffer.descriptor)
		};

		vkUpdateDescriptorSets(device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);
	}

	void preparePipelines()
	{
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
			vks::initializers::pipelineInputAssemblyStateCreateInfo(
				VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
				0,
				VK_FALSE);

		VkPipelineRasterizationStateCreateInfo rasterizationState =
			vks::initializers::pipelineRasterizationStateCreateInfo(
				VK_POLYGON_MODE_FILL,
				VK_CULL_MODE_BACK_BIT,
				VK_FRONT_FACE_COUNTER_CLOCKWISE,
				0);

		VkPipelineColorBlendAttachmentState blendAttachmentState =
			vks::initializers::pipelineColorBlendAttachmentState(
				0xf,
				VK_FALSE);

		VkPipelineColorBlendStateCreateInfo colorBlendState =
			vks::initializers::pipelineColorBlendStateCreateInfo(
				1,
				&blendAttachmentState);

		VkPipelineDepthStencilStateCreateInfo depthStencilState =
			vks::initializers::pipelineDepthStencilStateCreateInfo(
				VK_TRUE,
				VK_TRUE,
				VK_COMPARE_OP_LESS_OR_EQUAL);

		VkPipelineViewportStateCreateInfo viewportState =
			vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);

		VkPipelineMultisampleStateCreateInfo multisampleState =
			vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);

		std::vector<VkDynamicState> dynamicStateEnables = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR,
			VK_DYNAMIC_STATE_LINE_WIDTH,
		};
		VkPipelineDynamicStateCreateInfo dynamicState =
			vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);

		VkGraphicsPipelineCreateInfo pipelineCreateInfo =
			vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass);

		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
		pipelineCreateInfo.pRasterizationState = &rasterizationState;
		pipelineCreateInfo.pColorBlendState = &colorBlendState;
		pipelineCreateInfo.pMultisampleState = &multisampleState;
		pipelineCreateInfo.pViewportState = &viewportState;
		pipelineCreateInfo.pDepthStencilState = &depthStencilState;
		pipelineCreateInfo.pDynamicState = &dynamicState;
		pipelineCreateInfo.stageCount = shaderStages.size();
		pipelineCreateInfo.pStages = shaderStages.data();

		// Shared vertex bindings and attributes used by all pipelines

		// Binding description
		std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
			vks::initializers::vertexInputBindingDescription(VERTEX_BUFFER_BIND_ID, vertexLayout.stride(), VK_VERTEX_INPUT_RATE_VERTEX),
		};

		// Attribute descriptions
		std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
			vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 0, VK_FORMAT_R32G32B32_SFLOAT, 0),					// Location 0: Position			
			vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 1, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3),	// Location 1: Color			
			vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 2, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 6),		// Location 2 : Texture coordinates			
			vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 3, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 8),	// Location 3 : Normal
		};

		VkPipelineVertexInputStateCreateInfo vertexInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
		vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
		vertexInputState.pVertexBindingDescriptions = vertexInputBindings.data();
		vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
		vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();

		pipelineCreateInfo.pVertexInputState = &vertexInputState;

		// Create the graphics pipeline state objects

		// We are using this pipeline as the base for the other pipelines (derivatives)
		// Pipeline derivatives can be used for pipelines that share most of their state
		// Depending on the implementation this may result in better performance for pipeline 
		// switchting and faster creation time
		pipelineCreateInfo.flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;

		// Textured pipeline
		// Phong shading pipeline
		shaderStages[0] = loadShader(getAssetPath() + "shaders/pipelines/phong.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/pipelines/phong.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.phong));

		// All pipelines created after the base pipeline will be derivatives
		pipelineCreateInfo.flags = VK_PIPELINE_CREATE_DERIVATIVE_BIT;
		// Base pipeline will be our first created pipeline
		pipelineCreateInfo.basePipelineHandle = pipelines.phong;
		// It's only allowed to either use a handle or index for the base pipeline
		// As we use the handle, we must set the index to -1 (see section 9.5 of the specification)
		pipelineCreateInfo.basePipelineIndex = -1;
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers()
	{
		// Create the vertex shader uniform buffer block
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffer,
			sizeof(uboVS)));

		// Map persistent
		VK_CHECK_RESULT(uniformBuffer.map());

		updateUniformBuffers();
	}

    // Get the mvp matrix from Vuforia and put them in the Uniform buffer
	void getMvp(){

        if(mvpUsed){
            // The mvp matrix from Vuforia has been used
            return;
        }

        mvpUsed = true;

		float * modelView = modelViewShare;
		float * projection = projectionShare;
        float farPlane=1000.0f;
        float nearPlane=0.1f;
        float c = (farPlane+nearPlane)/(farPlane - nearPlane);
        float d = -nearPlane *(1.0f+c);
        projection[10]=c;
        projection[14]=d;
        uboVS.projection = glm::make_mat4(projection);
        uboVS.modelView = glm::make_mat4(modelView);
        if(uniformBuffer.mapped){
            memcpy(uniformBuffer.mapped, &uboVS, sizeof(uboVS));
        }

	}

    void updateVertexBuffer(){
        bool bufferResized = models.cube.updateVertexBuffer(vulkanDevice, queue);
        // rebuild commandbuffer
        if(bufferResized){
            buildCommandBuffers();
        }
    }

	void updateUniformBuffers()
	{
		uboVS.projection = glm::perspective(glm::radians(60.0f), (float)(width) / (float)height, 0.1f, 256.0f);

		glm::mat4 viewMatrix = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, zoom));

		uboVS.modelView = viewMatrix * glm::translate(glm::mat4(), cameraPos);
		uboVS.modelView = glm::rotate(uboVS.modelView, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		uboVS.modelView = glm::rotate(uboVS.modelView, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		uboVS.modelView = glm::rotate(uboVS.modelView, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

        if(uniformBuffer.mapped){
            memcpy(uniformBuffer.mapped, &uboVS, sizeof(uboVS));
        }
	}

	void draw()
	{
		bool succeed = VulkanExampleBase::prepareFrame();
        if(!succeed){
            VulkanExampleBase::windowResize();
            LOGE("Error: resize the window.");
            return;
        }
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

		VulkanExampleBase::submitFrame();
	}

	void prepare()
	{
		VulkanExampleBase::prepare();
		loadAssets();
		prepareUniformBuffers();
		setupDescriptorSetLayout();
		preparePipelines();
		setupDescriptorPool();
		setupDescriptorSet();
		buildCommandBuffers();
		prepared = true;
	}

	virtual void render()
	{
		if (!prepared)
			return;

        // Get the MVP matrix from Vuforia
        getMvp();

        // Initiation. For measuring the time
        models.cube.model.renderingStartTime = -1;

        updateVertexBuffer();

		draw();

        // For measuring the rendering time.
        if(models.cube.model.renderingStartTime > 0){

            int result = getCurrentTimeMillis() - models.cube.model.renderingStartTime;

            saveFPS(result);
        }

	}

	virtual void viewChanged()
	{
		updateUniformBuffers();
	}

	virtual void getOverlayText(VulkanTextOverlay *textOverlay)
	{
		if (!deviceFeatures.fillModeNonSolid) {
			textOverlay->addText("Non solid fill modes not supported!", width - (float)width / 6.5f, (float)height / 2.0f - 7.5f, VulkanTextOverlay::alignCenter);
		}

		// test
//		saveFPSData();
//
//        rotate();
	}

    void rotate(){
        float deltaX = 0.f;
        float  deltaY = 0;
        float deltaZ = 10.f;

//        camera.rotate(glm::vec3(deltaX, 0.0f, 0.0f));
//        camera.rotate(glm::vec3(0.0f, -deltaY, 0.0f));
//        camera.rotate(glm::vec3(0.0f, 0.0f, deltaZ * 100));

        rotation.x += deltaX;
        rotation.y -= deltaY;
        rotation.z += deltaZ;

        viewChanged();
    }

	// set how many times the objects will be duplicated
	void setMultiple(int multiple){
		models.cube.setObjectsMultiple(multiple);
	}

	void setFilePath(string dir){
		setFileDirectory(dir);
	}

    int previousCount = -1;
	void saveFPSData(){
//		int currentFPS = lastFPS;
//		if(saveFPS(currentFPS)){
//			// The count of data is enough
//			stopRenderLoop();
//		}
        if(previousCount > 0){
            int count = frames.getCount() - previousCount;
            saveFPS(count);
        }
        previousCount = frames.getCount();
	}
};

VulkanExample *vulkanExample;

void timerCall(){
//    vulkanExample->saveFPSData();
}

void android_main(android_app* state)
{

    startServer();

    app_dummy();
    vulkanExample = new VulkanExample();

    vulkanExample->setFilePath(state->activity->externalDataPath);

    // create File
    initFile();
    startTimer();

    state->userData = vulkanExample;
    state->onAppCmd = VulkanExample::handleAppCommand;
    state->onInputEvent = VulkanExample::handleAppInput;
    androidApp = state;
    vulkanExample->renderLoop();
    delete (vulkanExample);

    // close log file
    closeFile();
    stopTimer();

    // close tcp server
    stopServer();

}