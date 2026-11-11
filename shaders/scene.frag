#version 330 core
in vec3 worldPosition;
in vec3 normal;
in vec4 shadowPosition;
out vec4 fragColor;
uniform vec3 baseColor, lightDirection, eye, lightColor, fogColor;
uniform float ambient, directStrength, snowAmount, wetness, emission, fogDensity, flash;
uniform int material;
uniform bool shadowsEnabled;
uniform sampler2D shadowMap, groundTexture;
// A slope-aware bias and 3x3 percentage-closer filter keep the shadow edge stable.
float shadowVisibility(vec3 n){
 if(!shadowsEnabled)return 1.0;
 vec3 p=shadowPosition.xyz/shadowPosition.w*0.5+0.5;
 if(p.z>1.0||p.z<0.0||any(lessThan(p.xy,vec2(0)))||any(greaterThan(p.xy,vec2(1))))return 1.0;
 float bias=max(0.0025*(1.0-dot(n,lightDirection)),0.00065);
 vec2 texel=1.0/vec2(textureSize(shadowMap,0));
 float visible=0.0;
 for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){
  float depth=texture(shadowMap,p.xy+vec2(x,y)*texel).r;
  visible+=(p.z-bias<=depth)?1.0:0.0;
 }
 return visible/9.0;
}
void main(){
 vec3 n=normalize(normal), albedo=baseColor;
 if(material==1)albedo*=texture(groundTexture,worldPosition.xz*0.20).rgb;
 if(material==2){float seam=step(0.92,fract(worldPosition.x*3.0))+step(0.94,fract(worldPosition.z*3.0)); albedo*=1.0-min(seam,1.0)*0.16;}
 if(material!=3 && material!=4){float cover=smoothstep(0.35,0.85,n.y)*snowAmount;albedo=mix(albedo,vec3(0.87,0.94,0.98),cover);}
 float diffuse=max(dot(n,lightDirection),0.0);
 vec3 halfVector=normalize(lightDirection+normalize(eye-worldPosition));
 float spec=pow(max(dot(n,halfVector),0.0),material==3?64.0:32.0);
 float vis=shadowVisibility(n);
 vec3 color=albedo*(ambient+lightColor*directStrength*diffuse*vis);
 color+=lightColor*spec*(material==3?0.8:wetness*0.26)*vis;
 color+=albedo*emission+vec3(0.64,0.76,1.0)*flash*0.65;
 // Warm local light from the cottage windows at night.
 float lamp=exp(-length(worldPosition-vec3(0.0,1.2,1.7))*0.9)*max(0.0,1.0-directStrength);
 color+=albedo*vec3(1.0,0.48,0.14)*lamp*0.55;
 float fog=1.0-exp(-pow(length(eye-worldPosition)*fogDensity,2.0));
 color=mix(color,fogColor,clamp(fog,0.0,0.85));
 color=color/(color+vec3(0.65));
 color=pow(max(color,vec3(0)),vec3(1.0/2.2));
 fragColor=vec4(color,1.0);
}
