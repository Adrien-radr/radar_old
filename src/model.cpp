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
    const aiScene *scene = importer.ReadFile(fileName, aiProcess_Triangulate | aiProcess_CalcTangentSpace);

    if(!scene || scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        LogErr("AssimpError : ", importer.GetErrorString());
        return -1;
    }

    u32 last_slash = fileName.find_last_of('/');
    model.resourceName = fileName.substr(last_slash+1, fileName.size());
    model.pathName = fileName.substr(0, last_slash+1);

    LogDebug("Loading Model : ", model.resourceName, " (from ", model.pathName, ")");


    if(!ModelResource::_ProcessAssimpMaterials(this, model, scene)) {
        return -1;
    }

    if(!ModelResource::_ProcessAssimpNode(this, model, scene->mRootNode, scene)) {
        return -1;
    }

    LogDebug(model.subMeshes.size(), " meshes, ", model.materials.size(), " materials, ");

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

static std::string GetTexturePath(aiMaterial *material, ModelResource::Data &model, aiTextureType type) {
    std::string path("");

    if(material->GetTextureCount(type) > 0) {
        aiString textureFile;
        material->GetTexture(type, 0, &textureFile);
        textureFile.Set(model.pathName + textureFile.C_Str());

        path = textureFile.C_Str();
    }

    return path;
}

bool ModelResource::_ProcessAssimpMaterials(Scene *gameScene, Data &model, const aiScene *scene) {
    // Load Materials and Textures
    if(scene->mNumMaterials) {
        model.materials.reserve(scene->mNumMaterials);
        
        for(u32 i = 0; i < scene->mNumMaterials; ++i) {
            aiMaterial *material = scene->mMaterials[i];

            Material::Handle mat_h;
            Render::Texture::Handle t_h;

            Material::Desc mat_desc;
            aiColor3D col;
            aiString material_name;

            material->Get(AI_MATKEY_NAME, material_name);
            // material->Get(AI_MATKEY_COLOR_AMBIENT, col);
            // mat_desc.Ka = aiColor3D_To_col3f(col);
            // material->Get(AI_MATKEY_COLOR_DIFFUSE, col);
            // mat_desc.Kd = aiColor3D_To_col3f(col);
            // material->Get(AI_MATKEY_COLOR_SPECULAR, col);
            // mat_desc.Ks = aiColor3D_To_col3f(col);
            // material->Get(AI_MATKEY_SHININESS, mat_desc.shininess);
            // mat_desc.shininess = std::min(1.f, mat_desc.shininess);
            // mat_desc.Ka.x = std::max(mat_desc.Ka.x, 0.05f);
            // mat_desc.Ka.y = std::max(mat_desc.Ka.y, 0.05f);
            // mat_desc.Ka.z = std::max(mat_desc.Ka.z, 0.05f);

            // LogDebug("Loading Material ", model.numSubMeshes, " ", material_name.C_Str(), " :\n\t\t\t\t\t"
                // "Ka (", mat_desc.Ka.x, mat_desc.Ka.y, mat_desc.Ka.z, "),\n\t\t\t\t\t"
                // "Kd (", mat_desc.Kd.x, mat_desc.Kd.y, mat_desc.Kd.z, "),\n\t\t\t\t\t"
                // "Ks (", mat_desc.Ks.x, mat_desc.Ks.y, mat_desc.Ks.z, "), shininess: ", mat_desc.shininess);

            // Those are texture based. Just use defaults
            mat_desc.uniform.Ka = col3f(0.1,0.1,0.1);
            mat_desc.uniform.Kd = col3f(1,1,1);
            mat_desc.uniform.Ks = col3f(1,1,1);
            mat_desc.uniform.shininess = 1.0;   // This gets multiplied by the Specular Texture in shader

            int tCount;

            // Ambient texture
            // mat_desc.ambientTexPath = GetTexturePath(material, model, aiTextureType_DIFFUSE);
            
            // Diffuse texture, no alpha
            mat_desc.diffuseTexPath = GetTexturePath(material, model, aiTextureType_DIFFUSE);

            // Specular Texture
            mat_desc.specularTexPath = GetTexturePath(material, model, aiTextureType_SPECULAR);

            mat_h = gameScene->Add(mat_desc);
            if(mat_h < 0) {
                LogErr("Error creating material from subMesh.");
                return false;
            }
            model.materials.push_back(mat_h);
        }
    }
    return true;
}

bool ModelResource::_ProcessAssimpNode(Scene *gameScene, Data &model, aiNode *node, const aiScene *scene) {
    // LogDebug("Loading ", node->mNumMeshes, " submeshes and ", node->mNumChildren, " subnodes.");
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

    // Index the used material/texture
    model.materialIdx.push_back(mesh->mMaterialIndex);

    return true;
}