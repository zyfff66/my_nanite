#pragma once

#include "types.h"
#include "vec_types.h"
#include "hash_table.h"
#include <vector>
#include <algorithm>
#include <vec_math.h>
#include <memory>
#include <assert.h>

using namespace std;

inline u32 hash_position(vec3 v)
{
    union {f32 f;u32 u;} x,y,z;
    x.f=(v.x==0.f?0:v.x);
    y.f=(v.y==0.f?0:v.y);
    z.f=(v.z==0.f?0:v.z);
    return murmur_finalize32(murmur32(murmur32(x.u,y.u),z.u));
}

inline u32 hash_position(vec3 u, vec3 v)
{
    u32 h0=hash_position(u);
    u32 h1=hash_position(v);
    return murmur_finalize32(murmur32(h0,h1));
}

inline u32 cycle3(u32 i)
{
    u32 imod3=i%3;
    return i-imod3+((1<<imod3)&3);
}
inline u32 cycle3(u32 i,u32 ofs)
{
    return i-i%3+(i+ofs)%3;
}

struct Quadric
{
    f64 a2,b2,c2,d2;
    f64 ab,ac,ad;
    f64 bc,bd,cd;

    Quadric()
    {
        memset(this,0,sizeof(f64)*10);
    }

    Quadric(dvec3 p0,dvec3 p1,dvec3 p2)
    {
        dvec3 n=normalize(cross(p1-p0,p2-p0));
        auto [a,b,c]=n;
        f64 d=-dot(n,p0);
        a2=a*a;b2=b*b;c2=c*c;d2=d*d;
        ab=a*b;ac=a*c;ad=a*d;
        bc=b*c;bd=b*d;cd=c*d;
    }

    void add(Quadric other)
    {
        f64* t1=(f64*)this;
        f64* t2=(f64*)&other;
        for(u32 i=0;i<10;i++)
            t1[i]+=t2[i];
    }

    // 求解方程找到新点的位置
    bool get(vec3& p){
        dmat4 m,inv;
        m.set_col(0,{a2,ab,ac,0});
        m.set_col(1,{ab,b2,bc,0});
        m.set_col(2,{ac,bc,c2,0});
        m.set_col(3,{ad,bd,cd,1});
        if(!invert(m,inv)) return false;
        dvec4 v=inv.col[3];
        p={(f32)v.x,(f32)v.y,(f32)v.z};
        return true;
    }

    // 返回p距离这个三角面的QEM
    f32 evaluate(vec3 p){
        f32 res=a2*p.x*p.x+2*ab*p.x*p.y+2*ac*p.x*p.z+2*ad*p.x
            +b2*p.y*p.y+2*bc*p.y*p.z+2*bd*p.y
            +c2*p.z*p.z+2*cd*p.z+d2;
        return res<=0.f?0.f:res;
    }
};

class BitArray
{
    u32* bits;
public:
    BitArray()
    {
        bits=nullptr;
    }
    BitArray(u32 size)
    {
        bits=new u32[(size+31)/32];
        memset(bits,0,(size+31)/32*sizeof(u32));
    }
    ~BitArray(){free();}

    void resize(u32 size)
    {
        free();
        bits=new u32[(size+31)/32];
        memset(bits,0,(size+31)/32*sizeof(u32));
    }
    void free()
    {
        if(bits) 
        {
            delete[] bits;
            bits=nullptr;
        }
    }
    void set_false(u32 idx){
        u32 x=idx>>5;
        u32 y=idx&31;
        bits[x]&=~(1<<y);
    }
    void set_true(u32 idx){
        u32 x=idx>>5;
        u32 y=idx&31;
        bits[x]|=(1<<y);
    }
    bool operator[](u32 idx){
        u32 x=idx>>5;
        u32 y=idx&31;
        return (bool)(bits[x]>>y&1);
    }
};

class Heap
{
    u32 heap_size;
    u32 num_index;
    u32* heap;
    f32* keys;
    u32* heap_indexes;

    void push_up(u32 i);
    void push_down(u32 i);
public:
    Heap();
    Heap(u32 num_index);
    ~Heap()
    {
        free();
    }

    void free()
    {
        heap_size=0,num_index=0;
        delete[] heap;
        delete[] keys;
        delete[] heap_indexes;
        heap=nullptr,keys=nullptr,heap_indexes=nullptr;
    }

    void resize(u32 _num_index);
    
    f32 get_key(u32 idx);
    void clear();
    bool empty(){return heap_size==0;}
    bool is_present(u32 idx){return heap_indexes[idx]!=~0u;}
    u32 top();
    void pop();
    void add(f32 key,u32 idx);
    void update(f32 key,u32 idx);
    void remove(u32 idx);
};

class MeshSimplifier
{
public:
    i32 degree_limit = 24;
    f32 degree_penalty = 0.5f;

    u32 num_verts;
    u32 num_indexs;
    u32 num_tris;

    u32 remaining_num_verts;
    u32 remaining_num_tris;

    vec3* verts;
    u32* indexes;

    HashTable vert_hash; //顶点hash->verts中的序号
    HashTable corner_hash;// indexes顺序的点的hash->indexes中的corner序号

    vector<u32> vert_ref_counts;
    vector<u8> corner_flags;
    BitArray tri_removed;

    vector<pair<vec3,vec3>> pairs;
    HashTable pair_hash0;// p0计算的hash值->在pairs中，以p0开头的边
    HashTable pair_hash1;// p1计算的hash值->在pairs中，以p1结尾的边
    Heap heap;

    vector<u32> move_verts;
    vector<u32> move_corners;
    vector<u32> move_pairs;
    vector<u32> reevaluate_pairs;

    vector<Quadric> tri_quadrics;
    f32 max_error;

    enum corner_flags
    {
        AdjMask=1,
        LockMask=2
    };

    MeshSimplifier(vec3* verts,u32 num_vert,u32* indexes,u32 num_index);
    ~MeshSimplifier(){}

    void simplify(u32 target_num_tri);
    void fixup_tri(u32 tri_index);
    void remove_duplicate_verts(u32 corner);
    void set_vert_index(u32 corner,u32 new_vert_index);
    bool is_duplicate_tri(u32 tri_index);
    void compact();
    float evaluate_merge(vec3 p0,vec3 p1,bool b_move_verts);
    void gather_adj_tris(vec3 p,vector<u32>& tris,bool& lock);
    void begin_move(vec3 p);
    void end_move();
    bool add_unique_pair(vec3& p0,vec3& p1,u32 index);

    void lock_position(vec3 p);

};