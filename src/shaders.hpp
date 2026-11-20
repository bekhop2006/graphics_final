#pragma once

// All shader sources are embedded. The program can run from any directory.
namespace astana {
const char* const sceneVertex = R"GLSL(#version 330 core
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec3 color;
layout(location=3) in vec2 facadeUV;
layout(location=4) in float material;
layout(location=5) in float building;
uniform mat4 view, projection;
out vec3 worldPosition, surfaceNormal, baseColor;
out vec2 uv;
flat out int materialId, objectId;
void main() {
    worldPosition=position; surfaceNormal=normal; baseColor=color; uv=facadeUV;
    materialId=int(material+0.5); objectId=int(building+0.5);
    gl_Position=projection*view*vec4(position,1.0);
}
)GLSL";

const char* const sceneFragment = R"GLSL(#version 330 core
in vec3 worldPosition, surfaceNormal, baseColor;
in vec2 uv;
flat in int materialId, objectId;
uniform vec3 eye;
uniform float daylight, time, fogAmount;
uniform int selected;
uniform samplerCube nightCity, dayCity;
out vec4 fragColor;
float hash(vec2 x) { return fract(sin(dot(x,vec2(127.1,311.7)))*43758.5453); }
float windowPane(vec2 cell, vec2 filterWidth) {
    vec2 margin=min(cell,1.0-cell);
    vec2 edge=smoothstep(vec2(.105,.15)-filterWidth,vec2(.105,.15)+filterWidth,margin);
    return edge.x*edge.y;
}
float asphaltGrain(vec2 point) {
    vec2 pixel=fwidth(point);
    float weight=1.-smoothstep(.24,.86,max(pixel.x,pixel.y));
    if(weight<=0.) return 0.;
    vec2 cell=floor(point), t=fract(point);
    t=t*t*(3.-2.*t);
    float a=mix(hash(cell),hash(cell+vec2(1.,0.)),t.x);
    float b=mix(hash(cell+vec2(0.,1.)),hash(cell+vec2(1.,1.)),t.x);
    return (mix(a,b,t.y)-.5)*weight;
}
void main() {
    float d=daylight, night=1.0-d;
    vec3 n=normalize(surfaceNormal), v=normalize(eye-worldPosition);
    vec3 l=normalize(mix(vec3(-.46,.75,.53),vec3(-.66,.79,.52),d));
    float diffuse=max(dot(n,l),0.0);
    vec3 skyFill=mix(vec3(.46,.54,.75),vec3(.65,.70,.80),d);
    vec3 light=mix(vec3(.74,.83,1.0),vec3(1.0,.90,.72),d);
    vec3 c=baseColor*(skyFill*(.64+.16*n.y)+light*(.47+.25*d)*diffuse);
    vec3 h=normalize(l+v);
    float gloss=pow(max(dot(n,h),0.0),36.0);
    if(materialId==1) {
        vec2 width=min(vec2(.22),max(vec2(.012),fwidth(uv)*.55));
        float pane=windowPane(fract(uv),width);
        float room=hash(floor(uv)+float(objectId)*2.17);
        vec3 glass=mix(c*.56,c*1.03+vec3(.09,.17,.24),smoothstep(-.1,1.0,n.y+v.y));
        glass+=gloss*vec3(.24,.31,.35);
        vec3 lamp=mix(vec3(.39,.70,.91),vec3(1.0,.76,.43),step(.28,room));
        float lit=smoothstep(.37,.56,room)*(.28+.63*room)*night;
        c=mix(c*.87,glass+lamp*lit,pane);
    } else if(materialId==2) {
        float rim=pow(1.0-max(dot(v,n),0.0),2.2);
        c=baseColor*(.58+.55*diffuse+.27*max(n.y,0.0))
          +vec3(1.0,.83,.52)*(gloss*.73+rim*.36);
    } else if(materialId==3) {
        c=mix(baseColor*(1.37+night*.33),baseColor*.92,d*.60);
    } else if(materialId==4) {
        float grain=hash(floor(worldPosition.xz*2.0));
        c*=.93+grain*.09;
    } else if(materialId==5) {
        vec2 tile=abs(fract(worldPosition.xz*.71)-.5);
        vec2 edge=fwidth(worldPosition.xz*.71);
        float line=max(smoothstep(.45-edge.x,.49+edge.x,tile.x),
                       smoothstep(.45-edge.y,.49+edge.y,tile.y));
        c*=1.0-.12*line;
    } else if(materialId==6) {
        float wave=sin(worldPosition.x*2.6+time)*sin(worldPosition.z*1.7-time*.7);
        c=c*(.85+.12*wave)+vec3(.05,.19,.26)*(.52+.22*wave);
    } else if(materialId==7) {
        c+=vec3(.29,.23,.40)*night*(.35+.65*max(n.y,0.0));
    } else if(materialId>=8 && materialId<=10) {
        vec3 reflectionRay=reflect(-v,n);
        float blur=materialId==8 ? 2.3 : (materialId==9 ? .85 : 1.65);
        float pixelAngle=max(length(dFdx(reflectionRay)),length(dFdy(reflectionRay)));
        blur=max(blur,log2(max(1.,pixelAngle*float(textureSize(nightCity,0).x))));
        vec3 nightImage=textureLod(nightCity,reflectionRay,blur).rgb;
        vec3 dayImage=textureLod(dayCity,reflectionRay,blur).rgb;
        vec3 reflected=mix(pow(nightImage,vec3(.74)),dayImage,d);
        float fresnel=pow(1.-clamp(dot(v,n),0.,1.),3.);
        if(materialId==8) {
            vec3 tint=mix(vec3(.69,.83,.98),vec3(.98,.99,1.),d);
            vec3 body=baseColor*tint*(.37+.44*diffuse+.10*max(n.y,0.));
            c=body+reflected*(.24+.40*fresnel)+vec3(.88,.94,1.)*pow(max(dot(n,h),0.),92.)*.50;
        } else if(materialId==9) {
            vec3 tinted=baseColor*(.23+.13*max(n.y,0.));
            c=mix(tinted,reflected,.65+.28*fresnel)+gloss*vec3(.12,.15,.19);
        } else {
            c=baseColor*(.20+.28*diffuse)+reflected*baseColor*.84+gloss*vec3(.51,.53,.57);
        }
    } else if(materialId==11) {
        c=baseColor*(.61+.31*diffuse+.20*max(n.y,0.))+vec3(.07,.08,.095)*gloss;
    } else if(materialId==12 || materialId==13) {
        vec2 road=worldPosition.xz;
        c*=1.+.24*asphaltGrain(road*97.)+.065*asphaltGrain(road*21.);
        if(materialId==12) {
            // World coordinates match the road underneath at the edge of each patch.
            c=c*(1.-clamp(uv.x,0.,.78))+vec3(.30,.36,.38)*clamp(uv.y,0.,1.)*night;
        }
    }
    if(objectId!=0 && objectId==selected) {
        float rim=pow(1.0-max(dot(n,v),0.0),1.7);
        c=mix(c,vec3(1.0,.55,.10),.38)+vec3(.49,.27,.04)*(.12+rim);
    }
    float dist=length(eye-worldPosition);
    float fog=(1.0-exp(-.0032*dist))*fogAmount;
    // The far ground merges into the same horizon color used by the sky.
    fog=max(fog,smoothstep(180.,420.,dist)*fogAmount/.85);
    vec3 haze=mix(vec3(.19,.21,.36),vec3(.78,.83,.89),d);
    fragColor=vec4(mix(c,haze,clamp(fog,0.,1.)),1.);
}
)GLSL";

const char* const pickVertex = R"GLSL(#version 330 core
layout(location=0) in vec3 position;
layout(location=5) in float building;
uniform mat4 view, projection;
flat out uint id;
void main() {
    id=uint(building+0.5);
    gl_Position=projection*view*vec4(position,1.0);
}
)GLSL";
const char* const pickFragment = R"GLSL(#version 330 core
flat in uint id;
layout(location=0) out uint value;
void main() { value=id; }
)GLSL";

const char* const skyVertex = R"GLSL(#version 330 core
out vec2 screen;
void main() {
    vec2 points[3]=vec2[](vec2(-1,-1),vec2(3,-1),vec2(-1,3));
    screen=points[gl_VertexID];
    gl_Position=vec4(screen,1.,1.);
}
)GLSL";
const char* const skyFragment = R"GLSL(#version 330 core
in vec2 screen;
uniform vec3 forward, right, up;
uniform float aspect, tanHalfFov, daylight;
out vec4 fragColor;
float hash(vec2 p) { return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453); }
float stars(vec2 angular, vec2 grid) {
    vec2 p=angular*grid, cell=floor(p);
    vec2 place=.18+.64*vec2(hash(cell+7.1),hash(cell+13.3));
    float radius=mix(.044,.105,hash(cell+4.52));
    float edge=clamp(max(fwidth(p.x),fwidth(p.y))*.45,.025,.26);
    return (1.-smoothstep(radius-edge,radius+edge,length(fract(p)-place)))
      *step(.895,hash(cell))*(.35+.58*hash(cell+30.1));
}
void main() {
    vec3 ray=normalize(forward+screen.x*aspect*tanHalfFov*right+screen.y*tanHalfFov*up);
    float h=max(0.,ray.y), t=1.-exp(-h*3.0);
    vec3 night=mix(vec3(.19,.21,.36),vec3(.025,.042,.113),t);
    vec3 day=mix(vec3(.78,.83,.89),vec3(.25,.56,.86),t);
    vec3 c=mix(night,day,daylight);
    float dusk=4.*daylight*(1.-daylight);
    vec3 west=normalize(vec3(-1.,0.,.22));
    c+=vec3(.16,.075,.035)*dusk*exp(-4.*h)*(.3+.7*max(dot(ray,west),0.));
    vec2 angular=vec2(atan(ray.z,ray.x)/6.2831853+.5,asin(clamp(ray.y,-1.,1.))/3.1415927+.5);
    c+=vec3(.76,.85,1.)*stars(angular,vec2(480.,240.))
       *smoothstep(.01,.18,h)*pow(1.-daylight,2.);
    vec3 moonDir=normalize(vec3(-.44,.40,-.81));
    float disc=length(ray-moonDir);
    float aa=max(fwidth(disc),.0004);
    c+=vec3(.73,.77,.84)*(1.-smoothstep(.016-aa,.016+aa,disc))*(1.-daylight);
    c+=vec3(.16,.19,.25)*exp(-18.*disc)*(1.-daylight);
    fragColor=vec4(c,1.);
}
)GLSL";
} // namespace astana
