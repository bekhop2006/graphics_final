#pragma once

#include "renderer.hpp"

namespace astana {
Camera overview();
Camera street(float progress=0);
Camera carView(int index, float orbit=0);
Camera carDetail();
Camera focus(int id);
Camera fly(Camera a, Camera b, float progress);
View tourFrame(float seconds);
}
