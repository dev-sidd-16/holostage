//#pragma once
#ifndef PIPELINES_MODELS_H
#define PIPELINES_MODELS_H

#include <stdlib.h>
#include <string>
#include <fstream>
#include <vector>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/cimport.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <android/asset_manager.h>
#include <VulkanAndroid.h>
#include "Datagram.hpp"
#include "DataStream.hpp"
#include "TraceTime.hpp"


namespace vks
{
    /** @brief Vertex layout components */
    typedef enum Component {
        VERTEX_COMPONENT_POSITION = 0x0,
        VERTEX_COMPONENT_NORMAL = 0x1,
        VERTEX_COMPONENT_COLOR = 0x2,
        VERTEX_COMPONENT_UV = 0x3,
        VERTEX_COMPONENT_TANGENT = 0x4,
        VERTEX_COMPONENT_BITANGENT = 0x5,
        VERTEX_COMPONENT_DUMMY_FLOAT = 0x6,
        VERTEX_COMPONENT_DUMMY_VEC4 = 0x7
    } Component;

    /** @brief Stores vertex layout components for model loading and Vulkan vertex input and atribute bindings  */
    struct VertexLayout {
    public:
        /** @brief Components used to generate vertices from */
        std::vector<Component> components;

        VertexLayout(std::vector<Component> components)
        {
            this->components = std::move(components);
        }

        uint32_t stride()
        {
            uint32_t res = 0;
            for (auto& component : components)
            {
                switch (component)
                {
                    case VERTEX_COMPONENT_UV:
                        res += 2 * sizeof(float);
                        break;
                    case VERTEX_COMPONENT_DUMMY_FLOAT:
                        res += sizeof(float);
                        break;
                    case VERTEX_COMPONENT_DUMMY_VEC4:
                        res += 4 * sizeof(float);
                        break;
                    default:
                        // All components except the ones listed above are made up of 3 floats
                        res += 3 * sizeof(float);
                }
            }
            return res;
        }
    };

    /** @brief Used to parametrize model loading */
    struct ModelCreateInfo {
        glm::vec3 center;
        glm::vec3 scale;
        glm::vec2 uvscale;

        ModelCreateInfo() {};

        ModelCreateInfo(glm::vec3 scale, glm::vec2 uvscale, glm::vec3 center)
        {
            this->center = center;
            this->scale = scale;
            this->uvscale = uvscale;
        }

        ModelCreateInfo(float scale, float uvscale, float center)
        {
            this->center = glm::vec3(center);
            this->scale = glm::vec3(scale);
            this->uvscale = glm::vec2(uvscale);
        }

    };

    struct ModelX {
        uint32_t indexCount = 0;
        uint32_t vertexCount = 0;

        /** @brief Stores vertex and index base and counts for each part of a model */
        struct ModelPart {
            uint32_t vertexBase;
            uint32_t vertexCount;
            uint32_t indexBase;
            uint32_t indexCount;
        };
        std::vector<ModelPart> parts;

        // vertices and indices which are loaded from the file
        std::vector<float> vertexBuffer;
        std::vector<uint32_t> indexBuffer;

        // vertices and indices for rendering
        // The data may vary according to the data from the server
        std::vector<float> renderingVertices;
        std::vector<uint32_t> renderingIndices;
        uint32_t renderingVerticesCount = 0;
        uint32_t renderingIndicesCount = 0;
        bool isDataChanged = true;
        int prePosSize = 0;

        static const int defaultFlags = aiProcess_FlipWindingOrder | aiProcess_Triangulate | aiProcess_PreTransformVertices | aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals;

        struct Dimension
        {
            glm::vec3 min = glm::vec3(FLT_MAX);
            glm::vec3 max = glm::vec3(-FLT_MAX);
            glm::vec3 size;
        } dim;

        /** @brief Release all Vulkan resources of this model */
        void destroy()
        {

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
        bool loadFromFile(const std::string& filename, VertexLayout layout, ModelCreateInfo *createInfo, const int flags = defaultFlags)
        {
            Assimp::Importer Importer;
            const aiScene* pScene;

            // Load file
            // Meshes are stored inside the apk on Android (compressed)
            // So they need to be loaded via the asset manager

            AAsset* asset = AAssetManager_open(androidApp->activity->assetManager, filename.c_str(), AASSET_MODE_STREAMING);
            if (!asset) {
                LOGE("Could not load mesh from \"%s\"!", filename.c_str());
                return false;
            }
            assert(asset);
            size_t size = AAsset_getLength(asset);

            assert(size > 0);

            void *meshData = malloc(size);
            AAsset_read(asset, meshData, size);
            AAsset_close(asset);

            pScene = Importer.ReadFileFromMemory(meshData, size, flags);

            free(meshData);

            if (pScene)
            {
				parts.clear();
				parts.resize(pScene->mNumMeshes);

                glm::vec3 scale(1.0f);
                glm::vec2 uvscale(1.0f);
                glm::vec3 center(0.0f);
                if (createInfo)
                {
                    scale = createInfo->scale;
                    uvscale = createInfo->uvscale;
                    center = createInfo->center;
                }

                vertexCount = 0;
                indexCount = 0;
                vertexBuffer.clear();
                indexBuffer.clear();

                // Load meshes
				for (unsigned int i = 0; i < pScene->mNumMeshes; i++)
                {
					const aiMesh* paiMesh = pScene->mMeshes[i];
                    parts[i] = {};
                    parts[i].vertexBase = vertexCount;
                    parts[i].indexBase = indexCount;

                    vertexCount += paiMesh ->mNumVertices;

                    aiColor3D pColor(0.f, 0.f, 0.f);
                    pScene->mMaterials[paiMesh->mMaterialIndex]->Get(AI_MATKEY_COLOR_DIFFUSE, pColor);

                    const aiVector3D Zero3D(0.0f, 0.0f, 0.0f);

                    for (unsigned int j = 0; j < paiMesh->mNumVertices; j++)
                    {
                        const aiVector3D* pPos = &(paiMesh->mVertices[j]);
                        const aiVector3D* pNormal = &(paiMesh->mNormals[j]);
                        const aiVector3D* pTexCoord = (paiMesh->HasTextureCoords(0)) ? &(paiMesh->mTextureCoords[0][j]) : &Zero3D;
                        const aiVector3D* pTangent = (paiMesh->HasTangentsAndBitangents()) ? &(paiMesh->mTangents[j]) : &Zero3D;
                        const aiVector3D* pBiTangent = (paiMesh->HasTangentsAndBitangents()) ? &(paiMesh->mBitangents[j]) : &Zero3D;

                        for (auto& component : layout.components)
                        {
                            switch (component) {
                                case VERTEX_COMPONENT_POSITION:
								    vertexBuffer.push_back(pPos->x * scale.x + center.x);
								    vertexBuffer.push_back(-pPos->y * scale.y + center.y);
								    vertexBuffer.push_back(pPos->z * scale.z + center.z);
                                    break;
                                case VERTEX_COMPONENT_NORMAL:
                                    vertexBuffer.push_back(pNormal->x);
                                    vertexBuffer.push_back(-pNormal->y);
                                    vertexBuffer.push_back(pNormal->z);
                                    break;
                                case VERTEX_COMPONENT_UV:
                                    vertexBuffer.push_back(pTexCoord->x * uvscale.s);
                                    vertexBuffer.push_back(pTexCoord->y * uvscale.t);
                                    break;
                                case VERTEX_COMPONENT_COLOR:
                                    vertexBuffer.push_back(pColor.r);
                                    vertexBuffer.push_back(pColor.g);
                                    vertexBuffer.push_back(pColor.b);
                                    break;
                                case VERTEX_COMPONENT_TANGENT:
                                    vertexBuffer.push_back(pTangent->x);
                                    vertexBuffer.push_back(pTangent->y);
                                    vertexBuffer.push_back(pTangent->z);
                                    break;
                                case VERTEX_COMPONENT_BITANGENT:
                                    vertexBuffer.push_back(pBiTangent->x);
                                    vertexBuffer.push_back(pBiTangent->y);
                                    vertexBuffer.push_back(pBiTangent->z);
                                    break;
                                    // Dummy components for padding
                                case VERTEX_COMPONENT_DUMMY_FLOAT:
                                    vertexBuffer.push_back(0.0f);
                                    break;
                                case VERTEX_COMPONENT_DUMMY_VEC4:
                                    vertexBuffer.push_back(0.0f);
                                    vertexBuffer.push_back(0.0f);
                                    vertexBuffer.push_back(0.0f);
                                    vertexBuffer.push_back(0.0f);
                                    break;
                            };
                        }

                        dim.max.x = fmax(pPos->x, dim.max.x);
                        dim.max.y = fmax(pPos->y, dim.max.y);
                        dim.max.z = fmax(pPos->z, dim.max.z);

                        dim.min.x = fmin(pPos->x, dim.min.x);
                        dim.min.y = fmin(pPos->y, dim.min.y);
                        dim.min.z = fmin(pPos->z, dim.min.z);
                    }

                    dim.size = dim.max - dim.min;

                    parts[i].vertexCount = paiMesh->mNumVertices;

                    uint32_t indexBase = static_cast<uint32_t>(indexBuffer.size());
                    for (unsigned int j = 0; j < paiMesh->mNumFaces; j++)
                    {
                        const aiFace& Face = paiMesh->mFaces[j];
                        if (Face.mNumIndices != 3)
                            continue;
                        indexBuffer.push_back(indexBase + Face.mIndices[0]);
                        indexBuffer.push_back(indexBase + Face.mIndices[1]);
                        indexBuffer.push_back(indexBase + Face.mIndices[2]);
                        parts[i].indexCount += 3;
                        indexCount += 3;
                    }
                }

                return true;
            }
            else
            {
                LOGE("Error parsing '%s': '%s'", filename.c_str(), Importer.GetErrorString());
                return false;
            }
        };

        /**
        * Loads a 3D model from a file into Vulkan buffers
        *
        * @param device Pointer to the Vulkan device used to generated the vertex and index buffers on
        * @param filename File to load (must be a model format supported by ASSIMP)
        * @param layout Vertex layout components (position, normals, tangents, etc.)
        * @param scale Load time scene scale
        * @param copyQueue Queue used for the memory staging copy commands (must support transfer)
        * @param (Optional) flags ASSIMP model loading flags
        */
        bool loadFromFile(const std::string& filename, VertexLayout layout, float scale, const int flags = defaultFlags)
        {
            ModelCreateInfo modelCreateInfo(scale, 1.0f, 0.0f);
            return loadFromFile(filename, layout, &modelCreateInfo, flags);
        }

        // For measuring the time
        long renderingStartTime = -1;
        void loadFromServer(){
            isDataChanged = false;
            Frame *frame = frames.getFrame();
            if (frame != nullptr) {
                // Treat the positions as the center of the objects
                std::vector<float> &positions = frame->points;
                int size = positions.size();
                if (size > 0 && size % 3 == 0) {
                    // For measuring the time
                    renderingStartTime = getCurrentTimeMillis();
//                    long startTime = getCurrentTimeMillis();
                    duplicateObjects(positions);
//                    long duration = getCurrentTimeMillis() - startTime;
//                    LOGD("Time consuming of duplicating cubes: %d", duration);
                    isDataChanged = true;
                } else {
                    // error, position data is not correct
                    LOGE("The positions data is wrong.");
                }
                delete (frame);
            }
        }

        void duplicateOneObject(glm::vec3 center){
            int size = vertexBuffer.size();
            int stride = size / vertexCount;
            int currentSize = renderingVertices.size();
            renderingVertices.insert(renderingVertices.end(), vertexBuffer.begin(), vertexBuffer.end());
            for(int i = 0; i < size; i += stride){
                renderingVertices[currentSize+i] = renderingVertices[currentSize+i] + center.x;
                renderingVertices[currentSize+i + 1] = renderingVertices[currentSize+i + 1] + center.y;
                renderingVertices[currentSize+i + 2] = renderingVertices[currentSize+i + 2] + center.z;
                renderingVertices[currentSize+i + 8] = center.x;
                renderingVertices[currentSize+i + 9] = center.y;
                renderingVertices[currentSize+i + 10] = center.z;
            }

            // duplicate indices
            const int N = indexBuffer.size();
            for(int i = 0; i < N; i++){
                renderingIndices.push_back(indexBuffer.at(i) + renderingIndicesCount);
            }

            renderingVerticesCount += vertexCount;
            renderingIndicesCount += indexCount;

        }

        void updateOneObject(int size, int stride, int offset, float x, float y, float z){
            int currentSize = size * offset;
            for(int i = 0; i < size; i += stride){
                renderingVertices[currentSize + i] = vertexBuffer[i] + x;
                renderingVertices[currentSize + i + 1] = vertexBuffer[i + 1] + y;
                renderingVertices[currentSize + i + 2] = vertexBuffer[i + 2] + z;
                renderingVertices[currentSize + i + 8] = 0.0f;
                renderingVertices[currentSize + i + 9] = 0;
                renderingVertices[currentSize + i + 10] = 0.5f;
            }
        }

        void clearRenderingData(){
            renderingVertices.clear();
            renderingIndices.clear();
            renderingVerticesCount = 0;
            renderingIndicesCount = 0;
        }

        void duplicateObjects(std::vector<float> &positions) {
            const int posSize = positions.size();
            if (posSize == prePosSize) {
                int offset = 0;
                int size = vertexBuffer.size();
                int stride = size / vertexCount;
                for (int n = 0; n < posSize; n += 3) {
                    updateOneObject(size, stride, offset, positions[n], positions[n + 1],
                                    positions[n + 2]);
                    offset++;
                }
            } else {
                clearRenderingData();
                for (int n = 0; n < posSize; n += 3) {
                    glm::vec3 center = glm::vec3(positions[n], positions[n + 1], positions[n + 2]);
                    duplicateOneObject(center);
                }
                prePosSize = posSize;
            }
        }

    };

};

#endif