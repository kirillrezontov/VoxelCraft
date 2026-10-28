#version 450
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec3 normal;
layout(location = 4) in float opacity;
layout(location = 5) in vec2 light;
layout(location = 6) in vec3 sway;
layout(location = 7) in vec3 instanceOffset;
layout(location = 8) in float instanceYaw;
layout(location = 9) in vec2 instanceLight;
layout(location = 10) in float instancePhase;
layout(location = 0) out vec3 vertexColor;
layout(location = 1) out vec2 textureUv;
layout(location = 2) flat out float untextured;
layout(location = 3) out vec3 surfaceNormal;
layout(location = 4) out vec2 illumination;
layout(location = 5) out float vertexOpacity;
layout(location = 6) out float viewDistance;
layout(push_constant) uniform Camera { mat4 viewProjection; vec4 offset; vec4 sun; vec4 sky; vec4 fog; } camera;
void main() {
    float c=cos(instanceYaw),s=sin(instanceYaw);
    mat3 rotation=mat3(c,0,-s,0,1,0,s,0,c);
    vec3 relative=rotation*(position+sway*sin(instancePhase))+instanceOffset+camera.offset.xyz;
    gl_Position = camera.viewProjection * vec4(relative,1);
    vertexColor = color;
    textureUv = uv;
    untextured = camera.offset.w;
    surfaceNormal=rotation*normal;illumination=vec2(light.x*instanceLight.x,max(light.y,instanceLight.y));vertexOpacity=opacity;
    viewDistance=camera.fog.z>0.5?length(relative.xz):length(relative);
}
