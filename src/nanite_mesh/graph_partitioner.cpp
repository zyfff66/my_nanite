#include <graph_partitioner.h>
#include <metis.h>
#include <assert.h>
#include <algorithm>

using namespace std;


struct MetisGraph{
    idx_t num;// 数量 
    vector<idx_t> adj_offset;// 邻边位移列表 xadj
    vector<idx_t> adj; // 邻边列表 adjncy
    vector<idx_t> adj_cost; // 邻边权重 adjwgt
};

/**
 * @brief 转换为metis库的图
 * 
 * @param graph 三角形的邻接关系
 * @return MetisGraph* Metis图
 */
MetisGraph* to_metis_data(const Graph& graph)
{
    MetisGraph* g = new MetisGraph;
    g->num = graph.g.size();
    for(auto& from:graph.g)
    {
        g->adj_offset.push_back(g->adj.size());
        for(auto [to,cost]:from)
        {
            g->adj.push_back(to);
            g->adj_cost.push_back(cost);
        }
    }
    g->adj_offset.push_back(g->adj.size());
    return g;
}

u32 divide_round_nearest(u32 dividend, u32 divisor)
{
    return (dividend >= 0)
    ? (dividend + divisor / 2)/ divisor
    : (dividend - divisor / 2 + 1 )/ divisor;
}

void GraphPartitioner::init(u32 num)
{
    sorted_to.resize(num);
    indexes.resize(num);
    u32 i=0;
    for(u32& x:indexes)
    {
        x=i;
        i++;
    }
    i=0;
    for(u32& x:sorted_to)
    {
        x=i;
        i++;
    }
}

/**
 * @brief 划分图
 * 转成metis图时，三角形是顶点，相邻关系是边，这样划分后是将三角形划分在一起
 * @param metis_graph 未划分的图 
 * @param child_graphs 如果划分之后满足size的要求，不生成；划分之后不满足size的要求，生成两个child_graph供下一轮划分
 * @param begin 图的开始
 * @param end 图的结束
 * @return u32 划分的下标位置
 */
u32 GraphPartitioner::bisect_graph(MetisGraph* metis_graph, MetisGraph* child_graphs[2], u32 begin, u32 end)
{
    assert(end-begin == metis_graph->num);

    if(metis_graph->num <= max_partition_size)
    {
        ranges.push_back({begin,end});
        return end;
    }

    const u32 target_partition_size = (min_partition_size + max_partition_size)/2;
    const u32 target_num_partitions = max(2u,divide_round_nearest(metis_graph->num,target_partition_size));
    // const u32 target_num_partitions = max(2u,(metis_graph->num+target_partition_size-1)/target_partition_size);


    idx_t num_constraints=1, num_parts=2, edges_cut=0;
    real_t partition_weights[]={
        float(target_num_partitions>>1)/target_num_partitions,
		1.0f-float(target_num_partitions>>1)/target_num_partitions
    };

    // idx_t options[ METIS_NOPTIONS ];
    // METIS_SetDefaultOptions( options );
    // // 在高层级允许宽松的容差, 严格的平衡在更接近分区大小之前并不重要
    // bool b_loose = target_num_partitions >= 128 || max_partition_size / min_partition_size > 1;
    // options[ METIS_OPTION_UFACTOR ] = b_loose ? 200 : 1;

    // metis的递归划分
    vector<idx_t> partition_ids(metis_graph->num);
    // https://ivan-pi.github.io/fmetis/interface/metis_partgraphrecursive.html
    int r = METIS_PartGraphRecursive(
        &metis_graph->num,
        &num_constraints,
        metis_graph->adj_offset.data(),
        metis_graph->adj.data(),
        nullptr, // Vert weights
        nullptr, // Vert sizes for computing the total communication volume
        metis_graph->adj_cost.data(),
        &num_parts,
        partition_weights,
        nullptr, 
        // options,
        nullptr,
        &edges_cut,
        partition_ids.data()
    );
    assert(r==METIS_OK);

    // 划分数组，在swapped_with中partition_ids==0的放在前面 partition_ids == 1的放在后面
    i32 front=0 , back = metis_graph->num-1;
    vector<idx_t> swapped_with(metis_graph->num);
    while(front<=back)
    {
        while(front<=back&&partition_ids[front]==0) 
        {
            swapped_with[front]=front;
            front++;
        }
        while(front<=back&&partition_ids[back]==1)
        {
            swapped_with[back]=back;
            back--;
        } 
        if(front<back)
        {
            swap(indexes[begin+front],indexes[begin+back]);
            swapped_with[front]= back;
            swapped_with[back]=front;
            front++;
            back--;
        }
    }

    i32 split = front;
    i32 num[2]={split,metis_graph->num-split};
    assert(num[0]>=1 && num[1]>=1);
    // 两个都满足小于最大的数量要求，直接添加
    if(num[0]<=max_partition_size&&num[1]<=max_partition_size)
    {
        ranges.push_back({begin,begin+split});
        ranges.push_back({begin+split,end});
    }
    else
    {
        for(u32 i=0;i<2;i++)
        {
            child_graphs[i]=new MetisGraph;
            child_graphs[i]->adj.reserve(metis_graph->adj.size()>>1);
            child_graphs[i]->adj_cost.reserve(metis_graph->adj_cost.size()>>1);
            child_graphs[i]->adj_offset.reserve(num[i]+1);
            child_graphs[i]->num=num[i];
        }
        // 将graph的邻边加入到child_graphs[0]或child_graphs[1]
        for(u32 i=0;i<metis_graph->num;i++)
        {
            u32 is_0_or_1=(i>=child_graphs[0]->num);
            MetisGraph* child = child_graphs[is_0_or_1];
            child->adj_offset.push_back(child->adj.size());
            u32 org_index=swapped_with[i];// swapped中按照0 1分类的第i个的在graph中的下标

            // metis_graph->adj_offset[org_index]表达了第org_index个边顶点
            for(idx_t adj_index=metis_graph->adj_offset[org_index];adj_index<metis_graph->adj_offset[org_index+1];adj_index++)
            {
                idx_t adj = metis_graph->adj[adj_index];
                idx_t adj_cost=metis_graph->adj_cost[adj_index];

                adj=swapped_with[adj]-(is_0_or_1?num[0]:0);
                if(0<=adj&& adj<num[is_0_or_1])
                {
                    child->adj.push_back(adj);
                    child->adj_cost.push_back(adj_cost);
                }
            }
        }
        // xadj的n+1位
        child_graphs[0]->adj_offset.push_back(child_graphs[0]->adj.size());
        child_graphs[1]->adj_offset.push_back(child_graphs[1]->adj.size());
    }
    return begin+split;
}

void GraphPartitioner::recursive_bisect_graph(MetisGraph* metis_graph,u32 begin,u32 end)
{
    MetisGraph* child_graphs[2]={0};
    u32 split=bisect_graph(metis_graph,child_graphs,begin,end);
    delete metis_graph;

    if(child_graphs[0]&&child_graphs[1])
    {
        recursive_bisect_graph(child_graphs[0],begin,split);
        recursive_bisect_graph(child_graphs[1],split,end);
    }
    else{
        assert(!child_graphs[0]&&!child_graphs[1]);
    }
}
 
void GraphPartitioner::partition(const Graph& graph,u32 min_part_size,u32 max_part_size)
{
    init(graph.g.size());
    this->min_partition_size=min_part_size;
    this->max_partition_size=max_part_size;
    MetisGraph* metis_graph=to_metis_data(graph);
    recursive_bisect_graph(metis_graph,0,metis_graph->num);
    sort(ranges.begin(),ranges.end());
    for(u32 i=0;i<indexes.size();i++)
    {
        sorted_to[indexes[i]] = i;
    }

}