#include "mesh.h"
#include <iostream>
#include <memory>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <assert.h>

using namespace std;

void Mesh::load(string path){
    Assimp::Importer importer;
    auto scene = importer.ReadFile(path.c_str(), 0);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        cout << "error: failed to load mesh" << endl;
        exit(0);
    }
    auto mesh = scene->mMeshes[0];
    u32 vertex_cnt = mesh->mNumVertices;
    u32 index_cnt = mesh->mNumFaces * 3;

    auto pos = mesh->mVertices;
    auto faces = mesh->mFaces;

    if (!pos||!faces) {
        cout << "invaild mesh" << endl;
        exit(0);
    }
    positions.resize(vertex_cnt);
    for(u32 i=0;i<vertex_cnt;i++){
        positions[i]={
            .x=pos[i].x,
            .y=pos[i].y,
            .z=pos[i].z
        };
    }
    indices.resize(index_cnt);
    for(u32 i=0;i<index_cnt;i+=3){
        assert(faces[i/3].mNumIndices==3);
        indices[i] = faces[i/3].mIndices[0];
        indices[i+1] = faces[i/3].mIndices[1];
        indices[i+2] = faces[i/3].mIndices[2];
    }
}