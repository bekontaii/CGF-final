#version 330 core
in vec2 uv;
out vec4 fragColor;
uniform vec3 zenith, horizon, sunDirection;
uniform mat4 inverseViewProjection;
uniform vec3 eye;
uniform float night, time, cloudiness, flash;
float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
void main(){
 vec4 q=inverseViewProjection*vec4(uv*2.0-1.0,1.0,1.0);
 vec3 ray=normalize(q.xyz/q.w-eye);
 float h=smoothstep(-0.7,0.6,ray.y);
 vec3 c=mix(horizon,zenith,h);
 float sun=max(dot(ray,sunDirection),0.0);
 c+=vec3(1.0,0.82,0.48)*pow(sun,180.0)*(1.0-night)*(1.0-cloudiness*0.8)*0.6;
 c+=vec3(1.0,0.93,0.7)*smoothstep(0.9985,0.9995,sun)*(1.0-night);
 vec2 starGrid=vec2(atan(ray.z,ray.x)*160.0,asin(ray.y)*160.0);
 vec2 cell=floor(starGrid);
 float point=1.0-smoothstep(0.02,0.12,length(fract(starGrid)-0.5));
 float star=step(0.985,hash(cell))*point*(0.4+0.6*hash(cell+3.0))*smoothstep(-0.6,0.15,ray.y);
 c+=star*night*(1.0-cloudiness)*vec3(0.6,0.75,1.0);
 c+=flash*vec3(0.5,0.62,0.9);
 fragColor=vec4(c,1.0);
}
