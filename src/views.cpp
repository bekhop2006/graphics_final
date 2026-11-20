#include "views.hpp"
#include <stdexcept>

namespace astana {
namespace {
Camera carPortrait(Vec3 closeEye,Vec3 target) {
    // Step back and frame the same car with a longer lens to keep its ends in proportion.
    return {target+(closeEye-target)*1.39f,target,33.f*pi/180.f};
}
}
Camera overview() {
    return {{-98,50,112},{3,7,0}};
}
Camera street(float progress) {
    return {{-52+12*progress,3.1f,2.5f},{0,15.8f,0}};
}
Camera carView(int index, float orbit) {
    const CarSpot& car=carSpots().at(index);
    struct Angle { float yaw, distance, eyeHeight, targetHeight; };
    static const Angle angles[]={
        {.94f,2.69f,.91f,.60f}, {.69f,3.12f,1.10f,.65f},
        {.80f,2.79f,.97f,.57f}, {.77f,2.52f,.88f,.52f}
    };
    const Angle& shot=angles[index];
    const float yaw=shot.yaw+orbit;
    // View the street from the outer curb, with the boulevard behind the cars.
    return carPortrait({car.x+car.heading*shot.distance*std::sin(yaw),shot.eyeHeight,car.z+shot.distance*std::cos(yaw)},
                       {car.x,shot.targetHeight,car.z});
}
Camera carDetail() {
    const CarSpot& car=carSpots().at(0);
    return carPortrait({car.x-1.85f*car.heading,.66f,car.z+1.16f},
                       {car.x-.75f*car.heading,.42f,car.z+.16f});
}
Camera focus(int id) {
    for(const Landmark& p:landmarks()) if(p.id==id) {
        Vec3 offset;
        switch(id) {
            case 1: offset={-1.34f,.36f,2.40f}; break;
            case 2: offset={-1.45f,.75f,2.05f}; break;
            case 3: offset={1.23f,.73f,2.10f}; break;
            case 4: offset={1.32f,.75f,2.29f}; break;
            case 5: offset={-2.43f,.64f,.63f}; break;
            case 6: offset={-1.46f,.78f,2.62f}; break;
            default: offset={-.76f,1.2f,2.54f}; break;
        }
        const Vec3 center={p.x,p.y+(id==6?1.2f:0.0f),p.z};
        return {center+offset*p.radius,center};
    }
    throw std::runtime_error("Unknown landmark");
}
Camera fly(Camera a,Camera b,float progress) {
    const float t=smooth(progress);
    Camera c=blend(a,b,t);
    // Lift the eye above intervening roofs during long flights.
    c.eye.y+=std::sin(t*pi)*std::min(18.f,length(a.eye-b.eye)*.15f);
    return c;
}
View tourFrame(float t) {
    View s; s.time=t; s.cam=overview();
    if(t<6) {
        if(t<2.3f) s.daylight=smooth((t-.35f)/1.6f);
        else s.daylight=1-smooth((t-3.4f)/1.65f);
    } else if(t<27) {
        const int segment=std::min(6,int((t-6)/3));
        const float elapsed=t-6-segment*3;
        s.cam=fly(segment==0?overview():focus(segment),focus(segment+1),elapsed/1.35f);
    } else if(t<41) {
        s.cam=fly(focus(7),focus(1),(t-27)/1.35f);
        s.wire=t>=32 && t<34.7f;
        s.ssaa=!(t>=36 && t<38.5f);
    } else if(t<44) {
        const float walk=clamp((t-42.6f)/1.4f,0,1);
        s.cam=fly(focus(1),street(walk),(t-41)/1.6f);
    } else {
        const float orbit=.27f*smooth((t-45.6f)/2.4f);
        s.cam=fly(street(1),carView(0,orbit),(t-44)/1.6f);
    }
    return s;
}
} // namespace astana
