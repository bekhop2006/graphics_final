#include "scene.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>
#include <utility>

namespace astana {
namespace {

const float pi = 3.14159265358979323846f;
const float tau = 2.0f * pi;
struct V { float x, y, z; };
struct UV { float u, v; };
typedef std::vector<Vertex> Mesh;

enum Material {
    Stone, Glass, Gold, Lamp, Grass, Paving, Water, Fabric,
    CarPaint, CarGlass, CarMetal, Rubber, AsphaltLight, Asphalt
};
const V ivory = {0.87f, 0.87f, 0.81f};
const V steel = {0.55f, 0.65f, 0.69f};
const V gold = {0.98f, 0.66f, 0.19f};
const V trim = {0.68f, 0.70f, 0.69f};
const V warm = {1.00f, 0.76f, 0.37f};

V operator+(V a, V b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
V operator-(V a, V b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
V operator*(V a, float b) { return {a.x*b, a.y*b, a.z*b}; }
float dot(V a, V b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
V cross(V a, V b) { return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x}; }
float length(V a) { return std::sqrt(dot(a,a)); }
V unit(V a) { float n=length(a); return n>1e-6f ? a*(1.0f/n) : V{0,1,0}; }
V lerp(V a, V b, float t) { return a*(1-t)+b*t; }

Vertex vertex(V p, V n, V c, UV uv, int mat, int id) {
    return {p.x,p.y,p.z, n.x,n.y,n.z, c.x,c.y,c.z, uv.u,uv.v, float(mat),float(id)};
}

void triangle(Mesh& m, V a, V b, V c, V normal, V color, int mat, int id,
              UV ta={0,0}, UV tb={1,0}, UV tc={1,1}) {
    V face=cross(b-a,c-a);
    if (dot(face, face)<1e-12f) return;
    if (dot(face,normal)<0) { std::swap(b,c); std::swap(tb,tc); face=face*(-1); }
    V n=unit(face);
    m.push_back(vertex(a,n,color,ta,mat,id));
    m.push_back(vertex(b,n,color,tb,mat,id));
    m.push_back(vertex(c,n,color,tc,mat,id));
}

void panel(Mesh& m, V a, V b, V c, V d, V facing, V color, int mat, int id,
           float repeatX=1, float repeatY=1, float startU=0, float startV=0) {
    UV ta={startU,startV}, tb={startU+repeatX,startV};
    UV tc={startU+repeatX,startV+repeatY}, td={startU,startV+repeatY};
    triangle(m,a,b,c,facing,color,mat,id,ta,tb,tc);
    triangle(m,a,c,d,facing,color,mat,id,ta,tc,td);
}

// Boxes measure facade texture coordinates in window bays and floors.
void box(Mesh& m, float x,float y,float z, float w,float h,float d,
         V col,int mat,int id=0) {
    float l=x-w/2, r=x+w/2, f=z+d/2, b=z-d/2, t=y+h;
    panel(m,{l,y,f},{r,y,f},{r,t,f},{l,t,f},{0,0,1},col,mat,id,w/0.95f,h/1.12f);
    panel(m,{r,y,b},{l,y,b},{l,t,b},{r,t,b},{0,0,-1},col,mat,id,w/0.95f,h/1.12f);
    panel(m,{r,y,f},{r,y,b},{r,t,b},{r,t,f},{1,0,0},col,mat,id,d/0.95f,h/1.12f);
    panel(m,{l,y,b},{l,y,f},{l,t,f},{l,t,b},{-1,0,0},col,mat,id,d/0.95f,h/1.12f);
    panel(m,{l,t,b},{l,t,f},{r,t,f},{r,t,b},{0,1,0},col,mat==Glass?Stone:mat,id,d,w);
}

V ring(V center, float rx,float rz,float angle) {
    return {center.x+rx*std::cos(angle),center.y,center.z+rz*std::sin(angle)};
}

// Connect two elliptical rings. Changing the centre makes leaning roofs possible.
void band(Mesh& m, V low,V high, float lx,float lz,float hx,float hz,int slices,
          V color,int mat,int id=0,bool cap=false) {
    float bays=std::max(4.0f,std::round(pi*(lx+lz)/0.95f));
    for (int i=0;i<slices;++i) {
        float a=tau*i/slices, b=tau*(i+1)/slices;
        V p=ring(low,lx,lz,a), q=ring(low,lx,lz,b);
        V r=ring(high,hx,hz,b), s=ring(high,hx,hz,a);
        V facing={std::cos((a+b)/2),0,std::sin((a+b)/2)};
        panel(m,p,q,r,s,facing,color,mat,id,bays/slices,(high.y-low.y)/1.12f,bays*i/slices,low.y/1.12f);
        if (cap) triangle(m,high,s,r,{0,1,0},color,mat==Glass?Stone:mat,id);
    }
}

void drum(Mesh& m,float x,float y,float z,float radius,float h,V c,int mat,int id=0,int sides=32) {
    band(m,{x,y,z},{x,y+h,z},radius,radius,radius,radius,sides,c,mat,id,true);
}

// A rod has its own local frame, so diagonals, roof ribs and vertical columns share one mesh builder.
void rod(Mesh& m,V a,V b,float r0,float r1,V color,int mat,int id=0,int sides=6) {
    if (length(b-a)<1e-5f) return;
    V axis=unit(b-a), ref=std::abs(axis.y)<0.85f ? V{0,1,0}:V{1,0,0};
    V u=unit(cross(axis,ref)), v=cross(axis,u);
    for(int i=0;i<sides;++i) {
        float p=tau*i/sides, q=tau*(i+1)/sides;
        V n=u*std::cos(p)+v*std::sin(p), k=u*std::cos(q)+v*std::sin(q);
        panel(m,a+n*r0,a+k*r0,b+k*r1,b+n*r1,n+k,color,mat,id);
    }
}

void orb(Mesh& m,V center,float rx,float ry,float rz,V color,int mat,int id,
         int slices=28,int stacks=16) {
    for(int j=0;j<stacks;++j) for(int i=0;i<slices;++i) {
        const int corners[4][2]={{i,j},{i+1,j},{i+1,j+1},{i,j+1}};
        V points[4], normals[4];
        for(int k=0;k<4;++k) {
            float az=tau*corners[k][0]/slices, lat=pi*(float(corners[k][1])/stacks-0.5f);
            float nx=std::cos(lat)*std::cos(az), ny=std::sin(lat), nz=std::cos(lat)*std::sin(az);
            points[k]=center+V{nx*rx,ny*ry,nz*rz};
            normals[k]=unit({nx/rx,ny/ry,nz/rz});
        }
        const int order[6]={0,2,1,0,3,2};
        for(int index=0;index<6;++index) {
            // A polar cell has one triangle because two corners meet at the pole.
            if ((j==0 && index<3) || (j==stacks-1 && index>=3)) continue;
            int k=order[index];
            m.push_back(vertex(points[k],normals[k],color,
                {float(corners[k][0])/slices,float(corners[k][1])/stacks},mat,id));
        }
    }
}

void hoop(Mesh& m,V c,float rx,float rz,float thickness,V color,int mat,int id,int n=24) {
    for(int i=0;i<n;++i)
        rod(m,ring(c,rx,rz,tau*i/n),ring(c,rx,rz,tau*(i+1)/n),thickness,thickness,color,mat,id,5);
}

void baiterek(Mesh& m) {
    const int id=1;
    drum(m,0,.07f,0,6.35f,.32f,trim,Stone,id,48);
    drum(m,0,.39f,0,5.55f,.36f,ivory,Stone,id,48);
    band(m,{0,.75f,0},{0,1.75f,0},4.50f,4.50f,2.65f,2.65f,36,ivory,Stone,id,true);
    band(m,{0,1.7f,0},{0,21,0},1.22f,1.22f,.85f,.85f,24,{.58f,.76f,.77f},Glass,id,true);

    // Sample the white crown at several heights. Opposite diagonals form open diamonds.
    const float heights[]={.83f,4.0f,11.5f,18.0f,22.6f,26.7f,29.5f,30.7f};
    const float radii[]  ={2.8f,1.83f,1.39f,1.83f,3.50f,5.20f,5.43f,5.20f};
    for(int i=0;i<16;++i) {
        float a=tau*i/16, step=tau/16;
        for(int k=0;k<7;++k) {
            V lo=ring({0,heights[k],0},radii[k],radii[k],a);
            V hi=ring({0,heights[k+1],0},radii[k+1],radii[k+1],a);
            rod(m,lo,hi,k<4?.163f:.132f,k==6?.045f:.145f,ivory,Stone,id);
            if(k>=2) {
                V right=ring({0,heights[k+1],0},radii[k+1],radii[k+1],a+step);
                V left=ring({0,heights[k+1],0},radii[k+1],radii[k+1],a-step);
                rod(m,lo,right,.069f,.069f,ivory,Stone,id,5);
                rod(m,lo,left,.069f,.069f,ivory,Stone,id,5);
            }
        }
    }
    for(int k=2;k<7;++k) hoop(m,{0,heights[k],0},radii[k],radii[k],.09f,ivory,Stone,id,32);
    orb(m,{0,28.7f,0},4.70f,4.70f,4.70f,gold,Gold,id,48,24);
    // Thin horizontal glazing joints follow the curvature of the observation sphere.
    for(int j=-2;j<=2;++j) {
        float lat=j*.24f,r=4.707f*std::cos(lat);
        hoop(m,{0,28.7f+4.70f*std::sin(lat),0},r,r,.028f,{.69f,.44f,.16f},Gold,id,40);
    }
}

void akorda(Mesh& m) {
    const int id=2;
    const float x=74;
    box(m,x,.08f,0,14.8f,.40f,24,trim,Stone,id);
    box(m,x,.48f,0,13.8f,1.0f,23,ivory,Stone,id);
    box(m,x,1.48f,0,13,6.6f,22,{.36f,.51f,.59f},Glass,id);
    for(int i=-5;i<=5;++i) {
        for(int side:{-1,1}) {
            float z=i*1.90f;
            box(m,x+side*6.60f,1.48f,z,.34f,6.6f,.40f,ivory,Stone,id);
        }
    }
    for(int i=-3;i<=3;++i) for(int side:{-1,1})
        box(m,x+i*1.78f,1.48f,side*11.08f,.36f,6.6f,.36f,ivory,Stone,id);
    box(m,x,4.7f,0,13.32f,.22f,22.35f,ivory,Stone,id);
    box(m,x,7.94f,0,13.8f,.64f,22.9f,ivory,Stone,id);
    box(m,x,8.58f,0,13.1f,.23f,22.2f,{.57f,.62f,.61f},Stone,id);
    // Portico faces west along the boulevard.
    for(int j=0;j<4;++j) box(m,x-7.67f-j*.37f,.10f,0,2.9f,.85f-.18f*j,10.0f+.48f*j,ivory,Stone,id);
    for(int j=0;j<8;++j) {
        float z=(j-3.5f)*1.24f;
        drum(m,x-8.05f,.90f,z,.35f,.39f,ivory,Stone,id,12);
        drum(m,x-8.05f,1.29f,z,.25f,5.75f,ivory,Stone,id,12);
        drum(m,x-8.05f,7.04f,z,.38f,.32f,ivory,Stone,id,12);
    }
    box(m,x-7.45f,7.33f,0,3.2f,.48f,10.5f,ivory,Stone,id);
    triangle(m,{x-9.08f,7.81f,-5.25f},{x-9.08f,7.81f,5.25f},{x-9.08f,9.65f,0},{-1,0,0},ivory,Stone,id);
    drum(m,x,8.80f,0,4.65f,1.45f,ivory,Stone,id,40);
    for(int i=0;i<20;++i) {
        V p=ring({x,8.83f,0},4.67f,4.67f,tau*i/20);
        rod(m,p,p+V{0,1.35f,0},.07f,.07f,gold,Gold,id,6);
    }
    const V blue={.10f,.44f,.68f};
    for(int j=0;j<10;++j) {
        float a=pi*.5f*j/10,b=pi*.5f*(j+1)/10;
        float ra=4.48f*std::cos(a),rb=std::max(.055f,4.48f*std::cos(b));
        float ya=10.28f+4.95f*std::sin(a),yb=10.28f+4.95f*std::sin(b);
        band(m,{x,ya,0},{x,yb,0},ra,ra,rb,rb,40,blue,Stone,id);
        for(int i=0;i<12;++i) {
            float t=tau*i/12;
            rod(m,ring({x,ya,0},ra+.035f,ra+.035f,t),ring({x,yb,0},rb+.035f,rb+.035f,t),.044f,.04f,gold,Gold,id,5);
        }
    }
    drum(m,x,15.20f,0,.63f,.55f,gold,Gold,id,16);
    rod(m,{x,15.73f,0},{x,20.00f,0},.28f,.055f,gold,Gold,id,12);
    orb(m,{x,17.06f,0},.42f,.45f,.42f,gold,Gold,id,16,8);
    V flag={.07f,.64f,.79f};
    panel(m,{x,19.15f,0},{x+1.55f,19.02f,.25f},{x+1.55f,19.80f,.25f},{x,19.94f,0},{0,0,1},flag,Stone,id);
    panel(m,{x,19.15f,-.02f},{x+1.55f,19.02f,.23f},{x+1.55f,19.80f,.23f},{x,19.94f,-.02f},{0,0,-1},flag,Stone,id);
}

void khanShatyr(Mesh& m) {
    const int id=3;
    const float x=-72;
    band(m,{x,.10f,0},{x,.65f,0},15.9f,12.1f,15.9f,12.1f,48,trim,Stone,id,true);
    band(m,{x,.65f,0},{x,2.65f,0},15.1f,11.4f,15.1f,11.4f,48,{.34f,.57f,.64f},Glass,id,true);
    hoop(m,{x,2.67f,0},15.15f,11.48f,.12f,warm,Lamp,id,48);
    const float heights[]={2.70f,4.6f,7.9f,12.6f,17.9f,22.7f};
    const float sizes[]={1.00f,.79f,.53f,.29f,.125f,.022f};
    const float offset[]={0,-.2f,-.9f,-1.9f,-3.0f,-3.8f};
    for(int j=0;j<5;++j) {
        V a={x+offset[j],heights[j],0}, b={x+offset[j+1],heights[j+1],0};
        band(m,a,b,15.25f*sizes[j],11.5f*sizes[j],15.25f*sizes[j+1],11.5f*sizes[j+1],
             48,{.76f,.79f,.83f},Fabric,id);
        for(int i=0;i<36;++i) {
            float t=tau*i/36;
            V p=ring(a,15.25f*sizes[j]+.045f,11.5f*sizes[j]+.045f,t);
            V q=ring(b,15.25f*sizes[j+1]+.045f,11.5f*sizes[j+1]+.045f,t);
            rod(m,p,q,.055f,.046f,i%3==0?ivory:steel,Stone,id,5);
        }
    }
    rod(m,{x-3.8f,22.65f,0},{x-4.45f,25,0},.20f,.04f,ivory,Stone,id,12);
    for(int i:{0,2,5}) {
        float t=tau*i/7;
        rod(m,ring({x,.65f,0},17,13,t),{x-4.0f,23.4f,0},.035f,.035f,steel,Stone,id,5);
    }
}

void pyramid(Mesh& m) {
    const int id=4;
    const float x=91,z=31,w=10.25f;
    box(m,x,.09f,z,22,.52f,22,trim,Stone,id);
    box(m,x,.61f,z,21.3f,.45f,21.3f,ivory,Stone,id);
    V corners[4]={{x-w,1.06f,z-w},{x+w,1.06f,z-w},{x+w,1.06f,z+w},{x-w,1.06f,z+w}};
    V tip={x,19.05f,z};
    for(int j=0;j<4;++j) {
        V a=corners[j],b=corners[(j+1)%4],facing=lerp(a,b,.5f)-V{x,0,z};
        auto point=[&](int i,int row) { return a+(b-a)*(i/8.0f)+(tip-a)*(row/8.0f); };
        for(int row=0;row<8;++row) for(int i=0;i<8-row;++i) {
            float tone=.87f+.13f*((row*7+i*3+j)%5)/4;
            V col=(row>=6?V{.66f,.78f,.81f}:V{.19f,.57f,.62f})*tone;
            V p=point(i,row),q=point(i+1,row),r=point(i,row+1);
            triangle(m,p,q,r,facing,col,Stone,id);
            if(i+1<8-row) triangle(m,q,point(i+1,row+1),r,facing,col*.92f,Stone,id);
        }
        for(int i=1;i<8;++i) {
            float t=i/8.0f;
            rod(m,lerp(a,tip,t),lerp(b,tip,t),.061f,.061f,steel,Stone,id,5);
            rod(m,lerp(a,b,t),lerp(a,tip,t),.059f,.059f,steel,Stone,id,5);
            rod(m,lerp(a,b,t),lerp(b,tip,1-t),.059f,.059f,steel,Stone,id,5);
        }
        rod(m,a,tip,.10f,.08f,ivory,Stone,id,6);
    }
}

void goldenTowers(Mesh& m) {
    const int id=5;
    for(int side:{-1,1}) {
        float x=48,z=side*10.0f;
        drum(m,x,.10f,z,4.25f,.36f,trim,Stone,id,32);
        drum(m,x,.46f,z,3.52f,1.12f,{.46f,.48f,.42f},Glass,id,32);
        const float h[]={1.58f,2.2f,5.1f,9.6f,14.3f,17.9f,18.6f};
        const float r[]={3.15f,3.30f,3.18f,2.91f,2.67f,2.57f,2.45f};
        for(int j=0;j<6;++j) {
            band(m,{x,h[j],z},{x,h[j+1],z},r[j],r[j],r[j+1],r[j+1],36,gold,Gold,id,j==5);
            int floors=(j>=2&&j<=4)?4:2;
            for(int k=0;k<floors;++k) {
                float t=float(k)/floors;
                float y=h[j]+(h[j+1]-h[j])*t;
                float radius=r[j]+(r[j+1]-r[j])*t+.012f;
                band(m,{x,y,z},{x,y+.075f,z},radius,radius,radius,radius,36,{.38f,.31f,.22f},Stone,id);
            }
            for(int k=0;k<20;++k) {
                float a=tau*k/20;
                rod(m,ring({x,h[j],z},r[j]+.015f,r[j]+.015f,a),
                      ring({x,h[j+1],z},r[j+1]+.015f,r[j+1]+.015f,a),.034f,.034f,{.54f,.43f,.24f},Stone,id,4);
            }
        }
        hoop(m,{x,18.65f,z},2.45f,2.45f,.095f,warm,Lamp,id,32);
    }
}

void emerald(Mesh& m) {
    const int id=6;
    const float xs[]={-23.0f,-16.0f,-9.0f}, zs[]={-28.9f,-26.1f,-28.0f}, hs[]={21.8f,28.0f,18.4f};
    box(m,-16,.08f,-27.3f,22.4f,.52f,11.5f,steel,Stone,id);
    for(int tower=0;tower<3;++tower) {
        float x=xs[tower], z=zs[tower], h=hs[tower];
        auto outline=[&](int i,float t) {
            float a=tau*i/24, cs=std::cos(a), sn=std::sin(a);
            float lean=2.7f*t*t, width=2.60f+.80f*t, depth=3.30f+1.08f*t;
            return V{x+lean+width*cs,.60f+(h+.98f*sn+1.35f*cs)*t,z+depth*sn};
        };
        for(int k=0;k<4;++k) for(int i=0;i<24;++i) {
            float lo=k/4.0f, hi=(k+1)/4.0f, a=tau*(i+.5f)/24;
            V p=outline(i,lo),q=outline(i+1,lo),r=outline(i+1,hi),s=outline(i,hi);
            panel(m,p,q,r,s,{std::cos(a),0,std::sin(a)},{.13f,.48f,.46f},Glass,id,1,h/4.4f,float(i),h*lo/1.1f);
            if(i%2==0) rod(m,p,s,.044f,.044f,{.38f,.68f,.64f},Stone,id,5);
            if(k==3) {
                triangle(m,{x+2.7f,.60f+h,z},s,r,{0,1,0},{.39f,.50f,.51f},Stone,id);
                rod(m,s,r,.073f,.073f,{.47f,.94f,.79f},Lamp,id,5);
            }
        }
    }
}

void tree(Mesh& m,float x,float z,float h,unsigned seed) {
    V brown={.35f,.26f,.18f};
    float t=(seed%11)/10.0f;
    V green={.14f+t*.085f,.28f+t*.10f,.19f+t*.04f};
    rod(m,{x,.19f,z},{x,h*.69f,z},h*.039f,h*.022f,brown,Stone,0,6);
    orb(m,{x,h*.71f,z},h*.254f,h*.35f,h*.224f,green,Stone,0,9,6);
    orb(m,{x+.35f,h*.68f,z+.16f},h*.18f,h*.24f,h*.17f,green*1.08f,Stone,0,8,5);
}

void lantern(Mesh& m,float x,float z) {
    V pole={.26f,.29f,.30f};
    rod(m,{x,.15f,z},{x,2.74f,z},.064f,.047f,pole,Stone,0,7);
    rod(m,{x,2.71f,z},{x+.24f,2.93f,z},.052f,.042f,pole,Stone,0,6);
    orb(m,{x+.25f,2.89f,z},.215f,.14f,.16f,{1,.83f,.55f},Lamp,0,10,7);
}

void pool(Mesh& m,float x,float z,float w,float d) {
    box(m,x,.13f,z,w+.74f,.21f,d+.74f,{.73f,.73f,.70f},Stone);
    box(m,x,.344f,z,w,.04f,d,{.14f,.51f,.60f},Water);
    for(int i=0;i<3;++i) {
        float px=x+(i-1)*w*.26f;
        band(m,{px,.40f,z},{px,1.08f+(i%2)*.42f,z},.14f,.14f,.044f,.044f,8,
             {.58f,.83f,.85f},Stone);
    }
}

void cityBlocks(Mesh& m) {
    struct Building {float x,z,w,h,d; int kind;};
    const Building buildings[]={
        {6,28,7,8.3f,5,1},{15,29,7,10,5,0},{22,27,5,4.9f,4,2},{-2,31,6,5.6f,4,2},
        {-47,-28,7,9,6,2},{-58,-29,8,13.5f,5,1},{-39,-27,5,12,6,0},{-68,-31,8,7,5,2},
        {5,-31,7,11,6,1},{15,-31,6,13,5,0},{24,-33,6,15,6,1},{33,-29,6,9,5,2},
        {47,-31,9,10,6,2},{59,-34,8,11.4f,5,1},{68,-31,6,7.2f,5,0},{82,-25,6,6.8f,5,2},
        {-88,24,7,6.1f,5,2},{-82,29,5,8,5,0},{-50,28,6,5.8f,5,1},{-40,30,8,4,5,2},
        {47,33,6,5.5f,4,2},{57,32,7,5,5,1},{67,34,5,4,4,2},
        {-90,-44,9,7,6,0},{-74,-45,7,10,5,2},{-58,-44,7,7,6,0},{-44,-45,6,12.6f,5,1},
        {-32,-43,7,10,5,0},{-3,-45,7,11,6,2},{10,-44,6,8.5f,5,1},{26,-46,7,9,5,2},
        {40,-44,6,7,6,0},{54,-45,9,9,5,1},{68,-45,6,8.6f,5,1},{83,-43,9,7,5,2}
    };
    const V colors[]={{.35f,.47f,.54f},{.37f,.48f,.50f},{.56f,.52f,.45f}};
    for(const Building& b:buildings) {
        const int id=7;
        V col=colors[b.kind];
        box(m,b.x,.10f,b.z,b.w+1.2f,.24f,b.d+1.0f,trim,Paving,id);
        box(m,b.x,.34f,b.z,b.w,.75f,b.d,{.44f,.45f,.44f},Stone,id);
        box(m,b.x,1.09f,b.z,b.w,b.h,b.d,col,Glass,id);
        float top=1.09f+b.h;
        box(m,b.x,top,b.z,b.w+.13f,.19f,b.d+.13f,trim,Stone,id);
        if(b.kind==0) {
            box(m,b.x,top+.19f,b.z,b.w*.65f,1.0f,b.d*.67f,col,Glass,id);
            for(int j:{-1,1})
                box(m,b.x+j*(b.w/2-.36f),1.09f,b.z,.28f,b.h,b.d+.12f,trim,Stone,id);
        } else if(b.kind==1) {
            for(float y=2.5f;y<top-.2f;y+=3.1f)
                box(m,b.x,y,b.z,b.w+.13f,.15f,b.d+.15f,{.58f,.62f,.62f},Stone,id);
            box(m,b.x+.58f,top+.19f,b.z,.90f,.58f,1.3f,steel,Stone,id);
        } else {
            for(int j:{-1,0,1})
                box(m,b.x+j*b.w*.32f,1.09f,b.z,.19f,b.h,b.d+.11f,{.68f,.63f,.54f},Stone,id);
            box(m,b.x,top+.19f,b.z,b.w*.49f,.48f,b.d*.50f,trim,Stone,id);
        }
    }
}

enum class BodyStyle { Fastback, SUV, Taxi, Compact };

// Dimensions are in metres in car-local space. The nose points along +X.
struct CarShape {
    float length, width, axle, tire, belt, roofRise;
    float rearGlassBase, roofRear, roofFront, windshieldBase;
    float noseDrop, noseRound;
};

CarShape carShape(BodyStyle body) {
    switch(body) {
        case BodyStyle::Fastback: return {4.74f,1.94f,1.40f,.350f,.87f,.53f,-1.96f,-.69f,.20f,1.20f,.225f,.48f};
        case BodyStyle::SUV:      return {4.64f,2.00f,1.38f,.390f,1.02f,.69f,-2.16f,-1.62f,.49f,1.34f,.175f,.34f};
        case BodyStyle::Taxi:     return {4.67f,1.86f,1.38f,.338f,.91f,.59f,-1.76f,-.82f,.34f,1.27f,.205f,.43f};
        case BodyStyle::Compact:  return {3.76f,1.79f,1.16f,.317f,.86f,.61f,-1.74f,-1.27f,.18f,1.03f,.210f,.46f};
    }
    return {};
}

float smooth01(float t) {
    t=std::max(0.f,std::min(1.f,t));
    return t*t*(3.f-2.f*t);
}
float bodyHalfWidth(const CarShape& s,float x) {
    const float t=std::abs(2*x/s.length);
    return .5f*s.width*(1.f-.072f*smooth01((t-.56f)/.44f));
}
float beltHeight(const CarShape& s,float x) {
    // A shallow continuous slope leaves the centre of the hood above both wings.
    const float t=2*x/s.length;
    const float front=std::max(0.f,(t-.36f)/.64f);
    const float rear=std::max(0.f,(-t-.48f)/.52f);
    return s.belt-.65f*s.noseDrop*std::pow(front,1.55f)-.052f*rear*rear;
}
float deck(const CarShape& s,float x) { return beltHeight(s,x)+.095f; }

struct BodySlice {float x, bottom;};

// Weld only the shading at coincident corners. An angle limit preserves the sharp
// shoulder and window seals; corner-angle weights are independent of triangulation.
void smoothCarPanels(Mesh& m,std::size_t start,int material,float minimumDot) {
    typedef std::tuple<int,int,int> Key;
    struct Corner {V normal;float weight;};
    std::map<Key,std::vector<Corner>> adjacent;
    auto key=[](const Vertex& v) {
        return Key(int(std::lround(v.x*100000.f)),int(std::lround(v.y*100000.f)),
                   int(std::lround(v.z*100000.f)));
    };
    auto position=[](const Vertex& v) {return V{v.x,v.y,v.z};};
    for(std::size_t i=start;i+2<m.size();i+=3) {
        if(int(m[i].mat)!=material) continue;
        for(int k=0;k<3;++k) {
            const Vertex& v=m[i+k];
            V a=unit(position(m[i+(k+1)%3])-position(v));
            V b=unit(position(m[i+(k+2)%3])-position(v));
            float angle=std::acos(std::max(-1.f,std::min(1.f,dot(a,b))));
            adjacent[key(v)].push_back({{v.nx,v.ny,v.nz},angle});
        }
    }
    for(std::size_t i=start;i<m.size();++i) {
        Vertex& v=m[i];
        if(int(v.mat)!=material) continue;
        V normal={v.nx,v.ny,v.nz},sum={0,0,0};
        for(const Corner& c:adjacent.find(key(v))->second)
            if(dot(normal,c.normal)>=minimumDot) sum=sum+c.normal*c.weight;
        if(length(sum)>.0001f) {
            V n=unit(sum);
            v.nx=n.x;v.ny=n.y;v.nz=n.z;
        }
    }
}

// Eight longitudinal rings continue round each bumper. The last ring is also the
// boundary of the hood and trunk, so the three patches share an identical edge.
const int shoulderRow=7;
const float deckColumns[]={0,.18f,.36f,.52f,.66f,.76f,.84f,.90f,.945f,.974f,.991f,1.f};
const int deckSegments=sizeof(deckColumns)/sizeof(deckColumns[0])-1;
float carClearance(const CarShape& s) { return s.tire>.37f?.238f:.174f; }
struct BodyRing { float y, width, setback, crown; };
BodyRing bodyRing(const CarShape& s,BodySlice slice,int row) {
    const float belt=beltHeight(s,slice.x),bottom=slice.bottom,span=belt-bottom;
    // Keep the upper shoulder level above the arches. Only the lower rings
    // climb over a wheel. This gives the fender a continuous reflection line.
    const float safeBottom=std::min(bottom,belt-.080f);
    const float heights[]={bottom,bottom+.12f*span,bottom+.26f*span,bottom+.40f*span,
        std::max(belt-.203f,safeBottom+.56f*(belt-safeBottom)),
        std::max(belt-.101f,safeBottom+.73f*(belt-safeBottom)),belt-.010f,belt+.030f};
    // Sample width at the physical height, even when the wheel cuts through
    // the lower rings. The flank stays smooth instead of pinching above a tyre.
    const float ground=carClearance(s),endWeight=smooth01((std::abs(2*slice.x/s.length)-.60f)/.40f);
    const float h=(heights[row]-ground)/(belt+.030f-ground);
    const float breaks[]={0,.12f,.26f,.40f,.63f,.82f,.94f,1};
    const float widths[]={.938f,.966f,.984f,.994f-.014f*endWeight,.999f,1.f,.989f,.964f};
    int segment=0;
    while(segment<6 && h>breaks[segment+1]) ++segment;
    float t=std::max(0.f,std::min(1.f,(h-breaks[segment])/(breaks[segment+1]-breaks[segment])));
    float width=widths[segment]+(widths[segment+1]-widths[segment])*t;
    const float setbacks[]={.175f,.075f,.025f,.070f,0.f,.010f,.025f,.071f};
    const float crowns[]={0,0,0,.025f,.05f,.16f,.70f,1};
    return {heights[row],bodyHalfWidth(s,slice.x)*width,setbacks[row],crowns[row]};
}
V ringSurface(const CarShape& s,float x,BodyRing r,float u) {
    const float a=std::min(1.f,std::abs(u));
    const float endWeight=smooth01((std::abs(2*x/s.length)-.54f)/.46f);
    // An elliptical plan-view corner approaches the flank tangentially.
    const float a2=a*a,a6=a2*a2*a2;
    const float corner=1.f-std::sqrt(std::max(0.f,1.f-a6));
    const float radius=x>0?s.noseRound*.75f:.30f;
    const float sign=x>0?1.f:-1.f;
    const float platePocket=.024f*(1.f-smooth01((a-.27f)/.20f))*
        smooth01((r.y-.37f)/.10f)*(1.f-smooth01((r.y-.60f)/.10f));
    const float crown=.020f*(1-a2)+.046f*(1-a6);
    return {x-sign*endWeight*(r.setback+radius*corner+platePocket),r.y+r.crown*crown,r.width*u};
}
V bodyPoint(const CarShape& s,BodySlice slice,int side,int row) {
    return ringSurface(s,slice.x,bodyRing(s,slice,row),float(side));
}
V topSurface(const CarShape& s,float x,float u) {
    return ringSurface(s,x,bodyRing(s,{x,carClearance(s)},shoulderRow),u);
}
// Intersect a lamp sample with the same two triangles that close each bumper
// cell. Even the plate pocket and the tight corners then stay flush at close range.
V fasciaPoint(const CarShape& s,int end,float u,float height,float relief) {
    const float x=end*s.length*.5f,absU=std::min(1.f,std::abs(u));
    const BodySlice edge={x,carClearance(s)};
    int col=0;
    while(col<deckSegments-1 && absU>deckColumns[col+1]) ++col;
    const float side=u<0?-1.f:1.f;
    const float a=side*deckColumns[col],b=side*deckColumns[col+1];
    const float t=(absU-deckColumns[col])/(deckColumns[col+1]-deckColumns[col]);
    BodyRing low=bodyRing(s,edge,0);
    V p=lerp(ringSurface(s,x,low,a),ringSurface(s,x,low,b),t);
    for(int row=1;row<=shoulderRow;++row) {
        BodyRing high=bodyRing(s,edge,row);
        V q=lerp(ringSurface(s,x,high,a),ringSurface(s,x,high,b),t);
        if(height<=q.y || row==shoulderRow) {
            V joint=lerp(ringSurface(s,x,low,a),ringSurface(s,x,high,b),t);
            V from=height<joint.y?p:joint,to=height<joint.y?joint:q;
            float dy=to.y-from.y,v=dy>.00001f?(height-from.y)/dy:0.f;
            V result=lerp(from,to,std::max(0.f,std::min(1.f,v)));
            result.x+=end*relief;
            result.z+=std::copysign(relief*smooth01((absU-.75f)/.25f),u);
            return result;
        }
        low=high;p=q;
    }
    return p;
}
void carBody(Mesh& m,const CarShape& s,V paint,BodyStyle style) {
    const std::size_t first=m.size();
    const float half=s.length*.5f,groundClearance=carClearance(s),opening=s.tire+.055f;
    std::vector<BodySlice> slices={{-half,groundClearance}};
    const float rearEdge=-s.axle-opening-.017f,frontEdge=-rearEdge;
    for(int i=1;i<6;++i) slices.push_back({-half+(rearEdge+half)*i/6,groundClearance});
    for(float axle:{-s.axle,s.axle}) {
        slices.push_back({axle-opening-.017f,groundClearance});
        for(int i=0;i<=16;++i) {
            float a=pi*(1.f-i/16.f);
            slices.push_back({axle+opening*std::cos(a),s.tire+opening*std::sin(a)});
        }
        slices.push_back({axle+opening+.017f,groundClearance});
        if(axle<0) slices.push_back({0,groundClearance});
    }
    for(int i=1;i<=6;++i) slices.push_back({frontEdge+(half-frontEdge)*i/6,groundClearance});
    const V liner={.075f,.084f,.092f};
    for(int side:{-1,1}) {
        const V outward={0,0,float(side)};
        for(std::size_t i=1;i<slices.size();++i) {
            BodySlice a=slices[i-1],b=slices[i];
            for(int k=0;k<shoulderRow;++k) {
                panel(m,bodyPoint(s,a,side,k),bodyPoint(s,b,side,k),
                      bodyPoint(s,b,side,k+1),bodyPoint(s,a,side,k+1),outward,paint,CarPaint,0);
            }
            for(int j=0;j<deckSegments;++j) {
                float u=side*deckColumns[j],v=side*deckColumns[j+1];
                panel(m,topSurface(s,a.x,u),topSurface(s,b.x,u),
                      topSurface(s,b.x,v),topSurface(s,a.x,v),{0,1,0},paint,CarPaint,0);
            }
            V p=bodyPoint(s,a,side,0),q=bodyPoint(s,b,side,0),inner={0,0,-.24f*side};
            panel(m,p,q,q+inner,p+inner,{0,-1,0},liner,Rubber,0);
            if(a.bottom>groundClearance+.01f || b.bottom>groundClearance+.01f) {
                float lip=style==BodyStyle::SUV?.040f:.014f;
                V zoff={0,0,side*.0025f};
                panel(m,p+zoff,q+zoff,q+V{0,lip,side*.010f},p+V{0,lip,side*.010f},
                      outward,style==BodyStyle::SUV?liner:paint*.85f,
                      style==BodyStyle::SUV?Rubber:CarPaint,0);
            }
        }
        for(BodySlice edge:{slices.front(),slices.back()}) {
            const V normal={edge.x>0?1.f:-1.f,0,0};
            for(int row=0;row<shoulderRow;++row) {
                const BodyRing low=bodyRing(s,edge,row),high=bodyRing(s,edge,row+1);
                for(int j=0;j<deckSegments;++j) {
                    float u=side*deckColumns[j],v=side*deckColumns[j+1];
                    panel(m,ringSurface(s,edge.x,low,u),ringSurface(s,edge.x,low,v),
                          ringSurface(s,edge.x,high,v),ringSurface(s,edge.x,high,u),normal,paint,CarPaint,0);
                }
            }
        }
    }
    smoothCarPanels(m,first,CarPaint,.65f);
    box(m,0,groundClearance-.016f,0,s.length*.87f,.030f,s.width*.64f,liner,Rubber);
}
float cabinFactor(const CarShape& s,float x) {
    if(x<s.roofRear) {
        float u=(x-s.rearGlassBase)/(s.roofRear-s.rearGlassBase);
        return std::max(0.f,u*(1.32f-.32f*u));
    }
    if(x>s.roofFront) {
        float u=(s.windshieldBase-x)/(s.windshieldBase-s.roofFront);
        return std::max(0.f,u*(1.25f-.25f*u));
    }
    float u=(x-s.roofRear)/(s.roofFront-s.roofRear);
    return 1.f+.035f*std::sin(pi*u);
}
V windowPoint(const CarShape& s,float x,float side,float t) {
    float h=cabinFactor(s,x);
    return {x,deck(s,x)-.026f+s.roofRise*h*t,
            side*bodyHalfWidth(s,x)*(.900f-.213f*h*t)};
}
V roofPoint(const CarShape& s,float x,float u) {
    V p=windowPoint(s,x,u,1);
    p.y+=.038f*(1.f-u*u)*cabinFactor(s,x);
    return p;
}
void carCabin(Mesh& m,const CarShape& s,V paint,BodyStyle style) {
    const std::size_t first=m.size();
    const V gasket={.065f,.080f,.088f},glass={.12f,.22f,.27f};
    const V roofColor=style==BodyStyle::Compact?V{.88f,.90f,.86f}:paint;
    float pillar=(s.roofRear+s.roofFront)*.5f;
    std::vector<float> stations={s.rearGlassBase,s.roofRear,s.roofFront,s.windshieldBase,
                                pillar-.042f,pillar+.042f};
    for(float u:{.09f,.27f,.52f,.79f,.94f}) {
        stations.push_back(s.rearGlassBase+(s.roofRear-s.rearGlassBase)*u);
        stations.push_back(s.roofFront+(s.windshieldBase-s.roofFront)*u);
    }
    std::sort(stations.begin(),stations.end());
    for(std::size_t i=1;i<stations.size();++i) {
        float x0=stations[i-1], x1=stations[i], xc=(x0+x1)*.5f;
        bool pillarZone=std::abs(xc-pillar)<.042f;
        float rearU=(xc-s.rearGlassBase)/(s.roofRear-s.rearGlassBase);
        float frontU=(xc-s.roofFront)/(s.windshieldBase-s.roofFront);
        bool windshield=frontU>0.f && frontU<.94f;
        bool backlight=rearU>.09f && rearU<1.f;
        for(int side:{-1,1}) {
            float sidef=float(side);
            bool endPillar=rearU<.27f || frontU>.79f;
            auto strip=[&](float lo,float hi,V color,int material) {
                panel(m,windowPoint(s,x0,sidef,lo),windowPoint(s,x1,sidef,lo),
                      windowPoint(s,x1,sidef,hi),windowPoint(s,x0,sidef,hi),
                      {0,.1f,sidef},color,material,0);
            };
            strip(0,.12f,paint,CarPaint);
            strip(.12f,.155f,{.52f,.59f,.60f},CarMetal);
            strip(.155f,.93f,endPillar?paint:pillarZone?gasket:glass,
                  endPillar?CarPaint:pillarZone?Rubber:CarGlass);
            strip(.93f,1.f,roofColor,CarPaint);
            bool glassTop=windshield || backlight;
            for(int j=0;j<4;++j) {
                const float borders[]={0,.50f,.87f,.91f,1.f};
                float a=sidef*borders[j],b=sidef*borders[j+1];
                V color=glassTop?(j<2?glass:j==2?gasket:paint):roofColor;
                int material=glassTop?(j<2?CarGlass:j==2?Rubber:CarPaint):CarPaint;
                panel(m,roofPoint(s,x0,a),roofPoint(s,x1,a),roofPoint(s,x1,b),roofPoint(s,x0,b),
                      {0,1,sidef*.1f},color,material,0);
            }
        }
    }
    if(style==BodyStyle::SUV) {
        for(int side:{-1,1}) {
            V rear=roofPoint(s,s.roofRear+.13f,side*.80f);
            V front=roofPoint(s,s.roofFront-.06f,side*.80f);
            V lift={0,.080f,0};
            rod(m,rear,rear+lift,.040f,.028f,gasket,Rubber,0);
            rod(m,front,front+lift,.040f,.028f,gasket,Rubber,0);
            rod(m,rear+lift,front+lift,.029f,.029f,steel,CarMetal,0,8);
        }
    }
    smoothCarPanels(m,first,CarPaint,.86f);
    smoothCarPanels(m,first,CarGlass,.82f);
}

void carWheel(Mesh& m,float x,float z,int side,float radius,float tireWidth) {
    const int segments=28;
    const V black={.081f,.089f,.104f}, sidewall={.117f,.128f,.143f};
    const V silver={.70f,.73f,.75f}, barrel={.18f,.20f,.23f}, brake={.23f,.26f,.28f};
    auto circle=[&](float a,float r,float depth) {
        return V{x+r*radius*std::cos(a),radius*(1.f+r*std::sin(a)),z+side*depth};
    };
    // Rounded shoulders lead from the tread to the broad sidewall and machined lip.
    auto ringStrip=[&](float r0,float d0,float r1,float d1,V color,int material) {
        for(int i=0;i<segments;++i) {
            float a=tau*i/segments,b=tau*(i+1)/segments;
            float outward=r0-r1, radial=d1-d0;
            V n={std::cos((a+b)/2)*radial,std::sin((a+b)/2)*radial,side*outward};
            panel(m,circle(a,r0,d0),circle(b,r0,d0),circle(b,r1,d1),circle(a,r1,d1),n,color,material,0);
        }
    };
    ringStrip(.90f,-tireWidth,1.0f,-tireWidth*.76f,black,Rubber);
    ringStrip(1.f,-tireWidth*.76f,1.f,-.040f,black,Rubber);
    ringStrip(1.f,-.040f,.94f,-.010f,black,Rubber);
    ringStrip(.94f,-.010f,.84f,0,sidewall,Rubber);
    ringStrip(.84f,0,.735f,-.009f,black,Rubber);
    ringStrip(.735f,-.009f,.68f,-.005f,silver,CarMetal);
    ringStrip(.68f,-.005f,.62f,-.046f,barrel,CarMetal);
    ringStrip(.62f,-.046f,.01f,-.047f,brake,CarMetal);
    for(int i=0;i<3;++i) {
        float a=.12f+i*.19f,b=a+.19f;
        panel(m,circle(a,.47f,-.035f),circle(a,.61f,-.035f),
              circle(b,.61f,-.035f),circle(b,.47f,-.035f),
              {0,0,float(side)},{.46f,.19f,.12f},CarPaint,0);
    }
    // Five pairs of swept spokes leave openings through which the brake disc is visible.
    for(int i=0;i<5;++i) {
        float a=tau*i/5+.12f;
        for(int branch:{-1,1}) {
            float tip=a+branch*.19f+.06f;
            panel(m,circle(a-.115f,.17f,.004f),circle(a+.115f,.17f,.004f),
                  circle(tip+.070f,.69f,-.008f),circle(tip-.070f,.69f,-.008f),
                  {0,0,float(side)},silver,CarMetal,0);
        }
    }
    ringStrip(.23f,.004f,.16f,.026f,silver,CarMetal);
    ringStrip(.16f,.026f,0,.026f,barrel,CarMetal);
    for(int i=0;i<5;++i) {
        float a=tau*i/5+.55f;
        V p=circle(a,.185f,.024f);
        float r=radius*.030f;
        panel(m,p+V{-r,-r,0},p+V{r,-r,0},p+V{r,r,0},p+V{-r,r,0},
              {0,0,float(side)},black,Rubber,0);
    }
}

// Flat printed marks are intentionally cut from geometry, so no font file is required.
void carPlate(Mesh& m,const CarShape& s,int end,int number) {
    const V pale={.84f,.87f,.84f},ink={.022f,.030f,.036f};
    float y=end>0?beltHeight(s,s.length/2)-.219f:beltHeight(s,-s.length/2)-.256f;
    float depth=0;
    for(int i=0;i<=10;++i) for(float u:{0.f,.18f,.34f}) {
        V skin=fasciaPoint(s,end,u,y-.069f+.141f*i/10.f,0);
        depth=std::max(depth,end*skin.x);
    }
    float x=end*(depth+.019f);
    V n={float(end),0,0};
    auto rectangle=[&](float dx,float z0,float z1,float y0,float y1,V color) {
        panel(m,{x+end*dx,y+y0,z0},{x+end*dx,y+y0,z1},
              {x+end*dx,y+y1,z1},{x+end*dx,y+y1,z0},n,color,Stone,0);
    };
    V frame={.070f,.085f,.095f};
    for(int side:{-1,1}) {
        const float z=side*.277f;
        panel(m,{x-end*.019f,y-.063f,z},{x-end*.004f,y-.063f,z},
              {x-end*.004f,y+.068f,z},{x-end*.019f,y+.068f,z},{0,0,float(side)},frame,Rubber,0);
    }
    panel(m,{x-end*.019f,y+.068f,-.277f},{x-end*.004f,y+.068f,-.277f},
          {x-end*.004f,y+.068f,.277f},{x-end*.019f,y+.068f,.277f},{0,1,0},frame,Rubber,0);
    rectangle(-.004f,-.277f,.277f,-.063f,.068f,{.070f,.085f,.095f});
    rectangle(0,-.265f,.265f,-.052f,.056f,pale);
    rectangle(.003f,-.263f,-.210f,-.051f,.054f,{.10f,.45f,.68f});
    // A repeated seven-segment alphabet creates a legible three-digit fleet number.
    const int bits[]={63,6,91,79,102,109,125,7,127,111};
    const float seg[7][4]={{0,.048f,.031f,.043f},{.037f,.048f,.004f,.036f},
        {.037f,.048f,-.029f,.000f},{0,.048f,-.039f,-.026f},{0,.011f,-.030f,.000f},
        {0,.011f,.004f,.037f},{0,.048f,-.006f,.008f}};
    for(int i=0;i<3;++i) {
        int divisor=i==0?100:i==1?10:1;
        int pattern=bits[number/divisor%10];
        for(int j=0;j<7;++j) if(pattern&(1<<j)) {
            float origin=-.088f+i*.077f;
            // Right on the plate is +Z at the rear and -Z at the nose.
            rectangle(.006f,-end*(origin+seg[j][0]),-end*(origin+seg[j][1]),
                      seg[j][2],seg[j][3],ink);
        }
    }
}

void carBumpers(Mesh& m,const CarShape& s,V paint,BodyStyle style) {
    const V black={.061f,.077f,.087f},metal={.34f,.40f,.43f};
    const V lampCase={.16f,.21f,.24f},led={.72f,.88f,.97f};
    const V smoke={.27f,.060f,.069f},red={.94f,.070f,.049f};
    const bool suv=style==BodyStyle::SUV,mini=style==BodyStyle::Compact;
    const float front=beltHeight(s,.5f*s.length),tail=beltHeight(s,-.5f*s.length);
    // Give openings and lenses rounded ends and a continuous curved mounting surface.
    auto ribbon=[&](int end,float u0,float u1,float centre0,float centre1,float halfH0,float halfH1,
                    float bend,float inset,V color,int material,int roundEnds) {
        const int columns=std::max(7,std::min(44,int(std::ceil(std::abs(u1-u0)*47.f))));
        const int rows=std::max(1,int(std::ceil(std::max(halfH0,halfH1)*2.f/.035f)));
        auto point=[&](float t,float v) {
            float edge=roundEnds==2?t:std::min(t,1.f-t);
            float tip=roundEnds && edge<.10f?std::sqrt(std::max(0.f,1.f-std::pow(1.f-10.f*edge,2.f))):1.f;
            float h=centre0+(centre1-centre0)*t+bend*(4*t*(1-t));
            h+=(halfH0+(halfH1-halfH0)*t)*tip*v;
            return fasciaPoint(s,end,u0+(u1-u0)*t,h,inset);
        };
        for(int i=0;i<columns;++i) for(int row=0;row<rows;++row) {
            float a=i/float(columns),b=(i+1)/float(columns);
            float c=-1.f+2.f*row/rows,d=-1.f+2.f*(row+1)/rows;
            panel(m,point(a,c),point(b,c),point(b,d),point(a,d),{float(end),0,0},color,material,0);
        }
    };
    // Trace a lens from its front-facing part around the tangent of the corner
    // and onto the quarter panel. The return stays flush against the same rings.
    auto lampReturn=[&](int end,int side,float y,float height,float reach,V color,int material,float relief) {
        const float half=s.length*.5f;
        auto curve=[&](float t,float v) {
            float h=y+.016f*t+v*height*(1.f-t*t);
            if(t<=.50f) {
                float q=1-2*t,u=1-.041f*q*q;
                return fasciaPoint(s,end,side*u,h,relief);
            }
            float d=(t-.50f)*2,x=end*(half-d*reach);
            BodySlice cut={x,carClearance(s)};
            V p=bodyPoint(s,cut,side,0);
            for(int i=1;i<=shoulderRow;++i) {
                V q=bodyPoint(s,cut,side,i);
                if(h<=q.y || i==shoulderRow) {
                    float v=std::max(0.f,std::min(1.f,(h-p.y)/(q.y-p.y)));
                    V point=lerp(p,q,v);
                    point.z+=side*relief;
                    point.x+=end*relief*(1-d);
                    return point;
                }
                p=q;
            }
            return p;
        };
        for(int i=0;i<22;++i) {
            float a=i/22.f,b=(i+1)/22.f;
            panel(m,curve(a,-1),curve(b,-1),curve(b,1),curve(a,1),
                  {float(end)*(1-a),0,float(side)*(.2f+a)},color,material,0);
        }
    };
    float low=carClearance(s)+.132f;
    // The intake is a single soft trapezoid with two horizontal vanes.
    for(int side:{-1,1}) {
        float k=float(side);
        ribbon(1,0,k*.65f,low+.073f,low+.079f,.100f,.066f,-.007f,.008f,black,Rubber,false);
        ribbon(1,k*.65f,k*.81f,low+.079f,low+.092f,.066f,.003f,-.002f,.009f,black,Rubber,false);
        for(int v=0;v<2;++v) {
            float y=low+.035f+.064f*v;
            ribbon(1,0,k*(.66f+.030f*v),y,y+.025f,.006f,.005f,-.004f,.014f,metal,CarMetal,false);
        }
        // Narrow brake-cooling pockets flank the main intake on the sports body.
        if(style==BodyStyle::Fastback || suv) {
            float vent=low+(suv?.121f:.099f);
            ribbon(1,k*.84f,k*.966f,vent-.026f,vent+.033f,.028f,.065f,
                   -.009f,.011f,black,Rubber,true);
            ribbon(1,k*.859f,k*.946f,vent-.030f,vent+.006f,.005f,.007f,
                   -.006f,.018f,suv?metal:paint*.50f,CarMetal,true);
        }
        float in=suv?.43f:style==BodyStyle::Taxi?.45f:.37f,out=.966f;
        float y=front-.003f;
        if(mini) {
            // A compact uses two inset oval projectors with a horseshoe running light.
            const float mid=.70f,rx=.204f,ry=.078f;
            auto oval=[&](float ang,float radius,float relief) {
                float lateral=radius*std::cos(ang);
                return fasciaPoint(s,1,k*(mid+rx*lateral),y+.016f*lateral+ry*radius*std::sin(ang),relief);
            };
            for(int i=0;i<40;++i) {
                float a=tau*i/40,b=tau*(i+1)/40;
                for(int j=0;j<4;++j) {
                    float r=j*.25f,r1=(j+1)*.25f;
                    panel(m,oval(a,r,.012f),oval(a,r1,.012f),oval(b,r1,.012f),oval(b,r,.012f),
                          {1,0,0},lampCase,CarMetal,0);
                }
                panel(m,oval(a,.94f,.019f),oval(a,1,.019f),oval(b,1,.019f),oval(b,.94f,.019f),
                      {1,0,0},metal,CarMetal,0);
                if(i<25 || i>=33)
                    panel(m,oval(a,.66f,.021f),oval(a,.82f,.021f),oval(b,.82f,.021f),oval(b,.66f,.021f),
                          {1,0,0},led,Lamp,0);
            }
            ribbon(1,k*.65f,k*.75f,y-.007f,y-.004f,.010f,.011f,0,.021f,led,Lamp,true);
        } else {
            float height=suv?.076f:style==BodyStyle::Taxi?.053f:.043f;
            ribbon(1,k*in,k*out,y-.019f,y+.004f,height*.84f,height*.65f,-.003f,.010f,lampCase,CarMetal,2);
            ribbon(1,k*(in+.020f),k*.969f,y+.006f,y+.025f,.007f,.008f,.002f,.018f,led,Lamp,2);
            for(int lamp=0;lamp<2;++lamp) {
                float a=in+.092f+.116f*lamp;
                ribbon(1,k*a,k*(a+.089f),y-.024f,y-.022f,.012f,.012f,0,.018f,led,Lamp,true);
            }
            if(suv)
                ribbon(1,k*.67f,k*.963f,y-.056f,y-.036f,.008f,.010f,0,.017f,led,Lamp,true);
            ribbon(1,k*.947f,k*.984f,y+.006f,y+.018f,.006f,.007f,0,.020f,
                   {.96f,.51f,.12f},Lamp,true);
        }
        if(!mini) {
            lampReturn(1,side,y+.012f,.023f,.21f,lampCase,CarMetal,.010f);
            lampReturn(1,side,y+.020f,.005f,.16f,{.91f,.44f,.14f},Lamp,.017f);
        }
        float backIn=suv?.43f:mini?.55f:.34f;
        float ty=tail-.048f;
        ribbon(-1,k*backIn,k*.969f,ty-.005f,ty+.014f,.043f,suv?.072f:.053f,
               -.006f,.009f,smoke,Rubber,2);
        ribbon(-1,k*(backIn+.021f),k*.957f,ty+.017f,ty+.047f,.009f,.011f,
               -.006f,.017f,red,Lamp,2);
        ribbon(-1,k*(backIn+.125f),k*.946f,ty-.020f,ty-.012f,.008f,.009f,
               -.005f,.017f,red,Lamp,true);
        lampReturn(-1,side,ty+.017f,.039f,.24f,smoke,Rubber,.010f);
        lampReturn(-1,side,ty+.047f,.008f,.18f,red,Lamp,.018f);
        ribbon(-1,k*.63f,k*.85f,low+.080f,low+.095f,.010f,.012f,0,.012f,smoke,Rubber,true);
    }
    if(suv) {
        ribbon(1,-.45f,.45f,front-.082f,front-.082f,.063f,.063f,-.010f,.012f,black,Rubber,true);
        for(int i=-3;i<=3;++i) {
            float u=i*.101f;
            ribbon(1,u-.020f,u+.020f,front-.092f,front-.092f,.044f,.044f,0,.020f,metal,CarMetal,true);
        }
        ribbon(1,-.41f,.41f,front-.034f,front-.034f,.009f,.009f,-.008f,.022f,metal,CarMetal,true);
    }
    if(style==BodyStyle::Fastback)
        ribbon(-1,-.37f,.37f,tail-.031f,tail-.031f,.0055f,.0055f,-.005f,.015f,
               {.57f,.042f,.037f},Lamp,true);
    // Lower rear bumper, recessed reflector pockets and the line of the hatch sill.
    ribbon(-1,-.93f,.93f,low+.012f,low+.012f,.047f,.047f,-.028f,.011f,black,Rubber,true);
    ribbon(-1,-.74f,.74f,low+.033f,low+.033f,.007f,.007f,-.029f,.021f,metal,CarMetal,true);
    for(int i=-2;i<=2;++i) {
        float u=i*.155f;
        ribbon(-1,u-.018f,u+.018f,low-.013f,low-.003f,.021f,.022f,
               0,.018f,{.16f,.18f,.19f},Rubber,true);
    }
    for(int side:{-1,1}) {
        float k=float(side),height=tail-.31f;
        ribbon(-1,0,k*.66f,height,height+.002f,.0045f,.0045f,-.005f,.005f,
               paint*.40f,Rubber,false);
        ribbon(-1,k*.66f,k*.974f,height+.002f,height+.044f,.0045f,.0035f,-.005f,.006f,
               paint*.40f,Rubber,false);
    }
    // The shut lines follow the actual hood and trunk crown. These flush narrow
    // strips make the panel boundaries readable without raised rails on the paint.
    const V seam=paint*.39f;
    const float half=s.length*.5f;
    auto topGap=[&](UV a,UV b,UV c) {
        const int steps=18;
        auto point=[&](float t,float edge) {
            float q=1.f-t;
            float x=q*q*a.u+2*q*t*b.u+t*t*c.u;
            float u=q*q*a.v+2*q*t*b.v+t*t*c.v;
            float dx=q*(b.u-a.u)+t*(c.u-b.u);
            float dz=(q*(b.v-a.v)+t*(c.v-b.v))*.5f*s.width;
            float len=std::sqrt(dx*dx+dz*dz);
            V p=topSurface(s,x-edge*dz/len,u+edge*dx/(.5f*len*s.width));
            p.y+=.0026f;
            return p;
        };
        for(int i=0;i<steps;++i) {
            float a=i/float(steps),b=(i+1)/float(steps);
            panel(m,point(a,-.0032f),point(b,-.0032f),point(b,.0032f),point(a,.0032f),
                  {0,1,0},seam,Rubber,0);
        }
    };
    for(int side:{-1,1}) {
        float k=float(side);
        topGap({s.windshieldBase-.035f,k*.88f},{half-.29f,k*.80f},{half-.18f,k*.748f});
        topGap({half-.18f,k*.748f},{half-.090f,k*.705f},{half-.079f,k*.58f});
        topGap({half-.079f,k*.58f},{half-.055f,k*.33f},{half-.055f,0});
        float rearStart=std::max(-half+.20f,s.rearGlassBase-.035f);
        topGap({rearStart,k*.87f},{-half+.18f,k*.861f},{-half+.133f,k*.79f});
        topGap({-half+.133f,k*.79f},{-half+.080f,k*.71f},{-half+.072f,k*.61f});
        topGap({-half+.072f,k*.61f},{-half+.051f,k*.36f},{-half+.051f,0});
    }
}
void carDetails(Mesh& m,const CarShape& s,V paint,BodyStyle style,int number) {
    const V dark={.07f,.10f,.13f}, chrome={.72f,.75f,.77f};
    carBumpers(m,s,paint,style);
    for(int side:{-1,1}) {
        float sign=float(side);
        // Door divisions follow the body shell, with a slim rocker cap below them.
        float split=(s.roofRear+s.roofFront)*.5f;
        for(int seam=0;seam<3;++seam) {
            float gap=s.axle-s.tire-.071f;
            const float lx[]={-gap,split,gap};
            const float ux[]={-gap-.18f,split+.055f,s.windshieldBase-.06f};
            for(int row=0;row<shoulderRow-1;++row) {
                auto point=[&](int k) {
                    float u=k/float(shoulderRow-1),x=lx[seam]+(ux[seam]-lx[seam])*u;
                    V p=bodyPoint(s,{x,style==BodyStyle::SUV?.238f:.174f},side,k);
                    p.z+=sign*.0035f;
                    return p;
                };
                rod(m,point(row),point(row+1),.0055f,.0055f,paint*.27f,CarPaint,0,5);
            }
        }
        for(float doorX:{split+.17f,split-.59f}) {
            float handleZ=bodyHalfWidth(s,doorX)*.998f*side;
            float handleY=beltHeight(s,doorX)-.083f;
            rod(m,{doorX-.071f,handleY,handleZ},{doorX+.071f,handleY,handleZ},
                .012f,.012f,chrome,CarMetal,0,6);
        }
        float limit=s.axle-s.tire-.077f;
        V p={-limit,.206f,sign*bodyHalfWidth(s,-limit)*.971f};
        V q={limit,.206f,sign*bodyHalfWidth(s,limit)*.971f};
        rod(m,p,q,.016f,.016f,style==BodyStyle::SUV?chrome:paint*.70f,CarMetal,0,6);
        float mirrorX=s.roofFront+.30f;
        V mount=windowPoint(s,mirrorX,sign,.26f);
        V mirror=mount+V{-.041f,.04f,sign*.195f};
        rod(m,mount,mirror,.027f,.035f,dark,Rubber,0,8);
        orb(m,mirror,.108f,.067f,.070f,paint,CarPaint,0,16,8);
        orb(m,mirror+V{-.063f,.002f,0},.051f,.043f,.056f,{.48f,.62f,.67f},CarGlass,0,12,6);
        for(float wheelX:{-s.axle,s.axle})
            carWheel(m,wheelX,sign*bodyHalfWidth(s,wheelX)*.968f,side,s.tire,s.width*.118f);
    }
    carPlate(m,s,1,number);carPlate(m,s,-1,number);
    if(style==BodyStyle::Taxi) {
        float x=(s.roofRear+s.roofFront)*.5f;
        float y=roofPoint(s,x,0).y;
        box(m,x,y-.012f,0,.39f,.115f,.44f,{1.f,.77f,.25f},Lamp);
        // Small dark checks also identify the taxi when seen from the side.
        for(int side:{-1,1}) for(int i=0;i<9;++i) for(int j=0;j<2;++j) {
            if((i+j)%2) continue;
            float x0=-.62f+.10f*i,y0=.62f+.055f*j;
            float z=(s.width*.5f+.001f)*side;
            panel(m,{x0,y0,z},{x0+.095f,y0,z},{x0+.095f,y0+.05f,z},{x0,y0+.05f,z},
                  {0,0,float(side)},dark,Rubber,0);
        }
    }
}

void carLightFootprint(Mesh& m,const CarShape& s) {
    const V road={.20f,.23f,.26f},up={0,1,0};
    const float y=.008f;
    auto shadowPoint=[&](float a,float r){return V{.53f*s.length*r*std::cos(a),y,.65f*s.width*r*std::sin(a)};};
    for(int i=0;i<24;++i) {
        float a=tau*i/24,b=tau*(i+1)/24;
        triangle(m,{0,y,0},shadowPoint(a,.60f),shadowPoint(b,.60f),up,road,AsphaltLight,0,
                 {.72f,0},{.62f,0},{.62f,0});
        for(int k=0;k<2;++k) {
            const float ring[]={.60f,.83f,1.0f}, shade[]={.62f,.28f,0};
            V p=shadowPoint(a,ring[k]),q=shadowPoint(b,ring[k]);
            V r=shadowPoint(b,ring[k+1]),t=shadowPoint(a,ring[k+1]);
            UV u={shade[k],0},v={shade[k+1],0};
            triangle(m,p,q,r,up,road,AsphaltLight,0,u,u,v);
            triangle(m,p,r,t,up,road,AsphaltLight,0,u,v,v);
        }
    }
    // Two beams spread across the lane; the outer vertices match unlit asphalt.
    auto beam=[&](int row,int col,V& pos,UV& uv){
        float t=row/10.f,across=col/16.f*2.f-1.f;
        float width=s.width*(.56f+.43f*t);
        float gap=std::abs(std::abs(width*across)-s.width*(.34f+.07f*t));
        float cone=std::max(0.f,1.f-gap/(.20f+.77f*t));
        float edge=std::max(0.f,1.f-std::pow(std::abs(across),4.f));
        float power=std::min(1.f,t*8.f)*(1.f-t)*(1.f-t);
        pos={s.length*.50f+t*5.4f,y+.001f,width*across};
        uv={0,cone*edge*power*.57f};
    };
    for(int row=0;row<10;++row) for(int col=0;col<16;++col) {
        V p,q,r,t;UV a,b,c,d;
        beam(row,col,p,a);beam(row+1,col,q,b);beam(row+1,col+1,r,c);beam(row,col+1,t,d);
        triangle(m,p,q,r,up,road,AsphaltLight,0,a,b,c);
        triangle(m,p,r,t,up,road,AsphaltLight,0,a,c,d);
    }
}

void addCar(Mesh& m,BodyStyle style,V paint,float x,float z,int heading,int number) {
    const std::size_t first=m.size();
    CarShape shape=carShape(style);
    carLightFootprint(m,shape);
    carBody(m,shape,paint,style);
    carCabin(m,shape,paint,style);
    carDetails(m,shape,paint,style,number);
    // Both horizontal axes change sign on the return lane: the chassis rotates as a unit.
    for(std::size_t i=first;i<m.size();++i) {
        Vertex& v=m[i];
        v.x=x+heading*.60f*v.x; v.z=z+heading*.60f*v.z; v.y=.050f+.60f*v.y;
        v.nx*=heading;v.nz*=heading;
    }
}

void boulevardFleet(Mesh& m) {
    const BodyStyle styles[]={BodyStyle::Fastback,BodyStyle::SUV,BodyStyle::Taxi,BodyStyle::Compact};
    const V colors[]={{.47f,.74f,.85f},{.32f,.40f,.49f},{.99f,.61f,.14f},{.83f,.27f,.20f},
                      {.78f,.80f,.77f},{.17f,.47f,.46f},{.56f,.19f,.23f},{.27f,.39f,.66f}};
    const auto& spots=carSpots();
    for(std::size_t i=0;i<spots.size();++i)
        addCar(m,styles[i],colors[i],spots[i].x,spots[i].z,spots[i].heading,214+int(i)*97);
    // Separate cars across both service roads. Each road follows right-hand traffic.
    const float north[]={-88,-69,-54,-31,-14,2,27,42,60,79,96};
    const float south[]={-84,-63,-47,-25,-9,48,69,91};
    int index=0;
    auto lane=[&](float roadCenter,float carX) {
        int heading=(index%2==0)?1:-1;
        float laneZ=roadCenter+1.45f*heading;
        addCar(m,styles[(index+1)%4],colors[(index*3+4)%8],carX,laneZ,heading,104+index*37);
        ++index;
    };
    for(float carX:north) lane(-17.4f,carX);
    for(float carX:south) lane(17.4f,carX);
}

void landscape(Mesh& m) {
    const V lawn={.24f,.38f,.23f}, path={.68f,.66f,.61f}, asphalt={.20f,.23f,.26f};
    // Keep the city connected to the horizon when the camera sees past the main district.
    box(m,0,-1.24f,0,1600,1.0f,1600,{.30f,.34f,.33f},Stone);
    const V outerStreet={.22f,.25f,.27f};
    for(float z:{-72.f,-102.f,-146.f,62.f,84.f})
        box(m,0,-.232f,z,410,.026f,3.1f,outerStreet,Stone);
    for(float x:{-183.f,-145.f,-122.f,-95.f,-36.f,33.f,88.f,124.f,156.f,189.f}) {
        for(int side:{-1,1})
            box(m,x,-.230f,side*111.f,2.8f,.031f,111.f,outerStreet,Stone);
    }
    // Subdued outer districts use progressively smaller silhouettes behind the landmarks.
    for(int row=0;row<4;++row) for(int col=-11;col<=11;++col) {
        unsigned seed=unsigned(23*row+col+12)*941u+143u;
        if(seed%8==0) continue;
        float x=col*15.f+float(seed%5)-2.f;
        float z=-62.f-row*22.f-float((seed/7)%7);
        float w=5.0f+float((seed/3)%5), d=4.0f+float((seed/5)%4);
        float h=3.2f+float((seed/11)%6);
        V concrete={.39f+((seed/13)%4)*.025f,.44f,.46f};
        box(m,x,-.23f,z,w,h,d,concrete,seed%3==0?Glass:Stone);
        box(m,x,h-.23f,z,w+.12f,.16f,d+.12f,{.42f,.47f,.48f},Stone);
    }
    for(int side:{-1,1}) for(int col=0;col<5;++col) {
        float x=side*(80.f+col*13.f);
        for(float z:{69.f,91.f})
            box(m,x,-.22f,z,6.5f,2.2f+float(col%3),4.2f,{.39f,.44f,.44f},Stone);
    }
    box(m,0,-.90f,0,238,.90f,118,lawn,Grass);
    // Parallel service roads frame the pedestrian axis and the central gardens.
    for(int side:{-1,1}) {
        float z=side*17.4f;
        box(m,0,.010f,z,217,.040f,5.9f,asphalt,Asphalt);
        for(int i=-104;i<=104;i+=6)
            box(m,float(i),.052f,z,2.9f,.012f,.09f,{.79f,.76f,.67f},Stone);
        box(m,0,.014f,z+side*3.7f,218,.080f,1.20f,path,Paving);
    }
    for(float x:{-95.f,-36.f,33.f,88.f}) {
        // These cross streets stop at the garden on either side.
        for(int side:{-1,1})
            box(m,x,.012f,side*36.8f,3.8f,.033f,32.0f,asphalt,Stone);
    }
    box(m,-2,.026f,0,153,.075f,4.8f,path,Paving);
    for(float z:{-9.1f,9.1f})
        box(m,-5,.022f,z,128,.065f,2.1f,path,Paving);
    for(float x:{-49.f,-26.f,0.f,25.f,48.f,61.f})
        box(m,x,.035f,0,2.0f,.081f,20.f,path,Paving);
    for(float x:{-45.f,-18.f,18.f,38.f}) {
        for(int side:{-1,1}) {
            float z=side*5.52f;
            if(x==18.f || x==-45.f) {
                pool(m,x,z,12.5f,2.75f);
            } else {
                box(m,x,.11f,z,12.8f,.18f,2.62f,{.70f,.68f,.62f},Stone);
                box(m,x,.296f,z,12.42f,.18f,2.24f,{.24f,.42f,.23f},Grass);
                for(int i=-4;i<=4;++i) {
                    float cx=x+i*1.25f;
                    orb(m,{cx,.48f,z},.46f,.44f,.43f,i%2?V{.43f,.34f,.29f}:V{.47f,.28f,.33f},Stone,0,8,5);
                }
            }
        }
    }
    for(int i=-9;i<=9;++i) for(int side:{-1,1}) {
        float x=i*6.5f, z=side*11.4f;
        if ((x>40 && x<55) || i==0) continue;
        float h=2.87f+float((i+11)%3)*.34f;
        tree(m,x,z,h,(i+10)*3+side+1);
    }
    for(int i=-10;i<=10;++i) for(int side:{-1,1}) {
        float x=i*5.8f;
        if (std::abs(x)<5) continue;
        lantern(m,x,side*3.2f);
    }
    for(int i=-9;i<=9;++i) {
        float x=i*10.1f;
        for(int side:{-1,1}) {
            float z=side*22.65f;
            if(x>75 && side>0) continue;
            tree(m,x,z,3.4f+float((i+13)%3)*.38f,unsigned(i+12));
        }
    }
}

} // namespace

const std::vector<CarSpot>& carSpots() {
    static const std::vector<CarSpot> spots={
        {"Fastback",15.f,18.85f,1},{"Crossover SUV",22.f,15.95f,-1},
        {"City taxi",10.f,15.95f,-1},{"Compact",4.f,18.85f,1}
    };
    return spots;
}

const std::vector<Landmark>& landmarks() {
    static const std::vector<Landmark> locations={
        {1,"Baiterek",0,17,0,17},
        {2,"Akorda",74,9,0,18},
        {3,"Khan Shatyr",-72,11,0,20},
        {4,"Palace of Peace and Reconciliation",91,9,31,16},
        {5,"Golden Towers",48,9,0,14},
        {6,"Emerald Quarter",-17,12,-27,16},
        {7,"City Blocks",10,6,26,12}
    };
    return locations;
}

std::vector<Vertex> buildScene(std::size_t* cityVertexCount, std::size_t* staticVertexCount,
                               std::size_t* playerVertexStart) {
    Mesh m;
    m.reserve(230000);
    landscape(m);
    cityBlocks(m);
    baiterek(m);
    akorda(m);
    khanShatyr(m);
    pyramid(m);
    goldenTowers(m);
    emerald(m);
    if(cityVertexCount) *cityVertexCount=m.size();
    boulevardFleet(m);
    if(staticVertexCount) *staticVertexCount=m.size();
    addCar(m,BodyStyle::Fastback,{.86f,.12f,.10f},0,0,1,777);
    if(playerVertexStart) *playerVertexStart=m.size();
    addCar(m,BodyStyle::SUV,{.10f,.35f,.85f},0,0,1,101);
    return m;
}

} // namespace astana
