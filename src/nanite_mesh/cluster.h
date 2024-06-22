#pragma once

#include <bounds.h>
#include <vector>
#include <vec_types.h>
#include <bvh_node.h>


struct Cluster
{
    u32 cluster_group_index;

    u32 mip_level;
    u32 group_id;
    f32 lod_error;

    Bounds bounds;
    Sphere sphere_bounds;
    Sphere lod_bounds;

    std::vector<vec3> verts; // cluster中顶点
    std::vector<u32> indexes; // cluster中的索引
    std::vector<u32> external_edges;// cluster的外围对应的索引


};

struct ClusterGroup{
    Bounds bounds;
    Sphere lod_bounds;
    f32 min_lod_error;
    f32 max_parent_lod_error;
    u32 mip_level;
    
    std::vector<u32> clusters; 
    std::vector<std::pair<u32,u32>> external_edges; // <cluster id,edge id>
};

void cluster_triangles(
    const std::vector<vec3>& vertexs,
    const std::vector<u32>& indexes, 
    std::vector<Cluster>& clusters);

void build_a_level(
    std::vector<Cluster>& clusters,
    u32 level_offset,
    u32 num_level_cluster,
    std::vector<ClusterGroup>& cluster_groups,
    u32 mip_level,
    std::shared_ptr<BVHNode>& root);