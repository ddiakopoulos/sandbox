#pragma once

#ifdef USE_FBX_SDK

#ifndef fbx_importer_hpp
#define fbx_importer_hpp

#include "model-io.hpp"
#include <vector>
#include <memory>
#include <unordered_map>
#include <map>

#include <fbxsdk/fbxsdk_nsbegin.h>
class FbxLayerElementNormal;
class FbxLayerElementVertexColor;
class FbxLayerElementUV;
class FbxMesh;
class FbxNode;
class FbxVector4;
class FbxScene;
class FbxAnimEvaluator;
#include <fbxsdk/fbxsdk_nsend.h>

////////////////////
//    Mesh Node   //
////////////////////

struct fbx_container
{
    std::map<std::string, runtime_skinned_mesh> meshes;
    std::map<std::string, skeletal_animation> animations;
};

void gather_meshes(fbx_container & file, fbxsdk::FbxNode * node);

fbx_container import_fbx_file(const std::string & file);

#endif // end fbx_importer_hpp

#endif // end USE_FBX_SDK
