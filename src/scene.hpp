#pragma once

#include <cstddef>
#include <vector>

namespace astana {

struct Vertex {
    float x, y, z;
    float nx, ny, nz;
    float r, g, b;
    float u, v;
    float mat, id;
};

struct Landmark {
    int id;
    const char* name;
    float x, y, z, radius;
};

struct CarSpot {
    const char* name;
    float x, z;
    int heading;
};

const std::vector<Landmark>& landmarks();
const std::vector<CarSpot>& carSpots();
std::vector<Vertex> buildScene(std::size_t* cityVertexCount=nullptr);

} // namespace astana
