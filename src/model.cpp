#include "scene.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

inline vec3f aiVector3D_To_vec3f(const aiVector3D &v) {
    return vec3f(v.x, v.y, v.z);
}

inline col3f aiColor3D_To_col3f(const aiColor3D &v) {
    return col3f(v.r, v.g, v.b);
}

inline vec2f aiVector3D_To_vec2f(const aiVector3D &v) {
    return vec2f(v.x, v.y);
}

ModelResource::Handle Scene::LoadModelResource(const std::string &fileName) {
	ModelResource::Data model;

    Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(fileName, aiProcess_Triangulate);

    if(!scene || scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        LogErr("AssimpError : ", importer.GetErrorString());
        return -1;
    }

    u32 last_slash = fileName.find_last_of('/');
    model.resourceName = fileName.substr(last_slash+1, fileName.size());
    model.pathName = fileName.substr(0, last_slash+1);

    LogDebug("Loading Model : ", model.resourceName, " (from ", model.pathName, ")");

    // Load Materials and Textures
    if(0 && scene->mNumMaterials) {
        LogDebug("nTextures:", scene->mNumTextures, " nMaterials:", scene->mNumMaterials);
        for(u32 i = 0; i < scene->mNumMaterials; ++i) {
            aiMaterial *material = scene->mMaterials[i];
            bool hasTextures = false;
            int tCount;
            if((tCount = material->GetTextureCount(aiTextureType_AMBIENT)) > 0) {
                LogDebug(i, " has ", tCount, " Ambient Textures");
            }
            if((tCount = material->GetTextureCount(aiTextureType_DIFFUSE)) > 0) {
                LogDebug(i, " has ", tCount, " Diffuse Textures");

                aiString textureFile;
                material->GetTexture(aiTextureType_DIFFUSE, 0, &textureFile);
                textureFile.Set(model.pathName + textureFile.C_Str());
                Render::Texture::Desc t_desc(textureFile.C_Str());
                Render::Texture::Handle t_h = Render::Texture::Build(t_desc);
                if(t_h < 0) {
                    LogErr("Error loading diffuse texture ", textureFile.C_Str());
                    return -1;
                }

                hasTextures = true;
                model.textures.push_back(t_h);
            }
            if((tCount = material->GetTextureCount(aiTextureType_SPECULAR)) > 0) {
                // LogDebug(i, " has ", tCount, " Specular Textures");
            }

            if(!hasTextures) {
                LogDebug(i, " has Debug Tex");
                model.textures.push_back(Render::Texture::DEFAULT_TEXTURE);
            }
        }

    }
    if(!ModelResource::_ProcessAssimpNode(this, model, scene->mRootNode, scene)) {
        return -1;
    }

    LogDebug(model.subMeshes.size(), " meshes, ", model.materials.size(), " materials, ", model.textures.size(), " textures.");

	size_t index = models.size();
	models.push_back(model);

    return (ModelResource::Handle)index;
}

ModelResource::Handle Scene::GetModelResource(const std::string &modelName) {
    // Linear search through models.
   	auto it = std::find_if(models.begin(), models.end(), [&](const ModelResource::Data &model) {
           return model.resourceName == modelName;
       });

    if(it != models.end())
        return std::distance(models.begin(), it);
    else
        return -1;
}

bool ModelResource::_ProcessAssimpNode(Scene *gameScene, Data &model, aiNode *node, const aiScene *scene) {
    LogDebug("Loading ", node->mNumMeshes, " submeshes and ", node->mNumChildren, " subnodes.");
    for(u32 i = 0; i < node->mNumMeshes; ++i) {
        aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];

        if(!_ProcessAssimpMesh(gameScene, model, mesh, scene)) {
            LogErr("Error loading subMesh ", i, " of ", model.resourceName);
            return false;
        }
        ++model.numSubMeshes;
    }

    // TODO : parent-children relations (relative matrices etc ?)
    for(u32 i = 0; i < node->mNumChildren; ++i) {
        if(!_ProcessAssimpNode(gameScene, model, node->mChildren[i], scene))
            return false;
    }

    return true;
}

bool ModelResource::_ProcessAssimpMesh(Scene *gameScene, Data &model, aiMesh *mesh, const aiScene *scene) {
    u32 vertices_n = mesh->mNumVertices;
    u32 faces_n = mesh->mNumFaces;
    u32 indices_n = faces_n * 3;

    vec3f vp[vertices_n], vn[vertices_n];
    vec2f vt[vertices_n];
    u32 idx[indices_n];

    if(!mesh->mNormals) {
        LogErr("Mesh has no normals!");
        return false;
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

    std::stringstream ss;
    ss << model.resourceName << model.numSubMeshes;
    
    Render::Mesh::Desc mesh_desc(ss.str(), false, indices_n, idx, vertices_n, (f32*)vp, (f32*)vn, (f32*)vt);
    Render::Mesh::Handle mesh_h = Render::Mesh::Build(mesh_desc);
    if(mesh_h < 0) {
        LogErr("Error creating subMesh.");
        return false;
    }
    
    model.subMeshes.push_back(mesh_h);

    // Load associated material
    // TODO : Load first all the materials (scene->mNumMaterials), and then index them for each submeshes
    Material::Handle mat_h;
    Material::Desc mat_desc;

    aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];
    aiColor3D col;
    aiString material_name;

    material->Get(AI_MATKEY_COLOR_AMBIENT, col);
    mat_desc.Ka = aiColor3D_To_col3f(col);
    material->Get(AI_MATKEY_COLOR_DIFFUSE, col);
    mat_desc.Kd = aiColor3D_To_col3f(col);
    material->Get(AI_MATKEY_COLOR_SPECULAR, col);
    mat_desc.Ks = aiColor3D_To_col3f(col);
    material->Get(AI_MATKEY_SHININESS, mat_desc.shininess);
    mat_desc.shininess = std::min(1.f, mat_desc.shininess);
    mat_desc.Ka.x = std::max(mat_desc.Ka.x, 0.05f);
    mat_desc.Ka.y = std::max(mat_desc.Ka.y, 0.05f);
    mat_desc.Ka.z = std::max(mat_desc.Ka.z, 0.05f);
    
    material->Get(AI_MATKEY_NAME, material_name);
    mat_desc.Ka = col3f(0.1,0.1,0.1);
    mat_desc.Kd = col3f(1,1,1);
    mat_desc.Ks = col3f(1,1,1);
    mat_desc.shininess = 0.3;

    Render::Texture::Handle t_h;
    if(material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
        aiString textureFile;
        material->GetTexture(aiTextureType_DIFFUSE, 0, &textureFile);
        textureFile.Set(model.pathName + textureFile.C_Str());
        Render::Texture::Desc t_desc(textureFile.C_Str());
        t_h = Render::Texture::Build(t_desc);
        if(t_h < 0) {
            LogErr("Error loading diffuse texture ", textureFile.C_Str());
            return false;
        }
    } else {
        t_h = Render::Texture::DEFAULT_TEXTURE;
    }
    model.textures.push_back(t_h);


    // LogDebug("Loading Material ", model.numSubMeshes, " ", material_name.C_Str(), " :\n\t\t\t\t\t"
        // "Ka (", mat_desc.Ka.x, mat_desc.Ka.y, mat_desc.Ka.z, "),\n\t\t\t\t\t"
        // "Kd (", mat_desc.Kd.x, mat_desc.Kd.y, mat_desc.Kd.z, "),\n\t\t\t\t\t"
        // "Ks (", mat_desc.Ks.x, mat_desc.Ks.y, mat_desc.Ks.z, "), shininess: ", mat_desc.shininess);
    mat_h = gameScene->Add(mat_desc);
    if(mat_h < 0) {
        LogErr("Error creating material from subMesh.");
        return false;
    }

    model.materials.push_back(mat_h);

    // Load Texture if any, if not, put default white one

    return true;
}