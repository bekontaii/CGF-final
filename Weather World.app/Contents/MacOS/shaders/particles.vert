#version 330 core
layout(location=0) in vec2 aCorner;
layout(location=2) in vec3 instancePosition;
uniform mat4 viewProjection;
uniform vec3 cameraRight, cameraUp;
uniform bool snow;
out vec2 uv;
void main(){
 uv=aCorner;
 vec3 offset=snow?(cameraRight*aCorner.x+cameraUp*aCorner.y)*0.055:cameraRight*aCorner.x*0.014+vec3(-0.08,0.33,0)*aCorner.y;
 gl_Position=viewProjection*vec4(instancePosition+offset,1.0);
}
