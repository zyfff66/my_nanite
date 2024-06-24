#pragma once
#include <types.h>
#include <bounds.h>
#include <vector>
#include <memory>


enum BVHNodeType
{
    DEFAULT=0,
    VIRTUAL_NODE=1,
    NODE=2,
    LEAF=3
};

struct BVHNode
{
    BVHNode()
    {
        
    };

    u32 start=-1;
    u32 end=-1;
    u32 depth=0;

    BVHNodeType type=BVHNodeType::DEFAULT;
    // Sphere sphere_bounds;
    Bounds bounds;

    std::vector<std::shared_ptr<BVHNode>> children;

    // leaf node has cluster ids
    // std::vector<u32> clusters; 
    u32 group_id=-1;
};