#include<mesh_simplify.h>

Heap::Heap()
{
    heap=nullptr,keys=nullptr,heap_indexes=nullptr;
    heap_size=0,num_index=0;
}

Heap::Heap(u32 _num_index)
{
    heap_size=0;
    num_index=_num_index;
    heap=new u32[num_index];
    keys=new f32[num_index];
    heap_indexes=new u32[num_index];
    memset(heap_indexes,0xff,num_index*sizeof(u32));
}

void Heap::resize(u32 _num_index){
    free();
    heap_size=0;
    num_index=_num_index;
    heap=new u32[num_index];
    keys=new f32[num_index];
    heap_indexes=new u32[num_index];
    memset(heap_indexes,0xff,num_index*sizeof(u32));
}
void Heap::push_up(u32 i){
    u32 idx=heap[i];
    u32 fa=(i-1)>>1;
    while(i>0&&keys[idx]<keys[heap[fa]])
    {
        heap[i]=heap[fa];
        heap_indexes[heap[i]]=i;
        i=fa,fa=(i-1)>>1;
    }
    heap[i]=idx;
    heap_indexes[heap[i]]=i;
}

void Heap::push_down(u32 i){
    u32 idx=heap[i];
    u32 ls=(i<<1)+1;
    u32 rs=ls+1;
    while(ls<heap_size){
        u32 t=ls;
        if(rs<heap_size&&keys[heap[rs]]<keys[heap[ls]])
            t=rs;
        if(keys[heap[t]]<keys[idx])
        {
            heap[i]=heap[t];
            heap_indexes[heap[i]]=i;
            i=t,ls=(i<<1)+1,rs=ls+1;
        }
        else break;
    }
    heap[i]=idx;
    heap_indexes[heap[i]]=i;
}

void Heap::clear(){
    heap_size=0;
    memset(heap_indexes,0xff,num_index*sizeof(u32));
}

u32 Heap::top(){
    assert(heap_size>0);
    return heap[0];
}

void Heap::pop(){
    assert(heap_size>0);

    u32 idx=heap[0];
    heap[0]=heap[--heap_size];
    heap_indexes[heap[0]]=0;
    heap_indexes[idx]=~0u;
    push_down(0);
}

void Heap::add(f32 key,u32 idx){
    assert(!is_present(idx));

    u32 i=heap_size++;
    heap[i]=idx;
    keys[idx]=key;
    heap_indexes[idx]=i;
    push_up(i);
}

void Heap::update(f32 key,u32 idx){
    keys[idx]=key;
    u32 i=heap_indexes[idx];
    if(i>0&&key<keys[heap[(i-1)>>1]]) push_up(i);
    else push_down(i);
}

void Heap::remove(u32 idx){
    //if(!is_present(idx)) return;
    assert(is_present(idx));

    f32 key=keys[idx];
    u32 i=heap_indexes[idx];

    if(i==heap_size-1){
        --heap_size;
        heap_indexes[idx]=~0u;
        return;
    }

    heap[i]=heap[--heap_size];
    heap_indexes[heap[i]]=i;
    heap_indexes[idx]=~0u;
    if(key<keys[heap[i]]) push_down(i);
    else push_up(i);
}

f32 Heap::get_key(u32 idx){
    return keys[idx];
}

MeshSimplifier::MeshSimplifier(vec3* vert,u32 num_vert,u32* index,u32 num_index):
    num_verts(num_vert)
    ,num_indexs(num_index)
    ,num_tris(num_index/3)
    ,verts(vert),indexes(index)
    ,vert_hash(num_vert),vert_ref_counts(num_vert),corner_hash(num_index)
    ,tri_removed(num_tris),corner_flags(num_index),remaining_num_tris(num_tris)
    ,remaining_num_verts(num_verts)
{
    for(u32 i=0;i<num_verts;i++)
    {
        vert_hash.add(hash_position(verts[i]),i);
    }
    u32 num_edges=min(min(num_index,3*num_vert-6),num_tris+num_vert);
    pairs.reserve(num_edges);
    pair_hash0.resize(num_edges);
    pair_hash1.resize(num_edges);

    for(u32 corner=0;corner<num_index;corner++)
    {
        u32 v_index=indexes[corner];
        vert_ref_counts[v_index]++;
        const vec3& p=verts[v_index];
        corner_hash.add(hash_position(p),corner);

        vec3 p0=p;
        vec3 p1=verts[indexes[cycle3(corner)]];
        if(add_unique_pair(p0,p1,pairs.size()))
        {
            pairs.push_back({p0,p1});
        }
    }
}
/**
 * @brief 按照remaining_num_verts的数量更新verts、vert_ref_counts；按照remaining_num_tris的数量更新indexes
 * 
 */
void MeshSimplifier::compact()
{
    u32 v_cnt=0;
    for(u32 i=0;i<num_verts;i++)
    {
        if(vert_ref_counts[i]>0)
        {
            if(i!=v_cnt) verts[v_cnt]=verts[i];
            vert_ref_counts[i]=v_cnt++;
        }
    }    
    assert(v_cnt==remaining_num_verts);

    u32 t_cnt=0;
    for(u32 i=0;i<num_tris;i++)
    {
        if(!tri_removed[i])
        {
            for(u32 k=0;k<3;k++)
            {
                indexes[t_cnt*3+k]=vert_ref_counts[indexes[i*3+k]];
            }
            t_cnt++;
        }
    }
    assert(t_cnt==remaining_num_tris);
}

bool MeshSimplifier::is_duplicate_tri(u32 tri_index)
{
    u32 i0=indexes[tri_index*3],i1=indexes[tri_index*3+1],i2=indexes[tri_index*3+2];
    for(u32 i:corner_hash[hash_position(verts[i0])])
    {
        if(i!=tri_index*3)
        {
            if(i0==indexes[i]&&i1==indexes[cycle3(i)]&&i2==indexes[cycle3(i,2)])
                return true;
        }
    }
    return false;
}

void MeshSimplifier::set_vert_index(u32 corner,u32 new_vert_index)
{
    u32& v_index=indexes[corner];
    assert(v_index!=~0u);
    assert(vert_ref_counts[v_index]>0);

    if(v_index==new_vert_index)
        return;

    // 更新v_index的ref计数、hash表和remaining_num_verts
    if(--vert_ref_counts[v_index]==0)
    {
        vert_hash.remove(hash_position(verts[v_index]),v_index);
        remaining_num_verts--;
    }

    v_index=new_vert_index;
    if(v_index!=~0u)
        vert_ref_counts[v_index]++;
}

// 类似并交集的思路，相同位置的点记录在一个点上
void MeshSimplifier::remove_duplicate_verts(u32 corner)
{
    u32 v_index=indexes[corner];
    vec3& v=verts[v_index];
    for(u32 i:vert_hash[hash_position(v)])
    {
        if(i==v_index)
            break;
        if(v==verts[i])
        {
            set_vert_index(corner,i);
            break;
        }
    }
}

/**
 * @brief 去除重复顶点；当三角形重复时，去除三角形；三角形不重复时，计算三角形的Q
 * 
 * @param tri_index 三角形下标
 */
void MeshSimplifier::fixup_tri(u32 tri_index)
{
    assert(!tri_removed[tri_index]);

    const vec3& p0=verts[indexes[tri_index*3+0]];
    const vec3& p1=verts[indexes[tri_index*3+1]];
    const vec3& p2=verts[indexes[tri_index*3+2]];

    bool b_remove_tri=false;

    if(!b_remove_tri)
    {
        b_remove_tri=(p0==p1)||(p1==p2)||(p2==p0);
    }
    if(!b_remove_tri)
    {
        for(u32 k=0;k<3;k++)
        {
            remove_duplicate_verts(tri_index*3+k);
        }
        b_remove_tri = is_duplicate_tri(tri_index);
    }

    if(b_remove_tri)
    {
        tri_removed.set_true(tri_index);
        remaining_num_tris--;
        for(u32 k=0;k<3;k++)
        {
            u32 corner=tri_index*3+k;
            u32 v_index=indexes[corner];
            corner_hash.remove(hash_position(verts[v_index]),corner);
            set_vert_index(corner,~0u);
        }
    }
    else
    {
        tri_quadrics[tri_index]=Quadric(p0,p1,p2);
    }
}

void MeshSimplifier::simplify(u32 target_num_tri)
{
    tri_quadrics.resize(num_tris);
    for(u32 i=0;i<num_tris;i++)
        fixup_tri(i);
    
    if(remaining_num_tris<=target_num_tri)
    {
        compact();
        return;
    }

    heap.resize(pairs.size());
    u32 i=0;
    for(auto& pair:pairs)
    {
        f32 error=evaluate_merge(pair.first,pair.second,false);
        heap.add(error,i);
        i++;
    }

    max_error=0;
    while(!heap.empty())
    {
        u32 e_index=heap.top();
        if(heap.get_key(e_index)>=1e6)break;
        heap.pop();

        auto& e=pairs[e_index];
        pair_hash0.remove(hash_position(e.first),e_index);
        pair_hash1.remove(hash_position(e.second),e_index);

        f32 error=evaluate_merge(e.first,e.second,true);
        if(error>max_error) max_error=error;

        if(remaining_num_tris<=target_num_tri) break;

        for(u32 i:reevaluate_pairs)
        {
            auto& e=pairs[i];
            f32 error=evaluate_merge(e.first,e.second,false);
            heap.add(error,i);
        }
        reevaluate_pairs.clear();
    }
    compact();
}
/**
 * @brief 获得过p的三角形并且其中一个三角形是lock的置lock为true
 * 
 * @param p 
 * @param tris 过p的AdjMask flag为0的三角面片，添加完毕后改变flag
 * @param lock 其中一个三角形是lock的置lock为true
 */
void MeshSimplifier::gather_adj_tris(vec3 p,vector<u32>& tris,bool& lock)
{
    for(u32 i:corner_hash[hash_position(p)])
    {
        if(p==verts[indexes[i]])
        {
            u32 tri_index=i/3;
            if((corner_flags[tri_index*3]&AdjMask)==0)
            {
                corner_flags[tri_index*3]|=AdjMask;//更改三角形的adjmask位
                tris.push_back(tri_index);
            }
            if(corner_flags[i]&LockMask)//判断三角形的LockMasj位是不是被锁的
            {
                lock = true;
            }
        }
    }
}

/**
 * @brief b_move_verts==true:进行边的合并；false时只计算边的error。
 * 
 * @param p0 
 * @param p1 
 * @param b_move_verts 
 * @return float error
 */
float MeshSimplifier::evaluate_merge(vec3 p0,vec3 p1,bool b_move_verts)
{
    if(p0==p1) return 0.f;
    f32 error=0;
    vector<u32> adj_tris;

    bool lock_flag0=false,lock_flag1=false;
    gather_adj_tris(p0,adj_tris,lock_flag0);
    gather_adj_tris(p1,adj_tris,lock_flag1);

    if(adj_tris.size()==0) return 0.f;
    if(adj_tris.size()>degree_limit)
    {
        error+=degree_penalty*(adj_tris.size()-degree_limit);
    }

    Quadric q;
    for(u32 i:adj_tris)
    {
        q.add(tri_quadrics[i]);// p0 p1周围所有面的QEM
    }

    auto is_valid_pos=[&](vec3 p)->bool{
        if(length(p-p0)+length(p-p1)>2*length(p0-p1))
            return false;
        return true;
    };

    // 合并两点后新点的位置
    vec3 new_position = (p0+p1)*0.5f;
    if(lock_flag0&&lock_flag1)
    {
        error+=1e8;
    }
    if(lock_flag0&&!lock_flag1)
    {
        new_position=p0;
    }
    else if(!lock_flag0&&lock_flag1)
    {
        new_position=p1;
    }
    // else if(!q.get(new_position)) 
    // {
    //     new_position=(p0+p1)*0.5f;
    // }
    else
    {
        // 寻找新点
        if(!q.get(new_position))
        {
            new_position=(p0+p1)*0.5f;
        }
        // bool b_is_valid=q.get(new_position);
        // if(!b_is_valid)
        //     new_position=(p0+p1)*0.5f;
    }
    if(!is_valid_pos(new_position))
        error+=100.0f;
    //  if(!is_valid_pos(new_position))
    //  {
    //     new_position=(p0+p1)*0.5f;
    // }
    error+=q.evaluate(new_position);

    if(b_move_verts)
    {
        begin_move(p0);
        begin_move(p1);
        // 修改与p0p1相连三角形的顶点
        for(u32 i:adj_tris)
        {
            for(u32 k=0;k<3;k++)
            {
                u32 corner=i*3+k;
                vec3& pos=verts[indexes[corner]];
                if(pos==p0||pos==p1)
                {
                    pos=new_position;
                    if(lock_flag0||lock_flag1)
                        corner_flags[corner]|=LockMask;
                }
            }
        }
        // 修改与p0p1相连边的端点
        for(u32 i:move_pairs)
        {
            auto& e=pairs[i];
            if(e.first==p0||e.first==p1)
            {
                e.first=new_position;
            }
            if(e.second==p0||e.second==p1)
            {
                e.second=new_position;
            }
        }
        end_move();
        // 去重
        vector<u32> adj_verts;
        for(u32 i:adj_tris)
        {
            for(u32 k=0;k<3;k++)
            {
                adj_verts.push_back(indexes[i*3+k]);
            }
        }
        sort(adj_verts.begin(),adj_verts.end());
        adj_verts.erase(unique(adj_verts.begin(),adj_verts.end()),adj_verts.end());

        // 重新评估相邻三角形的pair
        for(u32 v_index:adj_verts)
        {
            u32 h=hash_position(verts[v_index]);
            for(u32 i:pair_hash0[h])
            {
                if(pairs[i].first==verts[v_index])
                {
                    if(heap.is_present(i))
                    {
                        heap.remove(i);
                        reevaluate_pairs.push_back(i);
                    }
                }
            }
            for(u32 i:pair_hash1[h])
            {
                if(pairs[i].second==verts[v_index])
                {
                    if(heap.is_present(i))
                    {
                        heap.remove(i);
                        reevaluate_pairs.push_back(i);
                    }
                }
            }
        }
        for(u32 i:adj_tris)
        {
            fixup_tri(i);
        }
    }


    for(u32 i:adj_tris)
    {
        for(u32 k=0;k<3;k++)
        {
            corner_flags[i*3+k]&=(~AdjMask);
        }
    }
    return error;
}

/**
 * @brief 移除vert_hash、corner_hash、pair_hash0、pair_hash1和点p相关的顶点、corner、pair到move_verts、move_corners、move_pairs中
 * 
 * @param p 要移除的点
 */
void MeshSimplifier::begin_move(vec3 p){
    u32 h=hash_position(p);
    for(u32 i:vert_hash[h])
    {
        if(verts[i]==p)
        {
            vert_hash.remove(h,i);
            move_verts.push_back(i);
        }
    }
    for(u32 i:corner_hash[h])
    {
        if(verts[indexes[i]]==p)
        {
            corner_hash.remove(h,i);
            move_corners.push_back(i);
        }
    }
    for(u32 i:pair_hash0[h])
    {
        if(pairs[i].first==p)
        {
            pair_hash0.remove(hash_position(pairs[i].first),i);
            pair_hash1.remove(hash_position(pairs[i].second),i);
            move_pairs.push_back(i);
        }
    }
    for(u32 i:pair_hash1[h])
    {
        if(pairs[i].second==p)
        {
            pair_hash0.remove(hash_position(pairs[i].first),i);
            pair_hash1.remove(hash_position(pairs[i].second),i);
            move_pairs.push_back(i);
        }
    }

}
/**
 * @brief 将更新的相关放入vert_hash corner_hash pairs
 * 
 */
void MeshSimplifier::end_move(){
    for(u32 i:move_verts)
    {
        vert_hash.add(hash_position(verts[i]),i);
    }
    for(u32 i:move_corners)
    {
        corner_hash.add(hash_position(verts[indexes[i]]),i);
    }
    for(u32 i:move_pairs)
    {
        auto& e=pairs[i];
        if(e.first==e.second||!add_unique_pair(e.first,e.second,i))
        {
            heap.remove(i);
        }
    }
    move_verts.clear();
    move_corners.clear();
    move_pairs.clear();
}
/**
 * @brief 添加边到pair_hash0和pair_hash1中
 * 
 * @param p0 
 * @param p1 
 * @param index 边序号
 * @return true 添加成功
 * @return false 添加失败
 */
bool MeshSimplifier::add_unique_pair(vec3& p0,vec3& p1,u32 index)
{
    u32 h0=hash_position(p0),h1=hash_position(p1);
    if(h0>h1)
    {
        swap(h0,h1);
        swap(p0,p1);
    }
    for(u32 i:pair_hash0[h0])
    {
        auto& e=pairs[i];
        if(e.first==p0&&e.second==p1) 
            return false;
    };
    pair_hash0.add(h0,index);
    pair_hash1.add(h1,index);
    return true;
}

void MeshSimplifier::lock_position(vec3 p)
{
     for(u32 i:corner_hash[hash_position(p)])
     {
        if(verts[indexes[i]]==p)
        {
            corner_flags[i]|=LockMask;
        }
    }
}
