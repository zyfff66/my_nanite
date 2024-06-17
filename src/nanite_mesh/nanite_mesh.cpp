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

u32 to_uint(f32 x)
{
    return *((u32*)&x);
}

void NaniteMesh::to_data(vector<u32>& data,u32& num_clusters)
{
    data.clear();

    data.push_back(clusters.size());// num cluster
    data.push_back(0);
    data.push_back(0);
    data.push_back(0);

    for(auto& cluster:clusters)
    {
        data.push_back(cluster.verts.size()); //num verts
        data.push_back(0);// vert offset
        data.push_back(cluster.indexes.size()/3); //num tris
        data.push_back(0);// offset

        // lod bounds
        data.push_back(to_uint(cluster.lod_bounds.center.x));
        data.push_back(to_uint(cluster.lod_bounds.center.y));
        data.push_back(to_uint(cluster.lod_bounds.center.z));
        data.push_back(to_uint(cluster.lod_bounds.radius));

        // parent lod bounds
        Sphere parent_lod_bounds=cluster_groups[cluster.group_id].lod_bounds;
        data.push_back(to_uint(parent_lod_bounds.center.x));
        data.push_back(to_uint(parent_lod_bounds.center.y));
        data.push_back(to_uint(parent_lod_bounds.center.z));
        data.push_back(to_uint(parent_lod_bounds.radius));

        // lod error
        f32 max_parent_lod_error=cluster_groups[cluster.group_id].max_parent_lod_error;
        data.push_back(to_uint(cluster.lod_error));
        data.push_back(to_uint(max_parent_lod_error));

        // group_id
        data .push_back(cluster.group_id);

        // mip_level
        data.push_back(cluster.mip_level);
    }

    u32 i=0;
    for(auto& cluster:clusters)
    {
        u32 offset=4+16*i;

        data[offset+1]=data.size();//vert offset
        for(vec3 p:cluster.verts)
        {
            data.push_back(to_uint(p.x));
            data.push_back(to_uint(p.y));
            data.push_back(to_uint(p.z));
        }

        data[offset+3]=data.size();// tri offset
        for(u32 j=0;j<cluster.indexes.size()/3;j++)
        {
            u32 i0=cluster.indexes[j*3];
            u32 i1=cluster.indexes[j*3+1];
            u32 i2=cluster.indexes[j*3+2];
            assert(i0<256&&i1<256&&i2<256);

            u32 packed_tri=(i0|(i1<<8)|(i2<<16));
            data.push_back(packed_tri);
        }
        i++;
    }
    num_clusters=data[0];

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