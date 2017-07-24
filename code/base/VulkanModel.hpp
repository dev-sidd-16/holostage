/*
* Vulkan Model loader using ASSIMP
*
* Copyright(C) 2016-2017 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license(MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <stdlib.h>
#include <string>
#include <fstream>
#include <vector>

#include "vulkan/vulkan.h"

#include <assimp/Importer.hpp> 
#include <assimp/scene.h>     
#include <assimp/postprocess.h>
#include <assimp/cimport.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "VulkanDevice.hpp"
#include "VulkanBuffer.hpp"

#if defined(__ANDROID__)
#include <android/asset_manager.h>
#endif

#include "../pipelines/models.h"

namespace vks
{

	struct Model {
		VkDevice device = nullptr;
		vks::Buffer vertices;
		vks::Buffer indices;
		uint32_t indexCount = 0;
        uint32_t currentVBufferSize = 0;

        // Used to load data from the file and server
		ModelX model;

		static const int defaultFlags = aiProcess_FlipWindingOrder | aiProcess_Triangulate | aiProcess_PreTransformVertices | aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals;

		/** @brief Release all Vulkan resources of this model */
		void destroy()
		{		
			assert(device);
			vkDestroyBuffer(device, vertices.buffer, nullptr);
			vkFreeMemory(device, vertices.memory, nullptr);
			if (indices.buffer != VK_NULL_HANDLE)
			{
				vkDestroyBuffer(device, indices.buffer, nullptr);
				vkFreeMemory(device, indices.memory, nullptr);
			}
		}

		/**
		* Loads a 3D model from a file into Vulkan buffers
		*
		* @param device Pointer to the Vulkan device used to generated the vertex and index buffers on
		* @param filename File to load (must be a model format supported by ASSIMP)
		* @param layout Vertex layout components (position, normals, tangents, etc.)
		* @param createInfo MeshCreateInfo structure for load time settings like scale, center, etc.
		* @param copyQueue Queue used for the memory staging copy commands (must support transfer)
		* @param (Optional) flags ASSIMP model loading flags
		*/
        bool loadFromFile(const std::string& filename, vks::VertexLayout layout, float scale, vks::VulkanDevice *device, VkQueue copyQueue, const int flags = defaultFlags)
        {
            this->device = device->logicalDevice;

            // load the model data from file
            model.loadFromFile(filename, layout, scale, flags);
            // update the rendering data according to the configure data from server
            model.loadFromServer();

            std::vector<float> * vertexBuffer;
            std::vector<uint32_t> * indexBuffer;

            if(model.renderingVerticesCount > 0){
                vertexBuffer = &model.renderingVertices;
            }else{
                vertexBuffer = &model.vertexBuffer;
            }

            if(model.renderingIndicesCount > 0){
                indexBuffer = &model.renderingIndices;
                indexCount = model.renderingIndicesCount;
            }else{
                indexBuffer = &model.indexBuffer;
                indexCount = model.indexCount;
            }

            uint32_t vBufferSize = static_cast<uint32_t>(vertexBuffer->size()) * sizeof(float);
            uint32_t iBufferSize = static_cast<uint32_t>(indexBuffer->size()) * sizeof(uint32_t);
            currentVBufferSize = vBufferSize;

            // Use staging buffer to move vertex and index buffer to device local memory
            // Create staging buffers
            vks::Buffer vertexStaging, indexStaging;

            // Vertex buffer
            VK_CHECK_RESULT(device->createBuffer(
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                    &vertexStaging,
                    vBufferSize,
                    vertexBuffer->data()));

            // Index buffer
            VK_CHECK_RESULT(device->createBuffer(
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                    &indexStaging,
                    iBufferSize,
                    indexBuffer->data()));

            // Create device local target buffers
            // Vertex buffer
            VK_CHECK_RESULT(device->createBuffer(
                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    &vertices,
                    vBufferSize));

            // Index buffer
            VK_CHECK_RESULT(device->createBuffer(
                    VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    &indices,
                    iBufferSize));

            // Copy from staging buffers
            VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                                                  true);

            VkBufferCopy copyRegion{};

            copyRegion.size = vertices.size;
            vkCmdCopyBuffer(copyCmd, vertexStaging.buffer, vertices.buffer, 1, &copyRegion);

            copyRegion.size = indices.size;
            vkCmdCopyBuffer(copyCmd, indexStaging.buffer, indices.buffer, 1, &copyRegion);

            device->flushCommandBuffer(copyCmd, copyQueue);

            // Destroy staging resources
            vkDestroyBuffer(device->logicalDevice, vertexStaging.buffer, nullptr);
            vkFreeMemory(device->logicalDevice, vertexStaging.memory, nullptr);
            vkDestroyBuffer(device->logicalDevice, indexStaging.buffer, nullptr);
            vkFreeMemory(device->logicalDevice, indexStaging.memory, nullptr);

            return true;

        }

        // return true if the vertex buffer is resized, otherwise return false
        bool updateBuffers(vks::VulkanDevice *device, VkQueue copyQueue){

            std::vector<float> * vertexBuffer;
            std::vector<uint32_t> * indexBuffer;

            if(model.renderingVerticesCount > 0){
                vertexBuffer = &model.renderingVertices;
            }else{
                vertexBuffer = &model.vertexBuffer;
            }

            if(model.renderingIndicesCount > 0){
                indexBuffer = &model.renderingIndices;
                indexCount = model.renderingIndicesCount;
            }else{
                indexBuffer = &model.indexBuffer;
                indexCount = model.indexCount;
            }

            uint32_t vBufferSize = static_cast<uint32_t>(vertexBuffer->size()) * sizeof(float);
            uint32_t iBufferSize = static_cast<uint32_t>(indexBuffer->size()) * sizeof(uint32_t);

            // Use staging buffer to move vertex and index buffer to device local memory
            // Create staging buffers
            vks::Buffer vertexStaging, indexStaging;

            // Vertex buffer
            VK_CHECK_RESULT(device->createBuffer(
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                    &vertexStaging,
                    vBufferSize,
                    vertexBuffer->data()));

            // Index buffer
            VK_CHECK_RESULT(device->createBuffer(
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                    &indexStaging,
                    iBufferSize,
                    indexBuffer->data()));

            bool bufferResized = false;

            if(currentVBufferSize != vBufferSize){
                bufferResized = true;
                // destroy the old buffer
                vertices.destroy();
                indices.destroy();
                // Create device local target buffers
                // Vertex buffer
                VK_CHECK_RESULT(device->createBuffer(
                        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                        &vertices,
                        vBufferSize));

                // Index buffer
                VK_CHECK_RESULT(device->createBuffer(
                        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                        &indices,
                        iBufferSize));
                currentVBufferSize = vBufferSize;
            }

            // Copy from staging buffers
            VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                                                  true);

            VkBufferCopy copyRegion{};

            copyRegion.size = vertices.size;
            vkCmdCopyBuffer(copyCmd, vertexStaging.buffer, vertices.buffer, 1, &copyRegion);

            copyRegion.size = indices.size;
            vkCmdCopyBuffer(copyCmd, indexStaging.buffer, indices.buffer, 1, &copyRegion);

            device->flushCommandBuffer(copyCmd, copyQueue);

            // Destroy staging resources
            vkDestroyBuffer(device->logicalDevice, vertexStaging.buffer, nullptr);
            vkFreeMemory(device->logicalDevice, vertexStaging.memory, nullptr);
            vkDestroyBuffer(device->logicalDevice, indexStaging.buffer, nullptr);
            vkFreeMemory(device->logicalDevice, indexStaging.memory, nullptr);

            return bufferResized;
        }

        // return true if vertex buffer is resized, otherwise return false
		bool updateVertexBuffer(vks::VulkanDevice *device, VkQueue copyQueue){
            // update the rendering data according to the configure data from server
            model.loadFromServer();
            if(model.isDataChanged){
                // data has changed, update the vertex buffer
                return updateBuffers(device, copyQueue);
            }
            return false;
		}

		void setObjectsMultiple(int multiple){
//			MULTIPLE = multiple;
		}
	};
};