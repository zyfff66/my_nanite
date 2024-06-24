
#include <cluster.h>
#include <graph_partitioner.h>
#include "hash_table.h"
#include "mesh_simplify.h"
#include <unordered_map>
#include <vec_math.h>
#include "bounds.h"
#include <assert.h>
#include <config.h>
#include <stack>
// #include <openmesh_simplify.h>

using namespace std;

/**
 * @brief 生成cluster
 * 
 * @param vertexs mesh中的顶点
 * @param indexes mesh面每个顶点对应顶点下标
 * @param begin cluster开始的三角形位置
 * @param end cluster结束的三角形位置
 * @param graph_partitioner 划分器
 * @param adjacency_edge_graph 邻接边关系
 * @param cluster 生成的cluster
 */
void build_a_cluster(
    const vector<vec3>& vertexs,
    const vector<u32>& indexes,
    u32 begin,
    u32 end,
    const GraphPartitioner& graph_partitioner, 
    const Graph& adjacency_edge_graph, 
    Cluster& cluster)
{
    unordered_map<u32,u32> mp;
    for(u32 i=begin;i<end;i++)
    {
        u32 idx=graph_partitioner.indexes[i];
        for(u32 k=0;k<3;k++)
        {
            u32 e_idx=idx*3+k;
            u32 v_idx=indexes[e_idx];
            if(mp.find(v_idx)==mp.end())
            {
                mp[v_idx]=cluster.verts.size();
                cluster.verts.push_back(vertexs[v_idx]);
            }
            bool is_external=false;
            for(auto[adj_edge,_]:adjacency_edge_graph.g[e_idx])
            {
                u32 adj_triangle = graph_partitioner.sorted_to[adj_edge/3];
                // 邻接的三角形在另外的cluster中，说明在cluster的外围
                if(adj_triangle<begin||adj_triangle>=end)
                {
                    is_external=true;
                    break;
                }
            }
            if(is_external)
            {
                cluster.external_edges.push_back(cluster.indexes.size());
            }
            cluster.indexes.push_back(mp[v_idx]);
        }
    }

    cluster.mip_level = 0;
    cluster.lod_error=0;
    cluster.sphere_bounds=Sphere::construct_from_points(cluster.verts.data(),cluster.verts.size());
    cluster.lod_bounds=cluster.sphere_bounds;
    cluster.bounds=cluster.verts[0];
    for(vec3 p:cluster.verts) 
        cluster.bounds = cluster.bounds+p;
}

/**
 * @brief 边哈希，记录边上邻接三角形（顶点共享，边相反）的数量
 * 
 *    /\
	 /  \
	o-<<-o
	o->>-o
	 \  /
	  \/
 * 
 * @param vertexs 顶点的位置
 * @param indexes 顶点的下标
 * @param adjacency_edge_graph 表示与边(i,j)的关联的邻接三角形的数量
 */
void build_adjacency_edge(
    const vector<vec3>& vertexs,
    const vector<u32>& indexes,
    Graph& adjacency_edge_graph)
{
    HashTable edge_hashtable(indexes.size());
    adjacency_edge_graph.init(indexes.size());

    for(u32 i=0;i<indexes.size();i++)
    {
        vec3 p0=vertexs[indexes[i]];
        vec3 p1=vertexs[indexes[cycle3(i)]];
        edge_hashtable.add(hash_position(p0,p1),i);

        for(u32 j:edge_hashtable[hash_position(p1,p0)])
        {
            if(p1==vertexs[indexes[j]]&&p0==vertexs[indexes[cycle3(j)]])
            {
                adjacency_edge_graph.increase_edge_cost(i,j,1);
                adjacency_edge_graph.increase_edge_cost(j,i,1);
            } 
        }
    }
}

/**
 * @brief 根据边的邻接关系构建三角形的邻接关系图
 *  
 * @param adjacency_edge_graph 边的邻接关系图
 * @param graph 三角形的邻接关系
 */
void build_adjacency_graph(
    const Graph& adjacency_edge_graph,
    Graph& graph)
{
    graph.init(adjacency_edge_graph.g.size()/3);

    u32 u=0;
    for(const auto& it:adjacency_edge_graph.g)
    {
        for(auto[to,cost]:it)
        {
            graph.increase_edge_cost(u/3,to/3,1);
        }
        u++;
    }
}
/**
 * @brief 划分为cluster 
 * 
 * @param vertexs 顶点位置
 * @param indexes 面对应点下标
 * @param clusters 生成的cluster
 */
void cluster_triangles(
    const vector<vec3>& vertexs,
    const vector<u32>& indexes,
    vector<Cluster>& clusters)
{
    Graph adjacency_edge_graph,graph;
    build_adjacency_edge(vertexs,indexes,adjacency_edge_graph);
    build_adjacency_graph(adjacency_edge_graph,graph);

    GraphPartitioner graph_partitioner;
    graph_partitioner.partition(graph,Config::cluster_size-4,Config::cluster_size);


    // range内生成一个cluster
    for(auto[begin,end]:graph_partitioner.ranges)
    {
        clusters.push_back({});
        Cluster& cluster =clusters.back();
        build_a_cluster(vertexs,indexes,begin,end,graph_partitioner,adjacency_edge_graph,cluster);
    }

}
/**
 * @brief 获得相邻的外围边的关系图
 * 
 * @param clusters cluster
 * @param external_edges 相邻的外围边 {cluster_id，外围边}
 * @param edge_link_graph 相邻的外围边的关系图
 */
void build_clusters_edge(
    const vector<Cluster>& clusters,
    const vector<pair<u32,u32>>& external_edges,
    Graph& edge_link_graph)
{
    HashTable edge_hashtable(external_edges.size());
    edge_link_graph.init(external_edges.size());

    u32 i=0;
    for(auto [cluster_id,ext_edge_id]:external_edges)
    {
        auto& vet=clusters[cluster_id].verts;
        auto& idxes=clusters[cluster_id].indexes;

        vec3 p0=vet[idxes[ext_edge_id]];
        vec3 p1=vet[idxes[cycle3(ext_edge_id)]];

        edge_hashtable.add(hash_position(p0,p1),i);
        for(auto j:edge_hashtable[hash_position(p1,p0)])
        {
            auto [cluster_id1,ext_edge_id1]=external_edges[j];
            auto& vet1=clusters[cluster_id1].verts;
            auto& idxes1=clusters[cluster_id1].indexes;
            if(vet1[idxes1[ext_edge_id1]]==p1&&vet1[idxes1[cycle3(ext_edge_id1)]]==p0)
            {
                edge_link_graph.increase_edge_cost(i,j,1);
                edge_link_graph.increase_edge_cost(j,i,1);
            }
        }
        i++;
    }
}

/**
 * @brief 获得相邻的cluster的关系图
 * 
 * @param edge_link_graph cluster边的相邻关系
 * @param cluster_map 外围边的id映射为cluster_id
 * @param num_cluster cluster的数量
 * @param graph 表示cluster邻接关系的图
 */
void build_clusters_graph(
    const Graph& edge_link_graph,
    const vector<u32>& cluster_map,
    u32 num_cluster,
    Graph& graph
){
    graph.init(num_cluster);
    u32 u=0;
    for(const auto& it:edge_link_graph.g)
    {
        for(auto[to,cost]:it)
        {
            graph.increase_edge_cost(cluster_map[u],cluster_map[to],1);
        }
        u++;
    }
}
/**
 * @brief 根据[begin,end)将对应的cluster建立为一个group
 * 
 * @param clusters 
 * @param level_offset 当前mip_level在clusters中的开始下标
 * @param begin 
 * @param end 
 * @param cluster_ext_id 属于第i个cluster的外围边_id范围
 * @param cluster_map 将edge_id映射为cluster_id
 * @param graph_partitioner 划分器
 * @param edge_graph 外围边相邻关系
 * @param group 生成的group
 */
void build_a_group(
    vector<Cluster>& clusters,
    u32 level_offset,
    u32 begin,
    u32 end,
    u32 group_id,
    const vector<u32>& cluster_ext_id,
    const vector<u32>& cluster_map,
    vector<pair<u32,u32>>& external_edges,
    const GraphPartitioner& graph_partitioner, 
    const Graph& edge_graph, 
    ClusterGroup& group)
{
    for(u32 i=begin;i<end;i++)
    {
        u32 cluster_idx=graph_partitioner.indexes[i];
        clusters[level_offset+cluster_idx].group_id=group_id;
        group.clusters.push_back(level_offset+cluster_idx);
        for(u32 e_idx=cluster_ext_id[cluster_idx];e_idx<cluster_map.size()&&cluster_map[e_idx]==cluster_idx;e_idx++)
        {
            bool is_external=false; 
            for(auto [adj_edge,_]:edge_graph.g[e_idx])
            {
                u32 adj_cluster=graph_partitioner.sorted_to[cluster_map[adj_edge]];
                if(adj_cluster<begin||adj_cluster>=end)
                {
                    is_external=true;
                    break;
                }
            }
            if(is_external)
            {
                u32 e=external_edges[e_idx].second;
                group.external_edges.push_back({level_offset+cluster_idx,e});
            }
        }
    }
}
/**
 * @brief cluster分组成group
 * 
 * @param clusters 
 * @param level_offset 
 * @param num_level_cluster 
 * @param cluster_groups 
 * @param mip_level 
 */
void group_clusters(
    vector<Cluster>& clusters,
    u32 level_offset,
    u32 num_level_cluster,
    vector<ClusterGroup>& cluster_groups,
    u32 mip_level)
{
    // 当前level cluster
    vector<Cluster> clusters_current_level(clusters.begin()+level_offset,clusters.begin()+level_offset+num_level_cluster);


    // 之前把三角形聚成cluster；这里把cluster视为之前的三角形，将cluster聚成group;
    vector<u32> cluster_map;// 外围边映射为边属于的cluster_id
    vector<u32> cluster_ext_edge_start_cluster_id;// 第i个cluster对应的外围边id范围[cluster_map[cluster_ext_edge_start_cluster_id[i-1]],cluster_map[cluster_ext_edge_start_cluster_id[i]]] 
    vector<pair<u32,u32>> external_edges;// cluster的外围边，相当于三角形的边
    u32 i=0;
    for(auto& cluster:clusters_current_level)
    {
        assert(cluster.mip_level==mip_level);
        cluster_ext_edge_start_cluster_id.push_back(cluster_map.size());
        for(u32 e:cluster.external_edges)
        {
            external_edges.push_back({i,e});// cluster_id 外围边
            cluster_map.push_back(i);
        }
        i++;
    }
    Graph edge_link_graph,graph;
    build_clusters_edge(clusters_current_level,external_edges,edge_link_graph);
    build_clusters_graph(edge_link_graph,cluster_map,num_level_cluster,graph);

    GraphPartitioner graph_partitioner;
    graph_partitioner.partition(graph,Config::group_size-4,Config::group_size);

    for(auto[begin,end]:graph_partitioner.ranges)
    {
        cluster_groups.push_back({});
        auto& group=cluster_groups.back();
        group.mip_level = mip_level;
        for(u32 i=begin;i<end;i++)
        {
            u32 cluster_idx=graph_partitioner.indexes[i];
            clusters[level_offset+cluster_idx].group_id=cluster_groups.size()-1;
            group.clusters.push_back(level_offset+cluster_idx);
            for(u32 e_idx=cluster_ext_edge_start_cluster_id[cluster_idx];e_idx<cluster_map.size()&&cluster_map[e_idx]==cluster_idx;e_idx++)
            {
                bool is_external=false; 
                for(auto [adj_edge,_]:edge_link_graph.g[e_idx])
                {
                    u32 adj_cluster=graph_partitioner.sorted_to[cluster_map[adj_edge]];
                    if(adj_cluster<begin||adj_cluster>=end)
                    {
                        is_external=true;
                        break;
                    }
                }
                if(is_external)
                {
                    // printf("e_idx:%d,ext_size:%d\n",e_idx,external_edges.size());
                    u32 e=external_edges[e_idx].second;
                    // printf("e_idx:%d,e:%d\n",e_idx,e);

                    group.external_edges.push_back({level_offset+cluster_idx,e});
                }
            }
        }

    }

}
/**
 * @brief 生成上一层group:
 * 合并简化cluster
 * 建立group的父子关系，设置cluster的parent error和当前error
 * @param cluster_group 
 * @param clusters 
 */
void build_parent(
    ClusterGroup& cluster_group,
    vector<Cluster>& clusters)
{
    vector<vec3> pos;
    vector<u32> idx;

    vector<Sphere> lod_bounds;
    f32 parent_lod_error=0;
    u32 idx_offset=0;
    for(u32 cluster_id:cluster_group.clusters)
    {
        auto& cluster=clusters[cluster_id];
        for(vec3 p:cluster.verts) 
            pos.push_back(p);
        for(u32 i:cluster.indexes) 
            idx.push_back(i+idx_offset);
        idx_offset+=cluster.verts.size();

        lod_bounds.push_back(cluster.lod_bounds);
        parent_lod_error=max(parent_lod_error,cluster.lod_error);// 父节点的error是子结点中最大的
    }

    Sphere parent_lod_bound=Sphere::construct_from_sphere_bounds(lod_bounds.data(),lod_bounds.size());
    
    // OpenMeshSimplifier openmesh_simplifier(pos,idx);
    // openmesh_simplifier.simplify((Config::cluster_size-2)*(cluster_group.clusters.size()/2),cluster_group.external_edges,clusters);
    // openmesh_simplifier.to_vec(pos,idx);

    MeshSimplifier simplifier(pos.data(),pos.size(),idx.data(),idx.size());

    HashTable edge_hash(cluster_group.external_edges.size());
    u32 i=0;
    for(auto [cluster_id,e_id]:cluster_group.external_edges)
    {
        auto& pos=clusters[cluster_id].verts;
        auto& idx=clusters[cluster_id].indexes;
        vec3 p0=pos[idx[e_id]],p1=pos[idx[cycle3(e_id)]];
        edge_hash.add(hash_position(p0,p1),i);
        simplifier.lock_position(p0);
        simplifier.lock_position(p1);
        i++;
    }
    simplifier.simplify((Config::cluster_size-2)*(cluster_group.clusters.size()/2));

    pos.resize(simplifier.remaining_num_verts);
    idx.resize(simplifier.remaining_num_tris*3);

    parent_lod_error=max(parent_lod_error,sqrt(simplifier.max_error));

    Graph adjacency_edge_graph,graph;
    build_adjacency_edge(pos,idx,adjacency_edge_graph);
    build_adjacency_graph(adjacency_edge_graph,graph);

    GraphPartitioner graph_partitioner;
    graph_partitioner.partition(graph,Config::cluster_size-4,Config::cluster_size);

    for(auto [begin,end]:graph_partitioner.ranges)
    {
        clusters.push_back({});
        Cluster& cluster=clusters.back();

        unordered_map<u32,u32> mp;
        for(u32 i=begin;i<end;i++)
        {
            u32 tri_idx=graph_partitioner.indexes[i];
            for(u32 k=0;k<3;k++)
            {
                u32 e_idx=tri_idx*3+k;
                u32 v_idx=idx[e_idx];
                if(mp.find(v_idx)==mp.end())
                {
                    mp[v_idx]=cluster.verts.size();
                    cluster.verts.push_back(pos[v_idx]);
                }
                bool is_external=false;
                for(auto[adj_edge,_]:adjacency_edge_graph.g[e_idx])
                {
                    u32 adj_triangle = graph_partitioner.sorted_to[adj_edge/3];
                    // 邻接的三角形在另外的cluster中，说明在cluster的外围
                    if(adj_triangle<begin||adj_triangle>=end)
                    {
                        is_external=true;
                        break;
                    }
                }
                vec3 p0=pos[v_idx],p1=pos[idx[cycle3(e_idx)]];
                if(!is_external)
                {
                    for(u32 j:edge_hash[hash_position(p0,p1)]){
                        // 如果是group边界上的点
                        auto [cluster_id,e_id]=cluster_group.external_edges[j];
                        auto& pos1=clusters[cluster_id].verts;
                        auto& idx1=clusters[cluster_id].indexes;
                        if(p0==pos1[idx1[e_id]]&&p1==pos1[idx1[cycle3(e_id)]])
                        {
                            is_external=true;
                            break;
                        }
                    }
                }
                if(is_external)
                {
                    cluster.external_edges.push_back(cluster.indexes.size());
                }
                cluster.indexes.push_back(mp[v_idx]);
            }
        }

        cluster.mip_level=cluster_group.mip_level+1;
        cluster.lod_error=parent_lod_error;
        cluster.sphere_bounds=Sphere::construct_from_points(cluster.verts.data(),cluster.verts.size());
        cluster.lod_bounds=parent_lod_bound;
        cluster.bounds=cluster.verts[0];
        for(vec3 p:cluster.verts) 
            cluster.bounds=cluster.bounds+p;
    }
    cluster_group.lod_bounds=parent_lod_bound;
    cluster_group.max_parent_lod_error=parent_lod_error;
}

void build_group_bounds(ClusterGroup& group,vector<Cluster>& clusters)
{
    for(auto& cluster_id:group.clusters)
    {
        group.bounds = group.bounds+clusters[cluster_id].bounds;
    }
}


std::shared_ptr<BVHNode> buid_bvh(u32 cluster_group_start,u32 cluster_group_end,vector<ClusterGroup>& cluster_groups,vector<Cluster>& clusters)
{

    std::vector<u32> cluster_group_index;
    for(u32 i=0;i<cluster_groups.size();i++)
    {
        cluster_group_index.push_back(i);
    }
    for(u32 i=cluster_group_start;i<cluster_group_end;i++)
    {
        build_group_bounds(cluster_groups[i],clusters);
    }

    std::shared_ptr<BVHNode> root = std::make_shared<BVHNode>();
    root->start = cluster_group_start;// 这一层 cluster_group开始的序号
    root->end = cluster_group_end;// 这一层 cluster_group结束的序号（不含）
    root->type=BVHNodeType::NODE;
   
    std::stack<std::shared_ptr<BVHNode>> bvh_stack;
    bvh_stack.push(root);

    while(!bvh_stack.empty())
    {
        auto& curr_node=bvh_stack.top();
        bvh_stack.pop();

        if(curr_node->type!=BVHNodeType::LEAF)
        {
            
            for(u32 i=curr_node->start;i<curr_node->end;i++)
            {
                auto& group=cluster_groups[i];
                curr_node->bounds = curr_node->bounds+group.bounds;
            }
            // TODO:cluster_group_index的按照AABB的结果应该映射到树上
            if(curr_node->end-curr_node->start>=4)// 划分
            {
                vec3 diff=curr_node->bounds.max-curr_node->bounds.min;
                u32 axis_longest = 0;
                if(diff[1]>diff[axis_longest]) axis_longest=1;
                if(diff[2]>diff[axis_longest]) axis_longest=2;
                std::sort(cluster_group_index.begin() + curr_node->start, cluster_group_index.begin() + curr_node->end, [&](u32 a, u32 b) {
				    return cluster_groups[a].bounds.min[axis_longest] < cluster_groups[b].bounds.min[axis_longest];
				});

                u32 mid = (curr_node->start + curr_node->end) / 2;
                u32 axis2 = (axis_longest+1)%3;
                u32 axis3 = (axis_longest+2)%3;
                u32 axis_second=diff[axis2]>diff[axis3]?axis2:axis3;
                std::sort(cluster_group_index.begin() + curr_node->start, cluster_group_index.begin() + mid, [&](u32 a, u32 b) 
                {
                    return cluster_groups[a].bounds.min[axis_second] < cluster_groups[b].bounds.min[axis_second];
				});
                std::sort(cluster_group_index.begin() + mid, cluster_group_index.begin() + curr_node->end, [&](u32 a, u32 b) {
                    return cluster_groups[a].bounds.min[axis_second] < cluster_groups[b].bounds.min[axis_second];
				});

                std::shared_ptr<BVHNode> node1= std::make_shared<BVHNode>();
                node1->start= curr_node->start;
                node1->end= curr_node->start+(mid-curr_node->start) / 2;
                node1->depth = curr_node->depth + 1;
                node1->type=BVHNodeType::NODE;

                std::shared_ptr<BVHNode> node2= std::make_shared<BVHNode>();
                node2->start=curr_node->start+(mid-curr_node->start) / 2;
                node2->end= mid;
                node2->depth = curr_node->depth + 1;
                node2->type=BVHNodeType::NODE;

                std::shared_ptr<BVHNode> node3= std::make_shared<BVHNode>();
                node3->start=mid;
                node3->end= mid+(curr_node->end-mid) / 2;
                node3->depth = curr_node->depth + 1;
                node3->type=BVHNodeType::NODE;

                std::shared_ptr<BVHNode> node4= std::make_shared<BVHNode>();
                node4->start= mid+(curr_node->end-mid) / 2;
                node4->end= curr_node->end;
                node4->depth = curr_node->depth + 1;
                node4->type=BVHNodeType::NODE;

                curr_node->children.push_back(node1);
                curr_node->children.push_back(node2);
                curr_node->children.push_back(node3);
                curr_node->children.push_back(node4);

                bvh_stack.push(node1);
                bvh_stack.push(node2);
                bvh_stack.push(node3);
                bvh_stack.push(node4);
            }
            else
            {
                // curr_node->end-curr_node->start<4
                curr_node->type=BVHNodeType::NODE;
                for (u32 i = curr_node->start; i < curr_node->end; ++i)
                {
                    std::shared_ptr<BVHNode> leaf_node= std::make_shared<BVHNode>();
                    leaf_node->type=BVHNodeType::LEAF;
                    leaf_node->start = i;
                    leaf_node->end = i + 1;
                    leaf_node->depth = curr_node->depth + 1;
                    curr_node->children.push_back(leaf_node);
                } 
                for (auto& child: curr_node->children)
                {
                    bvh_stack.push(child);
                }
            }
        }
        else
        {// 叶子节点 
            // auto& group = cluster_groups[curr_node->start];
            // curr_node->clusters=group.clusters;
            curr_node->group_id=curr_node->start;
            curr_node->bounds.min=cluster_groups[curr_node->start].bounds.min;
            curr_node->bounds.max=cluster_groups[curr_node->start].bounds.max;
        }
    }    
    return root;

}

void build_a_level(
    vector<Cluster>& clusters,
    u32 level_offset,
    u32 num_level_cluster,
    vector<ClusterGroup>& cluster_groups,
    u32 mip_level,
    std::shared_ptr<BVHNode>& root)
{
    u32 num_pre_group=cluster_groups.size();
    group_clusters(clusters,level_offset,num_level_cluster,cluster_groups,mip_level);
    for(u32 i=num_pre_group;i<cluster_groups.size();i++)
    {
        build_parent(cluster_groups[i],clusters);
    }
    // BVH
    root=buid_bvh(num_pre_group,cluster_groups.size(),cluster_groups,clusters);

}