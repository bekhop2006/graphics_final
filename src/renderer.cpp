#include "renderer.hpp"
#include "shaders.hpp"
#include <algorithm>
#include <cstddef>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>

namespace astana {
namespace {
GLuint compile(GLenum type,const char* source) {
    GLuint s=glCreateShader(type);
    glShaderSource(s,1,&source,nullptr); glCompileShader(s);
    GLint ok=0; glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok) {
        char why[4096]={}; glGetShaderInfoLog(s,sizeof why,nullptr,why);
        throw std::runtime_error(std::string("Shader compilation: ")+why);
    }
    return s;
}
GLuint program(const char* vs,const char* fs) {
    GLuint v=compile(GL_VERTEX_SHADER,vs), f=compile(GL_FRAGMENT_SHADER,fs), p=glCreateProgram();
    glAttachShader(p,v); glAttachShader(p,f); glLinkProgram(p);
    glDeleteShader(v); glDeleteShader(f);
    GLint ok=0; glGetProgramiv(p,GL_LINK_STATUS,&ok);
    if(!ok) {
        char why[4096]={}; glGetProgramInfoLog(p,sizeof why,nullptr,why);
        throw std::runtime_error(std::string("Shader link: ")+why);
    }
    return p;
}
void scalar(GLuint p, const char* name,float x) { glUniform1f(glGetUniformLocation(p,name),x); }
void vector(GLuint p, const char* name,Vec3 v) { glUniform3f(glGetUniformLocation(p,name),v.x,v.y,v.z); }
void allocateTarget(GLuint& frame,GLuint& color,GLuint& depth,int w,int h,bool integer) {
    if(!frame) {
        glGenFramebuffers(1,&frame); glGenTextures(1,&color); glGenRenderbuffers(1,&depth);
    }
    glBindFramebuffer(GL_FRAMEBUFFER,frame);
    glBindTexture(GL_TEXTURE_2D,color);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,integer?GL_NEAREST:GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,integer?GL_NEAREST:GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D,0,integer?GL_R32UI:GL_RGBA8,w,h,0,
        integer?GL_RED_INTEGER:GL_RGBA,integer?GL_UNSIGNED_INT:GL_UNSIGNED_BYTE,nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
    glBindRenderbuffer(GL_RENDERBUFFER,depth);
    glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,w,h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("Framebuffer allocation failed");
    glBindFramebuffer(GL_FRAMEBUFFER,0);
}
}

void Renderer::init(const std::vector<Vertex>& mesh, std::size_t cityVertices, bool offscreenCapture) {
    offscreen=offscreenCapture;
    surface=program(sceneVertex,sceneFragment);
    picking=program(pickVertex,pickFragment);
    sky=program(skyVertex,skyFragment);
    count=static_cast<GLsizei>(mesh.size());
    cityCount=static_cast<GLsizei>(cityVertices);
    glGenVertexArrays(1,&meshVAO); glBindVertexArray(meshVAO);
    glGenBuffers(1,&meshVBO); glBindBuffer(GL_ARRAY_BUFFER,meshVBO);
    glBufferData(GL_ARRAY_BUFFER,mesh.size()*sizeof(Vertex),mesh.data(),GL_STATIC_DRAW);
    const int sizes[]={3,3,3,2,1,1};
    const std::size_t offsets[]={offsetof(Vertex,x),offsetof(Vertex,nx),offsetof(Vertex,r),
                                 offsetof(Vertex,u),offsetof(Vertex,mat),offsetof(Vertex,id)};
    for(GLuint a=0;a<6;++a) {
        glEnableVertexAttribArray(a);
        glVertexAttribPointer(a,sizes[a],GL_FLOAT,GL_FALSE,sizeof(Vertex),
                              reinterpret_cast<void*>(offsets[a]));
    }
    glGenVertexArrays(1,&skyVAO); glBindVertexArray(0);
    glDisable(GL_BLEND); glDisable(GL_DITHER);
    glPixelStorei(GL_PACK_ALIGNMENT,1);
    bakeReflections();
}
void Renderer::resize(int width,int height) {
    width=std::max(1,width); height=std::max(1,height);
    if(width==w && height==h) return;
    w=width; h=height;
    GLint textureLimit=0, renderLimit=0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE,&textureLimit);
    glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE,&renderLimit);
    sampleScale=(std::max(w,h)<=std::min(textureLimit,renderLimit)/2)?2:1;
    allocateTarget(highFBO,highColor,highDepth,w*sampleScale,h*sampleScale,false);
    allocateTarget(pickFBO,pickColor,pickDepth,w,h,true);
    if(offscreen) allocateTarget(outputFBO,outputColor,outputDepth,w,h,false);
}
void Renderer::setCamera(GLuint prog,const Camera& camera,float aspect,float fovY,Vec3 upDirection) {
    const Mat4 p=perspective(fovY,aspect,.15f,520.0f), v=lookAt(camera,upDirection);
    glUniformMatrix4fv(glGetUniformLocation(prog,"projection"),1,GL_FALSE,p.m);
    glUniformMatrix4fv(glGetUniformLocation(prog,"view"),1,GL_FALSE,v.m);
}
void Renderer::drawWorld(const View& s, float aspect, float fovY, Vec3 upDirection, bool bake) {
    glClear(GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST); glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    glUseProgram(sky); glBindVertexArray(skyVAO);
    const Vec3 f=unit(s.cam.target-s.cam.eye), r=unit(cross(f,upDirection)), u=cross(r,f);
    vector(sky,"forward",f); vector(sky,"right",r); vector(sky,"up",u);
    scalar(sky,"aspect",aspect); scalar(sky,"tanHalfFov",std::tan(fovY/2));
    scalar(sky,"daylight",s.daylight); glDrawArrays(GL_TRIANGLES,0,3);
    glEnable(GL_DEPTH_TEST); glUseProgram(surface); glBindVertexArray(meshVAO);
    setCamera(surface,s.cam,aspect,fovY,upDirection);
    vector(surface,"eye",s.cam.eye); scalar(surface,"time",s.time);
    scalar(surface,"daylight",s.daylight); scalar(surface,"fogAmount",s.fog);
    glUniform1i(glGetUniformLocation(surface,"selected"),s.selected);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP,bake ? 0 : reflections[0]);
    glUniform1i(glGetUniformLocation(surface,"nightCity"),0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_CUBE_MAP,bake ? 0 : reflections[1]);
    glUniform1i(glGetUniformLocation(surface,"dayCity"),1);
    glActiveTexture(GL_TEXTURE0);
    if(s.wire) glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
    glDrawArrays(GL_TRIANGLES,0,bake ? cityCount : count);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
}
void Renderer::bakeReflections() {
    // Capture the static city at car height. Six 90-degree views form each cubemap.
    constexpr int faceSize=256;
    const Vec3 directions[]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    const Vec3 up[]={{0,-1,0},{0,-1,0},{0,0,1},{0,0,-1},{0,-1,0},{0,-1,0}};
    const CarSpot& spot=carSpots().front();
    const Vec3 origin={spot.x,.89f,spot.z};
    GLuint fbo=0, depth=0;
    glGenFramebuffers(1,&fbo);
    glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    glGenRenderbuffers(1,&depth);
    glBindRenderbuffer(GL_RENDERBUFFER,depth);
    glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,faceSize,faceSize);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
    glGenTextures(2,reflections);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    glViewport(0,0,faceSize,faceSize);
    for(int light=0;light<2;++light) {
        GLuint cube=reflections[light];
        glBindTexture(GL_TEXTURE_CUBE_MAP,cube);
        for(GLenum axis:{GL_TEXTURE_WRAP_S,GL_TEXTURE_WRAP_T,GL_TEXTURE_WRAP_R})
            glTexParameteri(GL_TEXTURE_CUBE_MAP,axis,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        for(int face=0;face<6;++face)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,0,GL_RGB8,faceSize,faceSize,
                         0,GL_RGB,GL_UNSIGNED_BYTE,nullptr);
        View probe;
        probe.daylight=static_cast<float>(light);
        for(int face=0;face<6;++face) {
            glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,cube,0);
            if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)
                throw std::runtime_error("Could not allocate the car reflection map");
            probe.cam={origin,origin+directions[face]};
            // The draw range ends before the first car, avoiding self-reflections.
            drawWorld(probe,1.f,.5f*pi,up[face],true);
        }
        glBindTexture(GL_TEXTURE_CUBE_MAP,cube);
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
        glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP,0);
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    glDeleteFramebuffers(1,&fbo);
    glDeleteRenderbuffers(1,&depth);
}
void Renderer::render(const View& s) {
    const int k=s.ssaa?sampleScale:1;
    glBindFramebuffer(GL_FRAMEBUFFER,s.ssaa?highFBO:outputFBO);
    glViewport(0,0,w*k,h*k);
    drawWorld(s,float(w)/h,s.cam.verticalFov,{0,1,0},false);
    if(s.ssaa) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER,highFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,outputFBO);
        glBlitFramebuffer(0,0,w*k,h*k,0,0,w,h,GL_COLOR_BUFFER_BIT,GL_LINEAR);
    }
    glBindFramebuffer(GL_FRAMEBUFFER,0); glViewport(0,0,w,h);
}
void Renderer::pickPass(const View& s) {
    glBindFramebuffer(GL_FRAMEBUFFER,pickFBO); glViewport(0,0,w,h);
    const GLuint empty[4]={0,0,0,0};
    glClearBufferuiv(GL_COLOR,0,empty); glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST); glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    glUseProgram(picking); glBindVertexArray(meshVAO);
    setCamera(picking,s.cam,float(w)/h,s.cam.verticalFov,{0,1,0});
    glDrawArrays(GL_TRIANGLES,0,count);
}
unsigned int Renderer::pick(const View& s,int x,int y) {
    if(x<0 || y<0 || x>=w || y>=h) return 0;
    pickPass(s);
    GLuint id=0;
    glReadPixels(x,h-1-y,1,1,GL_RED_INTEGER,GL_UNSIGNED_INT,&id);
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    return id;
}
std::pair<int,int> Renderer::findClick(const View& s,int object,Vec3 preferred) {
    // Choose a visible sample closest to the projected crown for the scripted click.
    const Vec3 f=unit(s.cam.target-s.cam.eye), r=unit(cross(f,{0,1,0})), u=cross(r,f);
    const Vec3 rel=preferred-s.cam.eye;
    const float depth=std::max(.15f,dot(rel,f));
    const float pixelScale=h/(2*depth*std::tan(s.cam.verticalFov/2));
    const int preferredX=int(w*.5f+dot(rel,r)*pixelScale);
    const int preferredY=int(h*.5f-dot(rel,u)*pixelScale);
    if(pick(s,preferredX,preferredY)==static_cast<unsigned>(object)) return {preferredX,preferredY};
    pickPass(s);
    std::vector<GLuint> ids(std::size_t(w)*h);
    glReadPixels(0,0,w,h,GL_RED_INTEGER,GL_UNSIGNED_INT,ids.data());
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    int best=std::numeric_limits<int>::max();
    std::pair<int,int> result(-1,-1);
    for(int y=0;y<h;++y) for(int x=0;x<w;++x) if(int(ids[std::size_t(h-1-y)*w+x])==object) {
        const int dx=x-preferredX,dy=y-preferredY,dist=dx*dx+dy*dy;
        if(dist<best) { best=dist; result={x,y}; }
    }
    return result;
}
void Renderer::saveFrame(const char* filename) {
    pixels.resize(std::size_t(w)*h*3);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,outputFBO);
    glReadBuffer(offscreen?GL_COLOR_ATTACHMENT0:GL_BACK);
    glReadPixels(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    glBindFramebuffer(GL_READ_FRAMEBUFFER,0);
    std::ofstream out(filename,std::ios::binary);
    if(!out) throw std::runtime_error(std::string("Cannot write ")+filename);
    out<<"P6\n"<<w<<" "<<h<<"\n255\n";
    const std::size_t stride=std::size_t(w)*3;
    for(int row=h-1;row>=0;--row)
        out.write(reinterpret_cast<char*>(pixels.data()+row*stride),stride);
    if(!out) throw std::runtime_error(std::string("Could not finish writing ")+filename);
}
void Renderer::shutdown() {
    glDeleteTextures(2,reflections);
    for(GLuint p:{surface,sky,picking}) glDeleteProgram(p);
    for(GLuint id:{meshVAO,skyVAO}) glDeleteVertexArrays(1,&id);
    glDeleteBuffers(1,&meshVBO);
    for(GLuint id:{pickFBO,highFBO,outputFBO})glDeleteFramebuffers(1,&id);
    for(GLuint id:{pickColor,highColor,outputColor})glDeleteTextures(1,&id);
    for(GLuint id:{pickDepth,highDepth,outputDepth})glDeleteRenderbuffers(1,&id);
}
} // namespace astana
