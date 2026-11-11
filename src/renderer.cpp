#include "renderer.hpp"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#include <stb_image_write.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <vector>
namespace ww { namespace {
constexpr float pi=3.14159265358979f;
struct Program {
 GLuint id=0;
 Program()=default; Program(const Program&)=delete; Program& operator=(const Program&)=delete;
 ~Program(){if(id)glDeleteProgram(id);}
 static std::string read(const std::string& p){std::ifstream f(p);if(!f)throw std::runtime_error("Missing shader: "+p);return {std::istreambuf_iterator<char>(f),{}};}
 void load(const std::string& vert,const std::string& frag){
  auto compile=[](GLenum type,const std::string& path){std::string text=read(path);const char* p=text.c_str();GLuint s=glCreateShader(type);glShaderSource(s,1,&p,nullptr);glCompileShader(s);GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);if(!ok){GLint n;glGetShaderiv(s,GL_INFO_LOG_LENGTH,&n);std::string log(n,' ');glGetShaderInfoLog(s,n,nullptr,log.data());glDeleteShader(s);throw std::runtime_error("Shader compile failed "+path+"\n"+log);}return s;};
  GLuint v=compile(GL_VERTEX_SHADER,vert),f=0;
  try{f=compile(GL_FRAGMENT_SHADER,frag);}catch(...){glDeleteShader(v);throw;}
  id=glCreateProgram();glAttachShader(id,v);glAttachShader(id,f);glLinkProgram(id);glDeleteShader(v);glDeleteShader(f);GLint ok;glGetProgramiv(id,GL_LINK_STATUS,&ok);if(!ok){GLint n;glGetProgramiv(id,GL_INFO_LOG_LENGTH,&n);std::string log(n,' ');glGetProgramInfoLog(id,n,nullptr,log.data());throw std::runtime_error("Shader link failed: "+log);}
 }
 void use()const{glUseProgram(id);} GLint loc(const char*n)const{return glGetUniformLocation(id,n);}
 void mat(const char*n,const glm::mat4&m)const{glUniformMatrix4fv(loc(n),1,GL_FALSE,glm::value_ptr(m));}
 void vec(const char*n,glm::vec3 v)const{glUniform3fv(loc(n),1,glm::value_ptr(v));}
 void value(const char*n,float v)const{glUniform1f(loc(n),v);} void integer(const char*n,int v)const{glUniform1i(loc(n),v);}
};
struct Mesh {
 GLuint vao=0,vbo=0; int count=0;
 Mesh()=default; Mesh(const Mesh&)=delete;Mesh&operator=(const Mesh&)=delete;
 ~Mesh(){if(vbo)glDeleteBuffers(1,&vbo);if(vao)glDeleteVertexArrays(1,&vao);}
 void upload(const std::vector<float>&v){count=int(v.size()/6);glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,v.size()*sizeof(float),v.data(),GL_STATIC_DRAW);glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,6*sizeof(float),nullptr);glEnableVertexAttribArray(1);glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)(3*sizeof(float)));glBindVertexArray(0);}
 void draw()const{glBindVertexArray(vao);glDrawArrays(GL_TRIANGLES,0,count);}
};
void tri(std::vector<float>&v,glm::vec3 a,glm::vec3 b,glm::vec3 c){glm::vec3 n=glm::normalize(glm::cross(b-a,c-a));for(auto p:{a,b,c})v.insert(v.end(),{p.x,p.y,p.z,n.x,n.y,n.z});}
void outward(std::vector<float>&v,glm::vec3 a,glm::vec3 b,glm::vec3 c){if(glm::dot(glm::cross(b-a,c-a),a+b+c)<0)std::swap(b,c);tri(v,a,b,c);}
void quad(std::vector<float>&v,glm::vec3 a,glm::vec3 b,glm::vec3 c,glm::vec3 d){tri(v,a,b,c);tri(v,a,c,d);}
void makeCube(Mesh&m){std::vector<float>v;std::array<glm::vec3,8>p={glm::vec3(-.5,-.5,-.5),{.5,-.5,-.5},{.5,.5,-.5},{-.5,.5,-.5},{-.5,-.5,.5},{.5,-.5,.5},{.5,.5,.5},{-.5,.5,.5}};for(auto f:std::array<std::array<int,4>,6>{{{0,3,2,1},{4,5,6,7},{0,4,7,3},{1,2,6,5},{0,1,5,4},{3,7,6,2}}})quad(v,p[f[0]],p[f[1]],p[f[2]],p[f[3]]);m.upload(v);}
void makeRadial(Mesh&m,int sides,bool cone){std::vector<float>v;for(int i=0;i<sides;i++){float a=2*pi*i/sides,b=2*pi*(i+1)/sides;glm::vec3 p(cos(a),0,sin(a)),q(cos(b),0,sin(b));tri(v,{0,0,0},p,q);if(cone)tri(v,p,{0,1,0},q);else{quad(v,p,p+glm::vec3(0,1,0),q+glm::vec3(0,1,0),q);tri(v,{0,1,0},q+glm::vec3(0,1,0),p+glm::vec3(0,1,0));}}m.upload(v);}
void makeSphere(Mesh&m){std::vector<float>v;constexpr int lat=8,lon=12;auto point=[](float a,float b){return glm::vec3(sin(a)*cos(b),cos(a),sin(a)*sin(b));};for(int j=0;j<lat;j++)for(int i=0;i<lon;i++){float a=pi*j/lat,b=pi*(j+1)/lat,c=2*pi*i/lon,d=2*pi*(i+1)/lon;if(j>0)outward(v,point(a,c),point(b,c),point(a,d));if(j<lat-1)outward(v,point(a,d),point(b,c),point(b,d));}m.upload(v);}
void makeRoof(Mesh&m){std::vector<float>v;glm::vec3 a(-1,0,-1),b(1,0,-1),c(0,.8,-1),d(-1,0,1),e(1,0,1),f(0,.8,1);tri(v,a,c,b);tri(v,d,e,f);quad(v,a,d,f,c);quad(v,b,c,f,e);quad(v,a,b,e,d);m.upload(v);}
glm::mat4 transform(glm::vec3 p,glm::vec3 s,float angle=0,glm::vec3 axis={0,1,0}){return glm::translate(glm::mat4(1),p)*glm::rotate(glm::mat4(1),angle,axis)*glm::scale(glm::mat4(1),s);}
struct Item { Mesh* mesh;glm::mat4 model;glm::vec3 color;int material=0;float emission=0;bool casts=true; };
struct Particle { glm::vec3 position;float phase; };
} // namespace
struct Renderer::Impl {
 Program scene,shadow,sky,particleProgram;
 Mesh cube,cylinder,cone,sphere,roof;
 GLuint shadowFbo=0,shadowTexture=0,groundTexture=0,skyVao=0,particleVao=0,particleVbo=0,instanceVbo=0;
 int shadowResolution=0; std::vector<Item>items;std::vector<Particle>particles;std::vector<glm::vec3>positions;
 std::mt19937 rng{2026};float snowBlend=0,nightBlend=0,cloudBlend=0; bool first=true;
 ~Impl(){glDeleteFramebuffers(1,&shadowFbo);glDeleteTextures(1,&shadowTexture);glDeleteTextures(1,&groundTexture);glDeleteVertexArrays(1,&skyVao);glDeleteVertexArrays(1,&particleVao);glDeleteBuffers(1,&particleVbo);glDeleteBuffers(1,&instanceVbo);}
 float random(float lo,float hi){return std::uniform_real_distribution<float>(lo,hi)(rng);}
 void setShadowResolution(int resolution){if(resolution==shadowResolution)return;shadowResolution=resolution;if(!shadowFbo)glGenFramebuffers(1,&shadowFbo);if(!shadowTexture)glGenTextures(1,&shadowTexture);glBindTexture(GL_TEXTURE_2D,shadowTexture);glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,resolution,resolution,0,GL_DEPTH_COMPONENT,GL_FLOAT,nullptr);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_BORDER);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_BORDER);float border[]={1,1,1,1};glTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,border);glBindFramebuffer(GL_FRAMEBUFFER,shadowFbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,shadowTexture,0);glDrawBuffer(GL_NONE);glReadBuffer(GL_NONE);GLenum status=glCheckFramebufferStatus(GL_FRAMEBUFFER);glBindFramebuffer(GL_FRAMEBUFFER,0);if(status!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("Shadow framebuffer incomplete, status "+std::to_string(status));}
 void add(Mesh&m,glm::vec3 p,glm::vec3 s,glm::vec3 color,int mat=0,float em=0,bool casts=true,float a=0,glm::vec3 axis={0,1,0}){items.push_back({&m,transform(p,s,a,axis),color,mat,em,casts});}
 void box(glm::vec3 p,glm::vec3 s,glm::vec3 c,int mat=0,float em=0){add(cube,p,s,c,mat,em);}
 void tree(float x,float z,float size,int type=0){box({x,.65f*size,z},{.22f*size,1.3f*size,.22f*size},{.29,.17,.09});if(type==0){for(int j=0;j<3;j++)add(cone,{x,(.7f+j*.58f)*size,z},{(1.05f-j*.17f)*size,1.55f*size,(1.05f-j*.17f)*size},{.035f+j*.013f,.17f+j*.023f,.10f+j*.012f});}else{for(int j=0;j<4;j++)add(sphere,{x+(j%2-.5f)*.55f*size,(1.6f+(j/2)*.35f)*size,z+(j/2-.5f)*.6f*size},{.85f*size,.9f*size,.8f*size},{.28,.45,.18});}}
 void buildScene(float time,float cloud,float night,float flash,float wind,glm::vec3 sunDir){
  items.clear();
  // Layered island, all geometry generated locally with explicit face normals.
  add(cylinder,{0,-.95,0},{8.0,.85,8.0},{.35,.25,.17});
  add(cylinder,{0,-.23,0},{8.12,.23,8.12},{.18,.33,.12},1);
  add(cylinder,{0,-1.16,0},{7.2,.24,7.2},{.24,.19,.16});
  // Pond and flat stepping stones.
  add(cylinder,{-3.2,.015,2.45},{2.15,.035,1.35},{.39,.45,.37});
  add(cylinder,{-3.2,.055,2.45},{1.94,.025,1.15},{.055,.39,.48},3);
  for(int i=0;i<8;i++){float a=i*2*pi/8;add(sphere,{-3.2f+2.03f*cos(a),.08f,2.45f+1.28f*sin(a)},{.26,.16,.21},{.55,.58,.49});}
  for(int i=0;i<7;i++)box({.15f+sin(i*.9f)*.15f,.04f,2.6f+i*.59f},{.68,.08,.43},{.69,.67,.53});
  // Cottage and porch.
  box({0,.97,0},{3.05,1.94,2.5},{.89,.78,.56});
  box({0,.2,0},{3.19,.4,2.64},{.51,.49,.40});
  for(int i=0;i<8;i++)box({0,.48f+i*.19f,1.255},{3.08,.025,.026},{.67,.55,.38});
  add(roof,{0,1.95,0},{1.82,1.65,1.65},{.04,.19,.22},2);
  box({.85,2.85,-.55},{.43,1.25,.48},{.54,.36,.27});box({.85,3.5,-.55},{.58,.16,.6},{.27,.26,.22});
  box({0,.63,1.27},{.62,1.24,.08},{.28,.16,.09});
  add(sphere,{.22,.65,1.34},{.045,.045,.045},{.98,.66,.2},4,.2);
  glm::vec3 glass=glm::mix(glm::vec3(.22,.56,.64),glm::vec3(1,.58,.16),night);
  for(float x:{-1.0f,1.0f}){box({x,1.15,1.28},{.64,.72,.07},{.38,.25,.13});box({x,1.15,1.33},{.5,.58,.04},glass,4,night*1.7f);box({x,1.15,1.37},{.045,.59,.03},{.35,.24,.14});box({x,1.15,1.37},{.51,.045,.03},{.35,.24,.14});box({x,.76,1.4},{.81,.12,.29},{.33,.24,.13});}
  for(float z:{-.65f,.65f}){box({1.535,1.16,z},{.04,.73,.65},{.38,.25,.13});box({1.565,1.16,z},{.025,.58,.5},glass,4,night*1.7f);box({1.585,1.16,z},{.025,.59,.04},{.35,.24,.14});}
  box({0,.11,1.97},{3.1,.22,1.26},{.48,.31,.16});
  for(int i=0;i<9;i++)box({-1.4f+i*.35f,.235,1.97},{.024,.025,1.26},{.29,.19,.1});
  for(float x:{-1.34f,1.34f}){box({x,.72,2.35},{.09,1.03,.09},{.52,.37,.2});box({x,.83,1.98},{.08,.07,.8},{.52,.37,.2});}
  // Trees and a woodland bench.
  tree(-3.4,-2.0,1.25);tree(-5.1,-.4,.95);tree(-5.3,-3.4,1.05);tree(3.4,-2.5,1.3);tree(5.2,-1.6,.85);tree(5.7,1.1,.8);tree(-.6,-4.2,1.0);tree(2.2,-5.0,.8);tree(-5.8,1.5,.65,1);
  box({3.5,.57,2.3},{1.65,.12,.58},{.54,.31,.13});for(float x:{2.86f,4.14f}){box({x,.28,2.3},{.12,.56,.47},{.25,.29,.24});box({x,.93,2.04},{.09,.8,.09},{.3,.31,.23});}box({3.5,1.02,2.04},{1.65,.28,.09},{.55,.33,.15});
  // Fence at the rear provides a visible test for the depth shadow map.
  for(int i=0;i<9;i++){float x=-3.6f+i*.9f;box({x,.5,-5.65},{.13,1,.14},{.61,.48,.29});}for(float y:{.35f,.75f})box({0,y,-5.65},{7.2,.1,.1},{.64,.51,.3});
  // Meadow tufts, flowers, and faceted stones.
  for(int i=0;i<70;i++){float a=i*2.399963f;float r=3.4f+float(i%17)*.23f;float x=cos(a)*r,z=sin(a)*r;if(z>1.2&&x<-.8&&x>-5.5)continue;float sway=sin(time*1.8f+i)*wind*.0005f;add(cone,{x,.01,z},{.10,.19f+(i%3)*.05f,.10},{.32,.43,.18},0,0,true,sway,{0,0,1});if(i%5==0)add(sphere,{x,.24,z},{.07,.06,.07},{.95,.7,.24});}
  for(int i=0;i<12;i++){float a=i*2.4f;add(sphere,{cos(a)*6.7f,.07f,sin(a)*6.7f},{.31f,.22f,.25f},{.47,.5,.43});}
  // Moving low-poly cloud clusters.
  int count=int(2+cloud*7);
  for(int i=0;i<count;i++){float x=std::fmod(i*4.83f+time*(.05f+wind*.004f)+15.f,29.f)-14.f;float z=-4.f-i%3*3.f;float y=7.1f+(i%2)*1.3f;glm::vec3 c=glm::mix(glm::vec3(.87,.92,.91),glm::vec3(.25,.32,.38),cloud*.8f);c=glm::mix(c,glm::vec3(.16,.21,.32),night*.7f);for(int j=0;j<4;j++)add(sphere,{x+(j-1.5f)*.9f,y+(j==1?.4f:0),z},{1.23f,.58f+(j%2)*.22f,.77f},c,4,.12f,false);}
  // Moon hangs above the far trees. Sun's disc is part of the sky shader.
  if(night>.05f)add(sphere,sunDir*11.f,{.65,.65,.65},{.76,.85,1},4,1.8f,false);
  if(night<.95f&&cloud<.7f)add(sphere,sunDir*11.f,{.68,.68,.68},{1.0,.69,.20},4,2.5f,false);
  // A segmented lightning bolt and its brief illumination share one timer.
  if(flash>.01f){glm::vec3 points[]={{-4,10,-4},{-4.4,8.2,-3.9},{-3.75,8.1,-3.9},{-4.2,6.5,-3.8},{-3.9,6.5,-3.8},{-4.7,4.8,-3.7}};for(int i=0;i<5;i++){glm::vec3 a=points[i],b=points[i+1],d=b-a;float len=glm::length(d);glm::mat4 m=glm::translate(glm::mat4(1),(a+b)*.5f);glm::vec3 up=glm::normalize(d),right=glm::normalize(glm::cross(up,glm::vec3(0,0,1))),forward=glm::cross(right,up);glm::mat4 basis(1);basis[0]=glm::vec4(right,0);basis[1]=glm::vec4(up,0);basis[2]=glm::vec4(forward,0);m=m*basis*glm::scale(glm::mat4(1),{.065f,len,.06f});items.push_back({&cube,m,{.72,.84,1},4,5,false});}}
 }
 void makeGround(){constexpr int size=64;std::vector<unsigned char>p(size*size*3);for(int y=0;y<size;y++)for(int x=0;x<size;x++){int i=(y*size+x)*3;float v=.86f+random(0,.14);for(int c=0;c<3;c++)p[i+c]=static_cast<unsigned char>(v*255);}glGenTextures(1,&groundTexture);glBindTexture(GL_TEXTURE_2D,groundTexture);glTexImage2D(GL_TEXTURE_2D,0,GL_RGB8,size,size,0,GL_RGB,GL_UNSIGNED_BYTE,p.data());glGenerateMipmap(GL_TEXTURE_2D);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);GLenum e=glGetError();if(e!=GL_NO_ERROR)throw std::runtime_error("Procedural ground texture upload failed: "+std::to_string(e));}
 void initParticles(){float q[]={-1,-1,1,-1,1,1,-1,-1,1,1,-1,1};glGenVertexArrays(1,&particleVao);glBindVertexArray(particleVao);glGenBuffers(1,&particleVbo);glBindBuffer(GL_ARRAY_BUFFER,particleVbo);glBufferData(GL_ARRAY_BUFFER,sizeof(q),q,GL_STATIC_DRAW);glEnableVertexAttribArray(0);glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),nullptr);glGenBuffers(1,&instanceVbo);glBindBuffer(GL_ARRAY_BUFFER,instanceVbo);glBufferData(GL_ARRAY_BUFFER,5000*sizeof(glm::vec3),nullptr,GL_STREAM_DRAW);glEnableVertexAttribArray(2);glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,sizeof(glm::vec3),nullptr);glVertexAttribDivisor(2,1);glBindVertexArray(0);for(int i=0;i<5000;i++)particles.push_back({{random(-9,9),random(.2,12),random(-9,9)},random(0,2*pi)});}
};
Renderer::Renderer():impl_(std::make_unique<Impl>()){}
Renderer::~Renderer()=default;
void Renderer::initialize(const std::string& root){auto&s=*impl_;auto path=[&](const char*f){return (std::filesystem::path(root)/"shaders"/f).string();};s.scene.load(path("scene.vert"),path("scene.frag"));s.shadow.load(path("shadow.vert"),path("shadow.frag"));s.sky.load(path("sky.vert"),path("sky.frag"));s.particleProgram.load(path("particles.vert"),path("particles.frag"));makeCube(s.cube);makeRadial(s.cylinder,48,false);makeRadial(s.cone,9,true);makeSphere(s.sphere);makeRoof(s.roof);s.makeGround();s.setShadowResolution(2048);s.initParticles();glGenVertexArrays(1,&s.skyVao);glEnable(GL_DEPTH_TEST);glEnable(GL_CULL_FACE);glCullFace(GL_BACK);glFrontFace(GL_CCW);std::cout<<"Renderer: depth testing, back-face culling, 2048px PCF shadows, instanced particles initialized\n";}
DrawStats Renderer::draw(const WeatherState&w,const Camera&camera,const RenderSettings&settings,int width,int height,float time,float dt){
 auto&s=*impl_;if(width<=0||height<=0)return{};
 float cloudy=std::clamp(w.cloudCover/100.f,0.f,1.f),night=w.isDay?0.f:1.f,snow=w.kind==WeatherKind::Snow?1.f:0.f;
 float blend=dt<=0.f?1.f:1.f-std::exp(-dt*2.f);if(s.first){s.nightBlend=night;s.cloudBlend=cloudy;s.snowBlend=snow;s.first=false;}else{s.nightBlend+=(night-s.nightBlend)*blend;s.cloudBlend+=(cloudy-s.cloudBlend)*blend;s.snowBlend+=(snow-s.snowBlend)*blend;}
 night=s.nightBlend;cloudy=s.cloudBlend;
 float phase=std::fmod(time+2.1f,7.0f);float flash=w.kind==WeatherKind::Storm?((phase<.09f)?1.f:((phase>.19f&&phase<.25f)?.6f:0.f)):0.f;
 glm::vec3 zenith=glm::mix(glm::vec3(.16,.40,.56),glm::vec3(.23,.30,.36),cloudy),horizon=glm::mix(glm::vec3(.63,.79,.88),glm::vec3(.59,.65,.65),cloudy);
 zenith=glm::mix(zenith,glm::vec3(.018,.035,.09),night);horizon=glm::mix(horizon,glm::vec3(.11,.16,.25),night);
 float angle=glm::radians(settings.lightAngle);glm::vec3 lightDir=glm::normalize(glm::vec3(cos(angle)*.85f,.9f,sin(angle)*.85f));
 glm::vec3 eye=glm::vec3(sin(glm::radians(camera.yaw))*cos(glm::radians(camera.pitch)),sin(glm::radians(camera.pitch)),cos(glm::radians(camera.yaw))*cos(glm::radians(camera.pitch)))*camera.distance+glm::vec3(0,1.5,0);
 glm::mat4 view=glm::lookAt(eye,glm::vec3(0,1.5,0),{0,1,0});
 int left=width/4;glm::mat4 projection=glm::perspective(glm::radians(43.f),float(width-left)/height,.1f,120.f),vp=projection*view;
 glm::mat4 lightSpace=glm::ortho(-13.f,13.f,-13.f,13.f,.1f,45.f)*glm::lookAt(lightDir*23.f,glm::vec3(0),{0,1,0});
 s.buildScene(time,cloudy,night,flash,w.windSpeed,lightDir);
 s.setShadowResolution(settings.highQuality?2048:1024);
 glEnable(GL_DEPTH_TEST);glEnable(GL_CULL_FACE);glDepthMask(GL_TRUE);glDisable(GL_BLEND);
 if(settings.shadows){glBindFramebuffer(GL_FRAMEBUFFER,s.shadowFbo);glViewport(0,0,s.shadowResolution,s.shadowResolution);glClear(GL_DEPTH_BUFFER_BIT);s.shadow.use();s.shadow.mat("lightSpace",lightSpace);glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(1.5,3);for(const auto&o:s.items)if(o.casts){s.shadow.mat("model",o.model);o.mesh->draw();}glDisable(GL_POLYGON_OFFSET_FILL);glBindFramebuffer(GL_FRAMEBUFFER,0);}
 glViewport(0,0,width,height);glClearColor(horizon.r,horizon.g,horizon.b,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glViewport(left,0,width-left,height);
 glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);s.sky.use();s.sky.mat("inverseViewProjection",glm::inverse(vp));s.sky.vec("eye",eye);s.sky.vec("zenith",zenith);s.sky.vec("horizon",horizon);s.sky.vec("sunDirection",lightDir);s.sky.value("night",night);s.sky.value("time",time);s.sky.value("cloudiness",cloudy);s.sky.value("flash",flash);glBindVertexArray(s.skyVao);glDrawArrays(GL_TRIANGLES,0,3);
 glEnable(GL_DEPTH_TEST);glEnable(GL_CULL_FACE);s.scene.use();s.scene.mat("viewProjection",vp);s.scene.mat("lightSpace",lightSpace);s.scene.vec("eye",eye);s.scene.vec("lightDirection",lightDir);s.scene.vec("lightColor",glm::mix(glm::vec3(1,.93,.77),glm::vec3(.46,.59,.90),night));s.scene.value("ambient",glm::mix(.36f,.15f,night)+cloudy*.05f);s.scene.value("directStrength",glm::mix(1.45f,.22f,night)*(1.f-cloudy*.56f));s.scene.value("snowAmount",s.snowBlend);s.scene.value("wetness",w.kind==WeatherKind::Rain||w.kind==WeatherKind::Storm?1.f:0.f);s.scene.value("flash",flash);s.scene.vec("fogColor",horizon);s.scene.value("fogDensity",w.kind==WeatherKind::Fog?.038f:.008f);s.scene.integer("shadowsEnabled",settings.shadows);s.scene.integer("shadowMap",0);s.scene.integer("groundTexture",1);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,s.shadowTexture);glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,s.groundTexture);
 for(const auto&o:s.items){s.scene.mat("model",o.model);s.scene.vec("baseColor",o.color);s.scene.integer("material",o.material);s.scene.value("emission",o.emission);o.mesh->draw();}
 int particleCount=0;bool isSnow=w.kind==WeatherKind::Snow;
 if(isSnow||w.kind==WeatherKind::Rain||w.kind==WeatherKind::Storm){particleCount=settings.highQuality?(isSnow?1800:w.kind==WeatherKind::Storm?5000:3200):(isSnow?700:1400);s.positions.clear();for(int i=0;i<particleCount;i++){auto&p=s.particles[i];p.position.y-=dt*(isSnow?1.1f:13.f);p.position.x+=dt*(w.windSpeed*.035f+(isSnow?sin(time+p.phase)*.25f:.4f));if(p.position.y<.1f){p.position.y=s.random(10,13);p.position.x=s.random(-9,9);p.position.z=s.random(-9,9);}if(p.position.x>9)p.position.x-=18;s.positions.push_back(p.position);}glBindBuffer(GL_ARRAY_BUFFER,s.instanceVbo);glBufferSubData(GL_ARRAY_BUFFER,0,s.positions.size()*sizeof(glm::vec3),s.positions.data());s.particleProgram.use();s.particleProgram.mat("viewProjection",vp);s.particleProgram.vec("cameraRight",{view[0][0],view[1][0],view[2][0]});s.particleProgram.vec("cameraUp",{view[0][1],view[1][1],view[2][1]});s.particleProgram.integer("snow",isSnow);s.particleProgram.value("night",night);glDisable(GL_CULL_FACE);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);glBindVertexArray(s.particleVao);glDrawArraysInstanced(GL_TRIANGLES,0,6,particleCount);glDepthMask(GL_TRUE);glDisable(GL_BLEND);glEnable(GL_CULL_FACE);}
 glBindVertexArray(0);glActiveTexture(GL_TEXTURE0);glViewport(0,0,width,height);
 return{int(s.items.size()),particleCount,settings.shadows?s.shadowResolution:0};
}
bool Renderer::screenshot(const std::string&path,int width,int height){if(width<1||height<1)return false;std::vector<unsigned char>pixels(static_cast<size_t>(width)*height*3);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadBuffer(GL_BACK);glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());glPixelStorei(GL_PACK_ALIGNMENT,4);for(int y=0;y<height/2;y++)for(int x=0;x<width*3;x++)std::swap(pixels[size_t(y)*width*3+x],pixels[size_t(height-1-y)*width*3+x]);std::error_code ec;auto parent=std::filesystem::path(path).parent_path();if(!parent.empty())std::filesystem::create_directories(parent,ec);bool ok=stbi_write_png(path.c_str(),width,height,3,pixels.data(),width*3)!=0;if(!ok)std::cerr<<"Screenshot write failed: "<<path<<"\n";return ok;}
} // namespace ww
