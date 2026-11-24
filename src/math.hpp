#pragma once

#include <algorithm>
#include <cmath>

namespace astana {
constexpr float pi = 3.141592654f;

struct Vec3 {
    float x, y, z;
};
inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
inline Vec3 operator*(Vec3 a, float s) { return {a.x*s, a.y*s, a.z*s}; }
inline float dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};
}
inline float length(Vec3 a) { return std::sqrt(dot(a,a)); }
inline Vec3 unit(Vec3 a) { return a * (1.0f / std::max(0.0001f,length(a))); }
inline float clamp(float a, float lo, float hi) { return std::max(lo,std::min(hi,a)); }
inline float smooth(float t) { t=clamp(t,0,1); return t*t*(3-2*t); }
inline Vec3 blend(Vec3 a, Vec3 b, float t) { return a*(1-t)+b*t; }

struct Camera {
    Vec3 eye, target;
    float verticalFov;
    Camera(Vec3 position={},Vec3 focus={},float fov=47.5f*pi/180.f)
        : eye(position),target(focus),verticalFov(fov) {}
};
inline Camera blend(Camera a, Camera b, float t) {
    return {blend(a.eye,b.eye,t),blend(a.target,b.target,t),
            a.verticalFov+(b.verticalFov-a.verticalFov)*t};
}
struct Mat4 { float m[16]; };
inline Mat4 perspective(float fovRadians, float aspect, float nearPlane, float farPlane) {
    Mat4 p = {};
    const float f=1.0f/std::tan(fovRadians/2);
    p.m[0]=f/aspect; p.m[5]=f;
    p.m[10]=(farPlane+nearPlane)/(nearPlane-farPlane);
    p.m[11]=-1; p.m[14]=2*farPlane*nearPlane/(nearPlane-farPlane);
    return p;
}
// Model matrix that turns car-local +X toward a horizontal unit direction and moves it to position.
inline Mat4 placement(Vec3 position, Vec3 forward) {
    Mat4 m={{forward.x,0,forward.z,0, 0,1,0,0, -forward.z,0,forward.x,0,
             position.x,position.y,position.z,1}};
    return m;
}
inline Mat4 lookAt(Camera c, Vec3 upDirection={0,1,0}) {
    const Vec3 f=unit(c.target-c.eye), r=unit(cross(f,upDirection)), u=cross(r,f);
    Mat4 v={{r.x,u.x,-f.x,0, r.y,u.y,-f.y,0, r.z,u.z,-f.z,0,
             -dot(r,c.eye),-dot(u,c.eye),dot(f,c.eye),1}};
    return v;
}
} // namespace astana
