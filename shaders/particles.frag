#version 330 core
in vec2 uv;
out vec4 fragColor;
uniform bool snow;
uniform float night;
void main(){
 float a=snow?(1.0-smoothstep(0.2,1.0,length(uv))):0.47;
 if(a<0.03)discard;
 fragColor=vec4(snow?mix(vec3(0.95),vec3(0.65,0.76,0.92),night):mix(vec3(0.65,0.83,0.98),vec3(0.31,0.43,0.7),night),a);
}
