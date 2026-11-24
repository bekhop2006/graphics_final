#include "renderer.hpp"
#include "views.hpp"
#ifdef __APPLE__
#include "glut_glfw.hpp"
#else
#include <GL/freeglut.h>
#endif
#include <cctype>
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
bool follow=false; // The camera chases the moving car.
bool driving=false; // W/A/S/D drive the blue car and the camera chases it.
float carSpeed=0, carYaw=0, odometer=0;
// Free camera: mouse look plus held-key flight through the whole map.
bool freeCam=false, boost=false, held[256]={};
float yaw=0, pitch=0;
int winW=1080, winH=640;

void title() {
    std::ostringstream s;
    s<<"Astana Night 3D  |  1-7 landmarks   0 overview   B boulevard   C cars   N day/night   "
      <<"G wire   J AA   F fog   K Akorda   V free cam   M moving car   P drive";
    if(driving) s.str("Astana Night 3D  |  DRIVING: W gas   S brake/reverse   A/D steer   Shift faster   P exit");
    if(freeCam) s.str("Astana Night 3D  |  FREE CAM: WASD move   mouse look   Space/E up   Q down   "
                      "Shift fast   click select   V/Esc exit");
    if(activeCar>=0) s<<"  |  "<<carSpots()[activeCar].name;
    if(follow) s<<"  |  Following the moving car";
    for(const auto& l:landmarks()) if(l.id==state.selected) s<<"  |  "<<l.name;
    glutSetWindowTitle(s.str().c_str());
}
void crosshair() {
    // Two scissored clears draw the aiming mark without another shader.
    GLfloat old[4]; glGetFloatv(GL_COLOR_CLEAR_VALUE,old);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,0); glViewport(0,0,winW,winH);
    glEnable(GL_SCISSOR_TEST); glClearColor(1,1,1,.9f);
    const int cx=winW/2, cy=winH/2, len=std::max(6,winH/90), thick=std::max(2,winH/400);
    glScissor(cx-len,cy-thick/2,2*len,thick); glClear(GL_COLOR_BUFFER_BIT);
    glScissor(cx-thick/2,cy-len,thick,2*len); glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST); glClearColor(old[0],old[1],old[2],old[3]);
}
// 5x7 glyphs for the driving readout. Each row holds five bits, leftmost pixel first.
const char* const glyphChars="0123456789.KMH/ ";
const unsigned char glyphs[][7]={
    {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},{31,2,4,2,1,17,14},
    {2,6,10,18,31,2,2},{31,16,30,1,1,17,14},{6,8,16,30,17,17,14},{31,1,2,4,8,8,8},
    {14,17,17,14,17,17,14},{14,17,17,15,1,2,12},{0,0,0,0,0,12,12},{17,18,20,24,20,18,17},
    {17,27,21,21,17,17,17},{17,17,17,31,17,17,17},{1,1,2,4,8,16,16},{0,0,0,0,0,0,0}};
void text(const std::string& s,int x,int top,int px) {
    for(char ch:s) {
        const char* at=std::strchr(glyphChars,ch);
        if(at && *at) for(int row=0;row<7;++row) for(int col=0;col<5;++col)
            if((glyphs[at-glyphChars][row]>>(4-col))&1) {
                glScissor(x+col*px,top-(row+1)*px,px,px); glClear(GL_COLOR_BUFFER_BIT);
            }
        x+=6*px;
    }
}
void hud() {
    // Odometer and speedometer in the top-left corner, drawn with scissored clears like the crosshair.
    // The cars are built at 0.6 scale, so one scene unit is 1/0.6 metres.
    std::ostringstream odo,speed;
    odo<<std::fixed<<std::setprecision(2)<<odometer/600<<" KM";
    speed<<int(std::abs(carSpeed)*6+.5f)<<" KM/H";
    GLfloat old[4]; glGetFloatv(GL_COLOR_CLEAR_VALUE,old);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,0); glViewport(0,0,winW,winH);
    glEnable(GL_SCISSOR_TEST);
    const int px=std::max(2,winH/180), pad=3*px, line=10*px;
    const int chars=int(std::max(odo.str().size(),speed.str().size()));
    glClearColor(.04f,.05f,.09f,1);
    glScissor(pad,winH-3*pad-17*px,2*pad+chars*6*px-px,2*pad+17*px); glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(1,.85f,.35f,1); text(odo.str(),2*pad,winH-2*pad,px);
    glClearColor(.80f,.90f,1,1); text(speed.str(),2*pad,winH-2*pad-line,px);
    glDisable(GL_SCISSOR_TEST); glClearColor(old[0],old[1],old[2],old[3]);
}
void display() {
    renderer.render(state);
    if(freeCam) crosshair();
    if(driving) hud();
    glutSwapBuffers();
}
void resize(int w,int h) { winW=w; winH=h; renderer.resize(w,h); glutPostRedisplay(); }
Vec3 freeDirection() { return {std::cos(pitch)*std::sin(yaw),std::sin(pitch),std::cos(pitch)*std::cos(yaw)}; }
void aimFree() { state.cam.target=state.cam.eye+freeDirection()*10; }
void setFreeCam(bool on) {
    if(on==freeCam) return;
    freeCam=on; boost=false; pressed=false; std::fill(held,held+256,false);
    glutSetCursor(on?GLUT_CURSOR_NONE:GLUT_CURSOR_INHERIT);
    if(!on) return;
    flightTime=1; activeCar=-1; follow=false; driving=false;
    const Vec3 d=unit(state.cam.target-state.cam.eye);
    yaw=std::atan2(d.x,d.z); pitch=std::asin(clamp(d.y,-1,1));
    state.cam.verticalFov=60*pi/180; aimFree();
    glutWarpPointer(winW/2,winH/2);
}
void look(int x,int y) {
    const int cx=winW/2, cy=winH/2;
    if(x==cx && y==cy) return;
    yaw-=(x-cx)*.002f; pitch=clamp(pitch-(y-cy)*.002f,-1.55f,1.55f);
    aimFree(); glutWarpPointer(cx,cy); glutPostRedisplay();
}
void beginFlight(Camera target) {
    flightFrom=state.cam; flightTo=target; flightTime=0; follow=false; driving=false;
}
void steer(float dt,float throttle,float turn) {
    // Gas and brake change the speed, rolling slows it, and steering grows with speed.
    const float top=boost?30.f:18.f;
    if(throttle!=0) carSpeed+=throttle*(throttle*carSpeed<0?24.f:9.f)*dt;
    else carSpeed-=std::copysign(std::min(std::abs(carSpeed),6.f*dt),carSpeed);
    carSpeed=clamp(carSpeed,-6,top);
    carYaw+=turn*1.5f*clamp(carSpeed/5,-1,1)*dt;
    state.playerForward={std::cos(carYaw),0,std::sin(carYaw)};
    Vec3& p=state.playerPosition;
    p=p+state.playerForward*(carSpeed*dt);
    odometer+=std::abs(carSpeed)*dt;
    p.x=clamp(p.x,-190,190); p.z=clamp(p.z,-190,190);
}
void move(float forward,float right,float vertical);
void timer(int) {
    if(closing || glutGetWindow()==0) return;
    const int now=glutGet(GLUT_ELAPSED_TIME);
    const float dt=std::min(.10f,(now-previousMs)/1000.f); previousMs=now;
    state.time+=dt;
    drive(state);
    // Held keys move continuously; macOS does not auto-repeat held letter keys.
    const float forward=float(held['w'])-held['s'], right=float(held['d'])-held['a'];
    const float up=float(held[' '] || held['e'])-held['q'];
    if(driving) steer(dt,forward,right);
    if(follow || driving) {
        // Aim the flight at the car's current pose, then trail it with a short lag.
        const Camera chase=driving ? chaseView(state.playerPosition,state.playerForward)
                                   : chaseView(state.carPosition,state.carForward);
        if(flightTime<1) flightTo=chase;
        else state.cam=blend(state.cam,chase,1-std::exp(-dt*5));
    }
    if(flightTime<1) {
        flightTime=std::min(1.f,flightTime+dt/1.35f);
        state.cam=fly(flightFrom,flightTo,flightTime);
    }
    const float step=(boost?75.f:25.f)*dt;
    if(freeCam) {
        const Vec3 f=freeDirection(), r=unit(cross(f,{0,1,0}));
        const Vec3 v=f*forward+r*right+Vec3{0,up,0};
        if(length(v)>0) {
            state.cam.eye=state.cam.eye+unit(v)*step;
            state.cam.eye.y=std::max(.5f,state.cam.eye.y); aimFree();
        }
    } else if(!driving && (forward || right || up)) move(forward*step,right*step,up*step);
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
    flightTime=1; follow=false;
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
    flightTime=1; follow=false;
    Vec3 dir=state.cam.target-state.cam.eye; dir.y=0;
    dir=unit(dir);
    Vec3 offset=dir*forward+cross(dir,{0,1,0})*right+Vec3{0,vertical,0};
    if(activeCar>=0) offset=offset*.20f;
    const float floorHeight=activeCar>=0 ? .27f : 1.1f;
    offset.y=std::max(floorHeight-state.cam.eye.y,offset.y);
    state.cam.eye=state.cam.eye+offset; state.cam.target=state.cam.target+offset;
}
void key(unsigned char c,int,int) {
    const unsigned char k=static_cast<unsigned char>(std::tolower(c));
    if(k=='v') { setFreeCam(!freeCam); title(); glutPostRedisplay(); return; }
    if(k==' ' || std::strchr("wasdqe",k)) { held[k]=true; return; }
    if(freeCam) {
        if(c==27) { setFreeCam(false); title(); return; }
        if((c>='0' && c<='7') || k=='b' || k=='c' || k=='k' || k=='m' || k=='p' || k=='r') setFreeCam(false);
    }
    if(k=='k') { activeCar=-1; beginFlight(focus(2)); title(); return; }
    if(c>='1' && c<='7') { activeCar=-1; beginFlight(focus(c-'0')); title(); return; }
    switch(c) {
        case '0': activeCar=-1; beginFlight(overview()); break;
        case 'b': case 'B': activeCar=-1; beginFlight(street()); break;
        case 'c': case 'C':
            activeCar=(activeCar+1)%static_cast<int>(carSpots().size());
            state.selected=0;
            beginFlight(carView(activeCar));
            break;
        case 'm': case 'M':
            activeCar=-1; beginFlight(chaseView(state.carPosition,state.carForward)); follow=true;
            break;
        case 'p': case 'P':
            if(driving) { driving=false; break; }
            activeCar=-1; state.selected=0;
            beginFlight(chaseView(state.playerPosition,state.playerForward)); driving=true;
            carSpeed=0; carYaw=std::atan2(state.playerForward.z,state.playerForward.x);
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
        case 'r': case 'R':
            state=View(); state.cam=overview(); cycle=false; flightTime=1; goalDaylight=0; activeCar=-1; follow=false; driving=false; odometer=0; break;
        case 27: closing=true; glutLeaveMainLoop(); return;
    }
    title(); glutPostRedisplay();
}
void keyUp(unsigned char c,int,int) { held[std::tolower(c)]=false; }
void special(int k,int,int) { if(k==GLUT_KEY_SHIFT_L || k==GLUT_KEY_SHIFT_R) boost=true; }
void specialUp(int k,int,int) { if(k==GLUT_KEY_SHIFT_L || k==GLUT_KEY_SHIFT_R) boost=false; }
void mouse(int button,int action,int x,int y) {
    if(button!=GLUT_LEFT_BUTTON) return;
    if(freeCam) {
        if(action==GLUT_DOWN) { state.selected=renderer.pick(state,winW/2,winH/2); title(); glutPostRedisplay(); }
        return;
    }
    if(action==GLUT_DOWN) {
        pressed=true; dragged=false; downX=lastX=x; downY=lastY=y;
    } else if(pressed) {
        if(!dragged) {state.selected=renderer.pick(state,x,y); title();}
        pressed=false; glutPostRedisplay();
    }
}
void motion(int x,int y) {
    if(freeCam) { look(x,y); return; }
    if(!pressed || driving) return; // The chase camera owns the view while driving.
    if(!dragged && std::abs(x-downX)+std::abs(y-downY)<4) return;
    dragged=true; orbit(float(x-lastX),float(y-lastY)); lastX=x; lastY=y; glutPostRedisplay();
}
void passive(int x,int y) { if(freeCam) look(x,y); }
void wheel(int,int direction,int,int) {
    if(freeCam || driving) return;
    flightTime=1; follow=false;
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
                       "J antialiasing, F fog, K Akorda, V free camera, M follow the moving car, P drive the blue car with WASD, WASD move, Space up, Shift fast, Q/E down/up, drag orbit, click pick, "
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
        std::size_t cityVertices=0, staticVertices=0, playerVertices=0;
        std::vector<Vertex> scene=buildScene(&cityVertices,&staticVertices,&playerVertices);
        const auto vertices=scene.size();
        renderer.init(scene,cityVertices,staticVertices,playerVertices,!opts.mode.empty()); std::vector<Vertex>().swap(scene);
        renderer.resize(opts.w,opts.h);
        if(!opts.mode.empty()) {
            capture(opts,vertices); renderer.shutdown(); glutDestroyWindow(window); return 0;
        }
        state.cam=overview(); title(); previousMs=glutGet(GLUT_ELAPSED_TIME);
        glutDisplayFunc(display); glutReshapeFunc(resize); glutKeyboardFunc(key);
        glutCloseFunc(closeWindow);
        glutIgnoreKeyRepeat(1); glutKeyboardUpFunc(keyUp); glutSpecialFunc(special); glutSpecialUpFunc(specialUp);
        glutMouseFunc(mouse); glutMotionFunc(motion); glutPassiveMotionFunc(passive); glutMouseWheelFunc(wheel);
        glutTimerFunc(16,timer,0); glutMainLoop();
        // Freeglut may destroy the window before returning from its event loop.
        // The final context teardown also releases all remaining GPU resources.
    } catch(const std::exception& e) { std::cerr<<"Astana Night 3D: "<<e.what()<<"\n"; return 1; }
    return 0;
}
