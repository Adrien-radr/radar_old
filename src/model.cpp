#include "scene.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

inline vec3f aiVector3D_To_vec3f(const aiVector3D &v) {
    return vec3f(v.x, v.y, v.z);
}

inline vec2f aiVector3D_To_vec2f(const aiVector3D &v) {
    return vec2f(v.x, v.y);
}

bool ModelResource::LoadFromFile(const std::string &fileName) {
    Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(fileName, aiProcess_Triangulate | aiProcess_GenNormals);

    if(!scene || scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        LogErr("AssimpError : ", importer.GetErrorString());
        return false;
    }

    u32 last_slash = fileName.find_last_of('/');
    resourceName = fileName.substr(last_slash+1, fileName.size());

    D(LogInfo("[DEBUG] Loading Model : ", resourceName);)

    ProcessAssimpNode(scene->mRootNode, scene);

    return true;
}

void ModelResource::ProcessAssimpNode(aiNode *node, const aiScene *scene) {
    for(u32 i = 0; i < node->mNumMeshes; ++i) {
        aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];

        /*TODO*/ProcessAssimpMesh(mesh, scene);
    }

    // TODO : parent-children relations (relative matrices etc ?)
    for(u32 i = 0; i < node->mNumChildren; ++i) {
        ProcessAssimpNode(node->mChildren[i], scene);
    }
}

bool ModelResource::ProcessAssimpMesh(aiMesh *mesh, const aiScene *scene) {
    u32 vertices_n = mesh->mNumVertices;
    u32 faces_n = mesh->mNumFaces;
    u32 indices_n = faces_n * 3;

    vec3f vp[vertices_n], vn[vertices_n];
    vec2f vt[vertices_n];
    u32 idx[indices_n];

    if(!mesh->mNormals) {
        LogErr("Mesh has no normals!");
        return -1;
    }

    for(u32 i = 0; i < vertices_n; ++i) {
        vp[i] = aiVector3D_To_vec3f(mesh->mVertices[i]);
        vn[i] = aiVector3D_To_vec3f(mesh->mNormals[i]);

        if(mesh->mTextureCoords[0]) {
            vt[i] = aiVector3D_To_vec2f(mesh->mTextureCoords[0][i]);
        }
    }

    for(u32 i = 0; i < faces_n; ++i) {
        aiFace &face = mesh->mFaces[i];
        for(u32 j = 0; j < 3; ++j) {
            idx[i*3+j] = face.mIndices[j];
        }
    }

    Render::Mesh::Desc mesh_desc(resourceName, false, indices_n, idx, vertices_n, (f32*)vp, (f32*)vn, (f32*)vt);
    Render::Mesh::Handle mesh_h = Render::Mesh::Build(mesh_desc);
    if(mesh_h < 0) {
        // LogErr("Error loading subMesh ", i, " of ", resourceName);
        return false;
    }
    
    subMeshes.push_back(mesh_h);
}