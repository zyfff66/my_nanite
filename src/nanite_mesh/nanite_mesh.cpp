#include <nanite_mesh.h>
#include <fmt/core.h>
#include "mesh_simplify.h"
#include "cluster.h"
#include <co/fs.h>

using namespace fmt;


void NaniteMesh::build(Mesh& mesh){
    print("\n-------------------- begin building nanite mesh --------------------\n\n");

    auto& [pos,idx]=mesh;
    print("# simply mesh\n");    

    MeshSimplifier simplifier(pos.data(),pos.size(),idx.data(),idx.size());
    simplifier.simplify(idx.size());
    pos.resize(simplifier.remaining_num_verts);
    idx.resize(simplifier.remaining_num_tris*3);
    print("----------------------------------------\n");
    print("verts: {}, triangles: {}\n",pos.size(),idx.size()/3);
    print("----------------------------------------\n\n");


    print("# clustering triangles\n\n");
    cluster_triangles(pos,idx,clusters);

    u32 level_offset=0,mip_level=0;
    print("# begin building cluster DAG\n");
    while(1)
    {
        print("-------------------- level {} --------------------\n",mip_level);
        print("num clusters: {}\n",clusters.size()-level_offset);

        u32 next_level_offset=clusters.size();
        u32 num_level_clusters=clusters.size()-level_offset;
        if(num_level_clusters<=1) break;
        build_a_level(clusters,level_offset,num_level_clusters,cluster_groups,mip_level);
        level_offset=next_level_offset;
        mip_level++;
    }
    num_mip_level=mip_level+1;
    print("\n# end building cluster DAG \n\n");
    print("----------------------------------------\n");
    print("total clusters: {}\n\n",clusters.size());
    print("-------------------- end build nanite mesh --------------------\n\n");
}   





void NaniteMesh::save(const vector<u32>& data,string save_path)
{
    // write
    print("# writting nanite mesh data at: {}\n",save_path);
    fs::file file(save_path,'w');
    file.write(data.data(),data.size()*sizeof(u32));
    print("finish write.\n");
}

void NaniteMesh::load(string load_path,vector<u32>& data,u32& num_clusters)
{
    fs::file file(load_path,'r');
    if(!file)
    {
        print("failed: {} is invaild",load_path);
        return;
    }
    data.resize(file.size()/sizeof(u32));

    print("# loading packed data...\n");
    file.read(data.data(),file.size());
    num_clusters=data[0];
    print("finish load: {}\n",load_path);
}