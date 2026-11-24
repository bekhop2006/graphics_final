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
// The moving car and then the drivable car follow the static mesh in car-local space.
// staticVertexCount and playerVertexStart mark where each of them starts.
std::vector<Vertex> buildScene(std::size_t* cityVertexCount=nullptr, std::size_t* staticVertexCount=nullptr,
                               std::size_t* playerVertexStart=nullptr);

} // namespace astana
