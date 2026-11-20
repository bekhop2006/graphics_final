#pragma once
// macOS only: the small subset of the freeglut API used by main.cpp, implemented on GLFW.
// XQuartz freeglut cannot create an OpenGL 3.3 core context, while GLFW uses native Cocoa.
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <algorithm>
#include <stdexcept>
#include <vector>

enum { GLUT_CORE_PROFILE=1, GLUT_RGBA=0, GLUT_DOUBLE=2, GLUT_DEPTH=16,
       GLUT_LEFT_BUTTON=0, GLUT_DOWN=0, GLUT_UP=1, GLUT_ELAPSED_TIME=700,
       GLUT_ACTION_ON_WINDOW_CLOSE=0x01F9, GLUT_ACTION_GLUTMAINLOOP_RETURNS=1,
       GLUT_KEY_SHIFT_L=0x0070, GLUT_KEY_SHIFT_R=0x0071,
       GLUT_CURSOR_INHERIT=0x0064, GLUT_CURSOR_NONE=0x0065 };

namespace glutglfw {
struct Timer { double at; void (*fn)(int); int value; };
inline GLFWwindow*& window() { static GLFWwindow* w=nullptr; return w; }
inline int& width() { static int v=300; return v; }
inline int& height() { static int v=300; return v; }
inline bool& redisplay() { static bool v=true; return v; }
inline bool& leave() { static bool v=false; return v; }
inline double& wheelSum() { static double v=0; return v; }
inline bool& ignoreRepeat() { static bool v=false; return v; }
inline std::vector<Timer>& timers() { static std::vector<Timer> v; return v; }
inline void (*&displayFn())() { static void (*f)()=nullptr; return f; }
inline void (*&reshapeFn())(int,int) { static void (*f)(int,int)=nullptr; return f; }
inline void (*&keyFn())(unsigned char,int,int) { static void (*f)(unsigned char,int,int)=nullptr; return f; }
inline void (*&mouseFn())(int,int,int,int) { static void (*f)(int,int,int,int)=nullptr; return f; }
inline void (*&motionFn())(int,int) { static void (*f)(int,int)=nullptr; return f; }
inline void (*&wheelFn())(int,int,int,int) { static void (*f)(int,int,int,int)=nullptr; return f; }
inline void (*&closeFn())() { static void (*f)()=nullptr; return f; }
inline void (*&keyUpFn())(unsigned char,int,int) { static void (*f)(unsigned char,int,int)=nullptr; return f; }
inline void (*&specialFn())(int,int,int) { static void (*f)(int,int,int)=nullptr; return f; }
inline void (*&specialUpFn())(int,int,int) { static void (*f)(int,int,int)=nullptr; return f; }
inline void (*&passiveFn())(int,int) { static void (*f)(int,int)=nullptr; return f; }

// Cursor position in framebuffer pixels (Retina screens have 2 pixels per point).
inline void cursor(int& x,int& y) {
    double cx,cy; int ww,wh,fw,fh;
    glfwGetCursorPos(window(),&cx,&cy);
    glfwGetWindowSize(window(),&ww,&wh); glfwGetFramebufferSize(window(),&fw,&fh);
    x=int(cx*fw/std::max(ww,1)); y=int(cy*fh/std::max(wh,1));
}
inline void onKey(GLFWwindow*,int key,int,int action,int) {
    if(action==GLFW_REPEAT && ignoreRepeat()) return;
    int x,y; cursor(x,y);
    if(key==GLFW_KEY_LEFT_SHIFT || key==GLFW_KEY_RIGHT_SHIFT) {
        const int k=key==GLFW_KEY_LEFT_SHIFT?GLUT_KEY_SHIFT_L:GLUT_KEY_SHIFT_R;
        void (*f)(int,int,int)=action==GLFW_RELEASE?specialUpFn():specialFn();
        if(f) f(k,x,y);
        return;
    }
    unsigned char c=0;
    if(key>=GLFW_KEY_A && key<=GLFW_KEY_Z) c=static_cast<unsigned char>('a'+key-GLFW_KEY_A);
    else if(key>=GLFW_KEY_0 && key<=GLFW_KEY_9) c=static_cast<unsigned char>('0'+key-GLFW_KEY_0);
    else if(key>=GLFW_KEY_KP_0 && key<=GLFW_KEY_KP_9) c=static_cast<unsigned char>('0'+key-GLFW_KEY_KP_0);
    else if(key==GLFW_KEY_ESCAPE) c=27;
    else if(key==GLFW_KEY_SPACE) c=' ';
    void (*f)(unsigned char,int,int)=action==GLFW_RELEASE?keyUpFn():keyFn();
    if(c && f) f(c,x,y);
}
inline void onButton(GLFWwindow*,int button,int action,int) {
    if(!mouseFn() || button!=GLFW_MOUSE_BUTTON_LEFT) return;
    int x,y; cursor(x,y); mouseFn()(GLUT_LEFT_BUTTON,action==GLFW_PRESS?GLUT_DOWN:GLUT_UP,x,y);
}
inline void onCursor(GLFWwindow* w,double,double) {
    void (*f)(int,int)=glfwGetMouseButton(w,GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS?motionFn():passiveFn();
    if(!f) return;
    int x,y; cursor(x,y); f(x,y);
}
inline void onScroll(GLFWwindow*,double,double dy) {
    // Trackpads send many fractional steps; one zoom step per whole wheel unit.
    wheelSum()+=dy;
    while(wheelFn() && std::abs(wheelSum())>=1) {
        const int d=wheelSum()>0?1:-1; wheelSum()-=d;
        int x,y; cursor(x,y); wheelFn()(0,d,x,y);
    }
}
inline void onResize(GLFWwindow*,int w,int h) {
    if(reshapeFn() && w>0 && h>0) reshapeFn()(w,h);
    redisplay()=true;
}
}

inline void glutInit(int*,char**) {
    if(!glfwInit()) throw std::runtime_error("GLFW initialization failed");
}
inline void glutInitContextVersion(int major,int minor) {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,major); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,minor);
}
inline void glutInitContextProfile(int) {
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,GLFW_TRUE); // required by macOS
}
inline void glutInitDisplayMode(unsigned) { glfwWindowHint(GLFW_DEPTH_BITS,24); }
inline void glutInitWindowSize(int w,int h) { glutglfw::width()=w; glutglfw::height()=h; }
inline void glutSetOption(int,int) {}
inline int glutCreateWindow(const char* title) {
    using namespace glutglfw;
    window()=glfwCreateWindow(width(),height(),title,nullptr,nullptr);
    if(!window()) throw std::runtime_error("Cannot create an OpenGL 3.3 core window");
    glfwMakeContextCurrent(window()); glfwSwapInterval(1);
    glfwSetKeyCallback(window(),onKey); glfwSetMouseButtonCallback(window(),onButton);
    glfwSetCursorPosCallback(window(),onCursor); glfwSetScrollCallback(window(),onScroll);
    glfwSetFramebufferSizeCallback(window(),onResize);
    return 1;
}
inline void glutHideWindow() { glfwHideWindow(glutglfw::window()); }
inline void glutDestroyWindow(int) { glfwDestroyWindow(glutglfw::window()); glutglfw::window()=nullptr; glfwTerminate(); }
inline int glutGetWindow() { return glutglfw::window()?1:0; }
inline void glutSetWindowTitle(const char* t) { glfwSetWindowTitle(glutglfw::window(),t); }
inline void glutSwapBuffers() { glfwSwapBuffers(glutglfw::window()); }
inline void glutPostRedisplay() { glutglfw::redisplay()=true; }
inline int glutGet(int) { return int(glfwGetTime()*1000); }
inline void glutTimerFunc(unsigned ms,void (*fn)(int),int value) {
    glutglfw::timers().push_back({glfwGetTime()+ms/1000.0,fn,value});
}
inline void glutDisplayFunc(void (*f)()) { glutglfw::displayFn()=f; }
inline void glutReshapeFunc(void (*f)(int,int)) { glutglfw::reshapeFn()=f; }
inline void glutKeyboardFunc(void (*f)(unsigned char,int,int)) { glutglfw::keyFn()=f; }
inline void glutMouseFunc(void (*f)(int,int,int,int)) { glutglfw::mouseFn()=f; }
inline void glutMotionFunc(void (*f)(int,int)) { glutglfw::motionFn()=f; }
inline void glutMouseWheelFunc(void (*f)(int,int,int,int)) { glutglfw::wheelFn()=f; }
inline void glutCloseFunc(void (*f)()) { glutglfw::closeFn()=f; }
inline void glutKeyboardUpFunc(void (*f)(unsigned char,int,int)) { glutglfw::keyUpFn()=f; }
inline void glutSpecialFunc(void (*f)(int,int,int)) { glutglfw::specialFn()=f; }
inline void glutSpecialUpFunc(void (*f)(int,int,int)) { glutglfw::specialUpFn()=f; }
inline void glutPassiveMotionFunc(void (*f)(int,int)) { glutglfw::passiveFn()=f; }
inline void glutIgnoreKeyRepeat(int ignore) { glutglfw::ignoreRepeat()=ignore!=0; }
// A hidden, captured cursor gives unlimited relative motion for mouse look.
inline void glutSetCursor(int cursor) {
    glfwSetInputMode(glutglfw::window(),GLFW_CURSOR,cursor==GLUT_CURSOR_NONE?GLFW_CURSOR_DISABLED:GLFW_CURSOR_NORMAL);
}
inline void glutWarpPointer(int x,int y) {
    int ww,wh,fw,fh;
    glfwGetWindowSize(glutglfw::window(),&ww,&wh); glfwGetFramebufferSize(glutglfw::window(),&fw,&fh);
    glfwSetCursorPos(glutglfw::window(),double(x)*ww/std::max(fw,1),double(y)*wh/std::max(fh,1));
}
inline void glutLeaveMainLoop() { glutglfw::leave()=true; }
inline void glutMainLoop() {
    using namespace glutglfw;
    int fw,fh; glfwGetFramebufferSize(window(),&fw,&fh); onResize(window(),fw,fh);
    while(!leave() && !glfwWindowShouldClose(window())) {
        double next=glfwGetTime()+.25;
        for(const Timer& t:timers()) next=std::min(next,t.at);
        const double wait=next-glfwGetTime();
        if(redisplay()) glfwPollEvents(); else if(wait>0) glfwWaitEventsTimeout(wait); else glfwPollEvents();
        const double now=glfwGetTime();
        std::vector<Timer> due;
        auto& all=timers();
        for(std::size_t i=0;i<all.size();) {
            if(all[i].at<=now) { due.push_back(all[i]); all.erase(all.begin()+long(i)); } else ++i;
        }
        for(const Timer& t:due) if(!leave()) t.fn(t.value);
        if(redisplay() && !leave() && displayFn()) { redisplay()=false; displayFn()(); }
    }
    if(closeFn()) closeFn()();
    glutDestroyWindow(1);
}
