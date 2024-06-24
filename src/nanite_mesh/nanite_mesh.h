#pragma once

#include <cluster.h>
#include <mesh.h>
#include <string>
#include <bvh_node.h>

struct NaniteMesh
{
    std::vector<Cluster> clusters;
    std::vector<ClusterGroup> cluster_groups;
    u32 num_mip_level;

    void build(Mesh& mesh);
    void save(const std::vector<u32>& data,std::string save_path);
    void save(const std::vector<u32>& data,std::string save_path, std::string depth_path);
    static void load(std::string load_path,std::vector<u32>& packed_data);
    static void load(std::string load_path,std::vector<u32>& packed_data,u32& num_clusters);
    static void load(std::string load_path,std::vector<u32>& data,std::string depth_path,std::vector<u32>& depths);


    std::shared_ptr<BVHNode> virtual_bvh_node;
    u32 get_depth0();
    std::vector<u32> get_depths();
};