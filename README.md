## my_nanite
基于 C++ 和 Vulkan 实现的化简版nanite

- CPU:
    - Mesh化简
    - 将三角面片划分为Cluster
    - 将Cluster分为Group，同时建立Group之间的父子关系（交由Compute Shader统一处理）
- GPU:
    - compute shader实时计算可见的cluster进行渲染，达到实时LOD

## Result
LOD测试
dragon:
verts: 435545, triangles: 871306

三角面片:
![](./pics/tri.PNG)


cluster:
![](./pics/cluster.PNG)

group:
![](./pics/group.PNG)

LOD:
![](./pics/lod.gif)

avg_fps:1603.765381



## Ref

UE5.0源码

GAMES104（https://games104.boomingtech.com/sc/）

https://github.com/liameitimie/learn-nanite