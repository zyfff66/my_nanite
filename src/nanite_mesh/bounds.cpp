#pragma once

#include "bounds.h"
#include <algorithm>
#include <vec_math.h>
#include <assert.h>

Bounds Bounds::operator+(const Bounds& other){
    Bounds bounds;
    bounds.min={
        .x=std::min(min.x,other.min.x),
        .y=std::min(min.y,other.min.y),
        .z=std::min(min.z,other.min.z),
    };
    bounds.max={
        .x=std::max(max.x,other.max.x),
        .y=std::max(max.y,other.max.y),
        .z=std::max(max.z,other.max.z),
    };
    return bounds;
}

Bounds Bounds::operator+(const vec3& other){
    Bounds bounds;
    bounds.min={
        .x=std::min(min.x,other.x),
        .y=std::min(min.y,other.y),
        .z=std::min(min.z,other.z),
    };
    bounds.max={
        .x=std::max(max.x,other.x),
        .y=std::max(max.y,other.y),
        .z=std::max(max.z,other.z),
    };
    return bounds;
}

inline f32 sqr(f32 x){return x*x;}

Sphere Sphere::operator+(Sphere& other)
{
    vec3 t=other.center-center;
    f32 tlen2=length2(t);
    if(sqr(radius-other.radius)>=tlen2){
        return radius<other.radius?other:*this;
    }
    Sphere sphere;
    f32 tlen=sqrt(tlen2);
    sphere.radius=(tlen+radius+other.radius)*0.5;
    sphere.center=center+t*((sphere.radius-radius)/tlen);
    return sphere;
}

Sphere Sphere::construct_from_points(vec3* position,u32 size)
{
    u32 min_idx[3]={};
    u32 max_idx[3]={};
    for(u32 i=0;i<size;i++)
    {
        for(int k=0;k<3;k++)
        {
            if(position[i][k]<position[min_idx[k]][k]) min_idx[k]=i;
            if(position[i][k]>position[max_idx[k]][k]) max_idx[k]=i;
        }
    }
    f32 max_len=0;
    u32 max_axis=0;
    for(u32 k=0;k<3;k++)
    {
        vec3 point_min=position[min_idx[k]];
        vec3 point_max=position[max_idx[k]];
        f32 tlen=length2(point_max-point_min);
        if(tlen>max_len) 
        {
            max_len=tlen;
            max_axis=k;
        }
    }
    vec3 point_min=position[min_idx[max_axis]];
    vec3 point_max=position[max_idx[max_axis]];

    Sphere sphere;
    sphere.center=(point_min+point_max)*0.5f;
    sphere.radius=f32(0.5*sqrt(max_len));
    max_len=sphere.radius*sphere.radius;

    for(u32 i=0;i<size;i++)
    {
        f32 len=length2(position[i]-sphere.center);
        if(len>max_len)
        {
            len=sqrt(len);
            f32 t=0.5-0.5*(sphere.radius/len);
            sphere.center=sphere.center+(position[i]-sphere.center)*t;
            sphere.radius=(sphere.radius+len)*0.5;
            max_len=sphere.radius*sphere.radius;
        }
    }
    for(u32 i=0;i<size;i++)
    {
        f32 len=length(position[i]-sphere.center);
        assert(len-1e-6<=sphere.radius);
    }
    return sphere;
}

Sphere Sphere::construct_from_sphere_bounds(Sphere* spheres,u32 size)
{
    u32 min_idx[3]={};
    u32 max_idx[3]={};
    for(u32 i=0;i<size;i++)
    {
        for(u32 k=0;k<3;k++)
        {
            if(spheres[i].center[k]-spheres[i].radius < spheres[min_idx[k]].center[k]-spheres[min_idx[k]].radius)
                min_idx[k]=i;
            if(spheres[i].center[k]+spheres[i].radius < spheres[max_idx[k]].center[k]+spheres[max_idx[k]].radius)
                max_idx[k]=i;
        }
    }

    f32 max_len=0;
    u32 max_axis=0;
    for(u32 k=0;k<3;k++)
    {
        Sphere spmin=spheres[min_idx[k]];
        Sphere spmax=spheres[max_idx[k]];
        f32 tlen=length(spmax.center-spmin.center)+spmax.radius+spmin.radius;
        if(tlen>max_len) 
        {
            max_len=tlen;
            max_axis=k;
        }
    }
    Sphere sphere=spheres[min_idx[max_axis]];
    sphere=sphere+spheres[max_idx[max_axis]];
    for(u32 i=0;i<size;i++)
    {
        sphere=sphere+spheres[i];
    }
    for(u32 i=0;i<size;i++)
    {
        f32 t1=sqr(sphere.radius-spheres[i].radius);
        f32 t2=length2(sphere.center-spheres[i].center);
        assert(t1+1e-6>=t2);
    }
    return sphere;

}
