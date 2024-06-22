## my_nanite
基于 C++ 和 Vulkan 实现的化简版nanite

- CPU:
    - [x] Mesh化简
    - [x] 将三角面片划分为Cluster
    - [x] 将Cluster分为Group，同时建立Group之间的父子关系（交由Compute Shader统一处理）
    - 构建BVH结构
    - 并行化
- GPU:
    - [x] compute shader实时计算可见的cluster进行渲染，达到实时LOD
    - Instance Cull: BVH
    - [x] Cluster Cull: 视锥剔除和HZB

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

### Instance Cull:BVH
Doing.....

### Cluster Cull:视锥剔除+HZB遮挡剔除

LOD & Cluster Culling:
![](./pics/culling.gif)

avg_fps:1640.190918


## Ref

UE5.0源码

GAMES104（https://games104.boomingtech.com/sc/）

https://github.com/liameitimie/learn-nanite
