#include <nanite_mesh.h>
#include <fmt/core.h>
#include "mesh_simplify.h"
#include "cluster.h"
#include <co/fs.h>

#include<queue>

using namespace fmt;

void NaniteMesh::build(Mesh& mesh){
    virtual_bvh_node=std::make_shared<BVHNode>();
    virtual_bvh_node->type=BVHNodeType::VIRTUAL_NODE;

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

        std::shared_ptr<BVHNode> root;
        build_a_level(clusters,level_offset,num_level_clusters,cluster_groups,mip_level,root);
        virtual_bvh_node->children.push_back(root);

        level_offset=next_level_offset;
        mip_level++;
    }
    num_mip_level=mip_level+1;
    print("\n# end building cluster DAG \n\n");
    print("----------------------------------------\n");
    print("total clusters: {}\n\n",clusters.size());
    print("-------------------- end build nanite mesh --------------------\n\n");
}

u32 NaniteMesh::get_depth0()
{
    return virtual_bvh_node->children.size();
}

std::vector<u32> NaniteMesh::get_depths()
{
    std::vector<u32> depth_count;
    queue<std::shared_ptr<BVHNode>> q;
    q.push(virtual_bvh_node);
    bool is_virtual=true;

    while(!q.empty())
    {
        u32 size=q.size();
        if(!is_virtual)
        {
            depth_count.push_back(size);
        }
        if(is_virtual)
        {
            is_virtual=false;
        }
        for(u32 i=0;i<size;i++)
        {
            std::shared_ptr<BVHNode> curr=q.front();
            q.pop();
            for(auto& child:curr->children)
            {
                q.push(child);
            }
        }
    }
    return depth_count;
}

void NaniteMesh::save(const vector<u32>& data,string save_path)
{
    // write
    print("# writting nanite mesh data at: {}\n",save_path);
    fs::file file(save_path,'w');
    file.write(data.data(),data.size()*sizeof(u32));
    print("finish write.\n");
}

void NaniteMesh::save(const vector<u32>& data,string save_path, string depth_path)
{
    // write
    print("# writting nanite mesh data at: {}\n",save_path);
    fs::file file(save_path,'w');
    file.write(data.data(),data.size()*sizeof(u32));

    std::vector<u32> depths=get_depths();
    fs::file depth_file(depth_path,'w');
    depth_file.write(depths.data(),depths.size()*sizeof(u32));
    print("finish write.\n");
}

void NaniteMesh::load(string load_path,vector<u32>& data,u32& num_clusters)
{
    load(load_path,data);
    num_clusters=data[0];
}

void NaniteMesh::load(string load_path,vector<u32>& data)
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
    print("finish load: {}\n",load_path);
}

void NaniteMesh::load(string load_path,vector<u32>& data,string depth_path,std::vector<u32>& depths)
{
    load(load_path,data);
    load(depth_path,depths);
}