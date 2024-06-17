#pragma once
#include <vec_types.h>
#include <vector>
#include <string>
#include <pipeline/vertex_input.h>
#include <vector>

struct Mesh{
    std::vector<vec3> positions;
    std::vector<u32> indices;

    void load(std::string path);

};