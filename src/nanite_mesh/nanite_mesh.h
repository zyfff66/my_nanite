#pragma once

#include <cluster.h>
#include <mesh.h>
#include <string>

struct NaniteMesh
{
    std::vector<Cluster> clusters;
    std::vector<ClusterGroup> cluster_groups;
    u32 num_mip_level;

    void build(Mesh& mesh);
    void save(const std::vector<u32>& data,std::string save_path);
    static void load(std::string load_path,std::vector<u32>& packed_data,u32& num_clusters);
};