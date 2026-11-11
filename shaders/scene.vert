#version 330 core
layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aNormal;
uniform mat4 model, viewProjection, lightSpace;
out vec3 worldPosition;
out vec3 normal;
out vec4 shadowPosition;
void main(){
 vec4 p=model*vec4(aPosition,1.0);
 worldPosition=p.xyz;
 normal=mat3(transpose(inverse(model)))*aNormal;
 shadowPosition=lightSpace*p;
 gl_Position=viewProjection*p;
}
