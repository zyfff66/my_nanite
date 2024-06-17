#pragma once

#include <types.h>
#include <vector>
#include <map>

struct Graph{
    std::vector<std::map<u32,i32>> g;

    void init(u32 n){
        g.resize(n);
    }
    void add_edge(u32 from,u32 to,i32 cost){
        g[from][to]=cost;
    }
    void increase_edge_cost(u32 from,u32 to,i32 i_cost){
        g[from][to]+=i_cost;
    }
};

struct MetisGraph;

class GraphPartitioner{

    // 平分网格
    u32 bisect_graph(MetisGraph* metis_graph, MetisGraph* child_graphs[2], u32 begin, u32 end);
    void recursive_bisect_graph(MetisGraph* metis_graph,u32 begin,u32 end);
    void init(u32 num);
public:
    std::vector<u32> indexes;// 按照划分结果有序排列的边（图中的边，其实是Mesh中三角形）序号，划分结果为0的在前，1在后
    std::vector<std::pair<u32,u32>> ranges;// 划分的结果,每一对表示一个相同划分的范围
    std::vector<u32> sorted_to;
    u32 max_partition_size; 
    u32 min_partition_size; 

    void partition(const Graph& graph,u32 min_part_size,u32 max_part_size);
};