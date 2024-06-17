#pragma once

#include <vec_types.h>
#include <vec_math.h>

struct Bounds
{
    vec3 min;
    vec3 max;

    Bounds()
    {
        min={MAX_flt,MAX_flt,MAX_flt};
        max={-MAX_flt,-MAX_flt,-MAX_flt};
    }
    Bounds(vec3 p)
    {
        min=p;
        max=p;
    }


    Bounds operator+(const Bounds& other);
    Bounds operator+(const vec3& other);

};

struct  Sphere
{
    vec3 center;
    f32 radius;
    static Sphere construct_from_points(vec3* position,u32 size);
    static Sphere construct_from_sphere_bounds(Sphere* spheres,u32 size);

    Sphere operator+(Sphere& other);
};
