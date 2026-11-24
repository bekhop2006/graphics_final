#pragma once

#include "math.hpp"
#include "scene.hpp"
#include <GL/glew.h>
#include <utility>
#include <vector>

namespace astana {
struct View {
    Camera cam;
    float daylight=0, time=0, fog=0.85f;
    int selected=0;
    bool wire=false, ssaa=true;
    Vec3 carPosition={-104,.015f,-17.4f}, carForward={1,0,0};
};

class Renderer {
public:
    void init(const std::vector<Vertex>& mesh, std::size_t cityVertices, std::size_t staticVertices, bool offscreenCapture);
    void resize(int width, int height);
    void render(const View& state);
    unsigned int pick(const View& state, int x, int y);
    std::pair<int,int> findClick(const View& state, int object, Vec3 preferred);
    void saveFrame(const char* filename);
    int width() const {return w;}
    int height() const {return h;}
    int scale() const {return sampleScale;}
    void shutdown();
private:
    int w=0, h=0, sampleScale=2;
    GLsizei count=0, cityCount=0, staticCount=0;
    GLuint meshVAO=0, meshVBO=0, skyVAO=0;
    GLuint surface=0, sky=0, picking=0;
    GLuint reflections[2]={0,0};
    bool offscreen=false;
    GLuint highFBO=0, highColor=0, highDepth=0;
    GLuint outputFBO=0, outputColor=0, outputDepth=0;
    GLuint pickFBO=0, pickColor=0, pickDepth=0;
    std::vector<unsigned char> pixels;
    void pickPass(const View& state);
    void drawMesh(GLuint program, const View& state, GLsizei staticEnd);
    void setCamera(GLuint program, const Camera& camera, float aspect, float fovY, Vec3 upDirection);
    void drawWorld(const View& state, float aspect, float fovY, Vec3 upDirection, bool bake);
    void bakeReflections();
};
} // namespace astana
