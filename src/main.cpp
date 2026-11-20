#include "renderer.hpp"
#include "views.hpp"
#ifdef __APPLE__
#include "glut_glfw.hpp"
#else
#include <GL/freeglut.h>
#endif
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

namespace {
using namespace astana;
Renderer renderer;
View state;
Camera flightFrom,flightTo;
float flightTime=1, goalDaylight=0;
bool cycle=false, pressed=false, dragged=false, closing=false;
int lastX=0,lastY=0, downX=0,downY=0,previousMs=0;
int activeCar=-1;
float cycleTime=0;

void title() {
    std::ostringstream s;
    s<<"Astana Night 3D  |  1-7 landmarks   0 overview   B boulevard   C cars   N day/night   "
      <<"G wire   J AA   F fog";
    if(activeCar>=0) s<<"  |  "<<carSpots()[activeCar].name;
    for(const auto& l:landmarks()) if(l.id==state.selected) s<<"  |  "<<l.name;
    glutSetWindowTitle(s.str().c_str());
}
void display() { renderer.render(state); glutSwapBuffers(); }
void resize(int w,int h) { renderer.resize(w,h); glutPostRedisplay(); }
void beginFlight(Camera target) {
    flightFrom=state.cam; flightTo=target; flightTime=0;
}
void timer(int) {
    if(closing || glutGetWindow()==0) return;
    const int now=glutGet(GLUT_ELAPSED_TIME);
    const float dt=std::min(.10f,(now-previousMs)/1000.f); previousMs=now;
    state.time+=dt;
    if(flightTime<1) {
        flightTime=std::min(1.f,flightTime+dt/1.35f);
        state.cam=fly(flightFrom,flightTo,flightTime);
    }
    if(cycle) {
        cycleTime+=dt;
        state.daylight=.5f-.5f*std::cos(cycleTime*(2*pi/20));
    } else {
        const float maxStep=dt*.72f;
        state.daylight+=clamp(goalDaylight-state.daylight,-maxStep,maxStep);
    }
    glutPostRedisplay(); glutTimerFunc(16,timer,0);
}
float carClearance(Vec3 direction) {
    // Keep the orbiting eye outside the longest body, plus the near clipping plane.
    const float nose=1.57f/std::max(std::abs(direction.x),.001f);
    const float roof=.66f/std::max(std::abs(direction.y),.001f);
    const float side=.68f/std::max(std::abs(direction.z),.001f);
    return std::min(nose,std::min(roof,side))+.20f;
}
void orbit(float dx,float dy) {
    flightTime=1;
    const Vec3 old=state.cam.eye-state.cam.target;
    const float distance=length(old);
    const float yaw=std::atan2(old.x,old.z)-dx*.006f;
    const float pitch=clamp(std::asin(clamp(old.y/distance,-1,1))+dy*.006f,-.32f,1.43f);
    const Vec3 n={std::sin(yaw)*std::cos(pitch),std::sin(pitch),std::cos(yaw)*std::cos(pitch)};
    const float orbitDistance=activeCar>=0 ? std::max(distance,carClearance(n)) : distance;
    state.cam.eye=state.cam.target+n*orbitDistance;
    state.cam.eye.y=std::max(activeCar>=0 ? .27f : 1.1f,state.cam.eye.y);
}
void move(float forward,float right,float vertical) {
    flightTime=1;
    Vec3 dir=state.cam.target-state.cam.eye; dir.y=0;
    dir=unit(dir);
    Vec3 offset=dir*forward+cross(dir,{0,1,0})*right+Vec3{0,vertical,0};
    if(activeCar>=0) offset=offset*.20f;
    const float floorHeight=activeCar>=0 ? .27f : 1.1f;
    offset.y=std::max(floorHeight-state.cam.eye.y,offset.y);
    state.cam.eye=state.cam.eye+offset; state.cam.target=state.cam.target+offset;
}
void key(unsigned char c,int,int) {
    if(c>='1' && c<='7') { activeCar=-1; beginFlight(focus(c-'0')); title(); return; }
    switch(c) {
        case '0': activeCar=-1; beginFlight(overview()); break;
        case 'b': case 'B': activeCar=-1; beginFlight(street()); break;
        case 'c': case 'C':
            activeCar=(activeCar+1)%static_cast<int>(carSpots().size());
            state.selected=0;
            beginFlight(carView(activeCar));
            break;
        case 'n': case 'N': {
            // A stopped cycle can leave the light anywhere between its endpoints.
            const float reference=cycle ? state.daylight : goalDaylight;
            goalDaylight=reference<.5f ? 1.f : 0.f;
            cycle=false;
            break;
        }
        case 't': case 'T':
            cycle=!cycle;
            if(cycle) cycleTime=std::acos(clamp(1-2*state.daylight,-1,1))*20/(2*pi);
            else goalDaylight=state.daylight;
            break;
        case 'g': case 'G': state.wire=!state.wire; break;
        case 'j': case 'J': state.ssaa=!state.ssaa; break;
        case 'f': case 'F': state.fog=state.fog>0?0:.85f; break;
        case 'w': case 'W': move(2,0,0); break;
        case 's': case 'S': move(-2,0,0); break;
        case 'a': case 'A': move(0,-2,0); break;
        case 'd': case 'D': move(0,2,0); break;
        case 'q': case 'Q': move(0,0,-2); break;
        case 'e': case 'E': move(0,0,2); break;
        case 'r': case 'R':
            state=View(); state.cam=overview(); cycle=false; flightTime=1; goalDaylight=0; activeCar=-1; break;
        case 27: closing=true; glutLeaveMainLoop(); return;
    }
    title(); glutPostRedisplay();
}
void mouse(int button,int action,int x,int y) {
    if(button!=GLUT_LEFT_BUTTON) return;
    if(action==GLUT_DOWN) {
        pressed=true; dragged=false; downX=lastX=x; downY=lastY=y;
    } else if(pressed) {
        if(!dragged) {state.selected=renderer.pick(state,x,y); title();}
        pressed=false; glutPostRedisplay();
    }
}
void motion(int x,int y) {
    if(!pressed) return;
    if(!dragged && std::abs(x-downX)+std::abs(y-downY)<4) return;
    dragged=true; orbit(float(x-lastX),float(y-lastY)); lastX=x; lastY=y; glutPostRedisplay();
}
void wheel(int,int direction,int,int) {
    flightTime=1;
    const Vec3 offset=state.cam.eye-state.cam.target;
    const Vec3 lookBack=unit(offset);
    const float minimumDistance=activeCar>=0 ? carClearance(lookBack) : 3.f;
    state.cam.eye=state.cam.target+lookBack*clamp(length(offset)*(direction>0?.88f:1.14f),minimumDistance,280);
    state.cam.eye.y=std::max(activeCar>=0 ? .27f : 1.1f,state.cam.eye.y); glutPostRedisplay();
}
void closeWindow() { closing=true; }

struct Options {
    std::string mode,dir;
    int w=1080,h=640,fps=18,first=0,last=-1;
    bool help=false;
};
int number(const std::string& arg,int lo,int hi) {
    char* end=nullptr; const long n=std::strtol(arg.c_str(),&end,10);
    if(!end || end==arg.c_str() || *end || n<lo || n>hi)
        throw std::runtime_error("Invalid numeric option: "+arg);
    return int(n);
}
Options parse(int argc,char** argv) {
    Options o;
    for(int i=1;i<argc;++i) {
        const std::string arg=argv[i];
        if(arg=="--help" || arg=="-h") {o.help=true; continue;}
        if(i+1==argc) throw std::runtime_error("Missing value for "+arg);
        const std::string value=argv[++i];
        if(arg=="--shots" || arg=="--tour") {
            if(!o.mode.empty()) throw std::runtime_error("Use a single capture mode");
            o.mode=arg; o.dir=value;
        } else if(arg=="--width") o.w=number(value,1,8192);
        else if(arg=="--height") o.h=number(value,1,8192);
        else if(arg=="--fps") o.fps=number(value,1,120);
        else if(arg=="--first") o.first=number(value,0,999999);
        else if(arg=="--last") o.last=number(value,0,999999);
        else throw std::runtime_error("Unknown option: "+arg);
    }
    if(o.mode=="--tour" && (o.first>=48*o.fps || (o.last>=0 && o.last<o.first)))
        throw std::runtime_error("Tour frame range is outside the 48-second timeline");
    return o;
}
void makeDir(const std::string& dir) {
#ifdef _WIN32
    const int result=_mkdir(dir.c_str());
#else
    const int result=mkdir(dir.c_str(),0755);
#endif
    if(result!=0 && errno!=EEXIST) throw std::runtime_error("Cannot create directory: "+dir);
}
void save(const std::string& path,const View& v) {
    renderer.render(v); renderer.saveFrame(path.c_str());
    const GLenum e=glGetError();
    if(e!=GL_NO_ERROR) throw std::runtime_error("OpenGL error in capture: "+std::to_string(e));
}
int clickLandmark(const View& v,int id,Vec3 target,std::ostream& log) {
    const auto xy=renderer.findClick(v,id,target);
    if(xy.first<0) throw std::runtime_error("Landmark hidden in picking test: "+std::to_string(id));
    const unsigned result=renderer.pick(v,xy.first,xy.second);
    log<<"pick id="<<id<<" x="<<xy.first<<" y="<<xy.second<<" read="<<result<<"\n";
    if(static_cast<int>(result)!=id) throw std::runtime_error("Unexpected ID read from pixel");
    return static_cast<int>(result);
}
void capture(const Options& o,std::size_t vertices) {
    makeDir(o.dir);
    const std::string base=o.dir+"/";
    std::ofstream log(base+(o.mode=="--shots"?"capture.log":"tour-"+std::to_string(o.first)+".log"));
    if(!log) throw std::runtime_error("Cannot open capture log");
    log<<"GL vendor: "<<glGetString(GL_VENDOR)<<"\nGL renderer: "<<glGetString(GL_RENDERER)
       <<"\nOpenGL: "<<glGetString(GL_VERSION)<<"\nGLSL: "<<glGetString(GL_SHADING_LANGUAGE_VERSION)
       <<"\nSize: "<<o.w<<" x "<<o.h<<"\nSSAA scale: "<<renderer.scale()
       <<" x "<<renderer.scale()<<"\nTriangles: "<<vertices/3<<"\n";
    if(o.mode=="--shots") {
        View v; v.cam=overview();
        auto shot=[&](const std::string& name) { save(base+name+".ppm",v); log<<"wrote "<<name<<"\n"; };
        shot("overview-night"); v.daylight=1; shot("overview-day");
        v.daylight=.43f; shot("overview-dusk"); v.daylight=0;
        shot("fog-on"); v.fog=0; shot("fog-off"); v.fog=.85f;
        v.cam={{-42,8.4f,26},{1,16,-1}}; shot("hero");
        for(const auto& p:landmarks()) {
            v.cam=focus(p.id); shot("landmark-"+std::to_string(p.id));
            clickLandmark(v,p.id,{p.x,p.y,p.z},log);
        }
        v.cam=focus(1); shot("unselected");
        v.selected=clickLandmark(v,1,{0,29,0},log); shot("selected"); v.selected=0;
        v.wire=true; shot("wireframe"); v.wire=false;
        v.ssaa=false; shot("aa-off"); v.ssaa=true; shot("aa-on");
        v.cam=street(.9f); shot("boulevard");
        v.cam=carView(0); shot("car-hero");
        v.daylight=1; shot("car-day"); v.daylight=0;
        v.cam=carDetail(); shot("car-detail");
        v.cam=carView(1); shot("car-suv");
        v.cam=carView(2); shot("car-taxi");
        v.cam=carView(3); shot("car-compact");
        // Aim above the tallest landmark so the middle pixel is guaranteed to be sky.
        v.cam={{0,70,0},{0,100,-2}};
        const unsigned bg=renderer.pick(v,o.w/2,o.h/2);
        log<<"Empty sky click: read="<<bg<<"\n";
        if(bg!=0) throw std::runtime_error("The empty sky must return object ID 0");
    } else {
        const int last=o.last<0?48*o.fps-1:std::min(o.last,48*o.fps-1);
        int selectedId=0;
        for(int i=o.first;i<=last;++i) {
            const float t=float(i)/o.fps;
            View v=tourFrame(t);
            if(t>=29 && t<32) {
                if(!selectedId) selectedId=clickLandmark(v,1,{0,29,0},log);
                v.selected=selectedId;
            }
            std::ostringstream filename;
            filename<<base<<"frame-"<<std::setfill('0')<<std::setw(5)<<i<<".ppm";
            save(filename.str(),v);
        }
        log<<"Frames: "<<o.first<<" through "<<last<<" at "<<o.fps<<" fps\n";
    }
    log<<"Completed without an OpenGL error.\n";
}
}
int main(int argc,char** argv) {
    try {
        const Options opts=parse(argc,argv);
        if(opts.help) {
            std::cout<<"Astana Night 3D\nUsage: astana3d [--shots DIR | --tour DIR] "
                       "[--width W --height H --fps N --first FRAME --last FRAME]\n"
                       "1-7 landmarks, 0 overview, B boulevard, C next car, N day/night, T cycle, G wireframe, "
                       "J antialiasing, F fog, WASD move, Q/E down/up, drag orbit, click pick, "
                       "wheel zoom, R reset, Esc exit.\n";
            return 0;
        }
        int glutArgc=1; char* glutArgv[]={argv[0],nullptr};
        glutInit(&glutArgc,glutArgv);
        glutInitContextVersion(3,3); glutInitContextProfile(GLUT_CORE_PROFILE);
        glutInitDisplayMode(GLUT_RGBA|GLUT_DOUBLE|GLUT_DEPTH);
        glutInitWindowSize(opts.mode.empty()?opts.w:64,opts.mode.empty()?opts.h:64);
        glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE,GLUT_ACTION_GLUTMAINLOOP_RETURNS);
        const int window=glutCreateWindow("Astana Night 3D");
        if(!opts.mode.empty()) glutHideWindow();
        glewExperimental=GL_TRUE;
        const GLenum glewResult=glewInit();
        if(glewResult!=GLEW_OK) throw std::runtime_error(reinterpret_cast<const char*>(glewGetErrorString(glewResult)));
        while(glGetError()!=GL_NO_ERROR) {} // GLEW can probe a removed core-profile entry.
        if(!GLEW_VERSION_3_3) throw std::runtime_error("OpenGL 3.3 or later is required");
        std::size_t cityVertices=0;
        std::vector<Vertex> scene=buildScene(&cityVertices);
        const auto vertices=scene.size();
        renderer.init(scene,cityVertices,!opts.mode.empty()); std::vector<Vertex>().swap(scene);
        renderer.resize(opts.w,opts.h);
        if(!opts.mode.empty()) {
            capture(opts,vertices); renderer.shutdown(); glutDestroyWindow(window); return 0;
        }
        state.cam=overview(); title(); previousMs=glutGet(GLUT_ELAPSED_TIME);
        glutDisplayFunc(display); glutReshapeFunc(resize); glutKeyboardFunc(key);
        glutCloseFunc(closeWindow);
        glutMouseFunc(mouse); glutMotionFunc(motion); glutMouseWheelFunc(wheel);
        glutTimerFunc(16,timer,0); glutMainLoop();
        // Freeglut may destroy the window before returning from its event loop.
        // The final context teardown also releases all remaining GPU resources.
    } catch(const std::exception& e) { std::cerr<<"Astana Night 3D: "<<e.what()<<"\n"; return 1; }
    return 0;
}
