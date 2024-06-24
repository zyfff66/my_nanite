## my_nanite
基于 C++ 和 Vulkan 实现的化简版nanite

- CPU:
    - [x] Mesh化简
    - [x] 将三角面片划分为Cluster
    - [x] 将Cluster分为Group，同时建立Group之间的父子关系（交由Compute Shader统一处理）
    - [x] 构建BVH结构
    - 并行化
- GPU:
    - [x] compute shader实时计算可见的cluster进行渲染，达到实时LOD
    - Cluster Cull: 视锥剔除和HZB 
        - [x] 简化版完成：管道可以跑通，视锥剔除完成，HZB的two pass需修改
    - Instance Cull: BVH
   

- Rasterization：
    - 硬件光栅化和软件光栅化
    - 基于Visibility Buffer的渲染

## Result
dragon:
verts: 435545, triangles: 871306
### LOD测试

三角面片:
![](./pics/tri.PNG)


cluster:
![](./pics/cluster.PNG)

group:
![](./pics/group.PNG)

LOD:

![](./pics/lod.gif)

avg_fps:1882.489136

## TODO
### Instance Cull:BVH
- [x] BVH的构建
- 着色器思路：
    - 做简单的基于BVH的视锥剔除，将可见的group_id和instance_id存入indirect_bvh_buffer
    - Cluster cull获取可见的group中的cluster做每一个cluster的视锥剔除和遮挡剔除，可见的cluster id写入indirect_buffer
    - 渲染管线draw_indirect


### Cluster Cull:视锥剔除+HZB遮挡剔除
Cluster Cull: 视锥剔除和HZB
- [x] 视锥剔除
- HZB没有做two pass，直接用的上一帧投影到本帧的结果。
    - two pass：离相机比较近的300个cluster用上一帧的zbuffer做一次筛选形成新的z-buffer，然后再把被剔除掉的用这个z-buffer做一次。
    - 后期可改为UE的思路，把可能的cluster投到上一帧做一次筛选形成一次z-buffer，再保守的将剔除的再用新的z-buffer在当前帧测试一次。

目前能跑通，但是细节上可能还有问题：

LOD & Cluster Culling:

![](./pics/culling.gif)

avg_fps:1640.190918


## Ref

UE5.0源码

GAMES104：https://games104.boomingtech.com/sc/

Vulkan主要参考：https://github.com/liameitimie/learn-nanite
