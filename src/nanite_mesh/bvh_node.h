#pragma once
#include <types.h>
#include <bounds.h>
#include <vector>
#include <memory>


enum BVHNodeType
{
    DEFAULT,
    VIRTUAL_NODE,
    NODE,
    LEAF
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
    Sphere sphere_bounds;
    Bounds bounds;

    std::vector<std::shared_ptr<BVHNode>> children;

    // leaf node has cluster ids
    std::vector<u32> clusters; 
};