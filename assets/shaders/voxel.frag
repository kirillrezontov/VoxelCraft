#version 450
layout(location = 0) in vec3 vertexColor;
layout(location = 1) in vec2 textureUv;
layout(location = 2) flat in float untextured;
layout(location = 3) in vec3 surfaceNormal;
layout(location = 4) in vec2 illumination;
layout(location = 5) in float vertexOpacity;
layout(location = 6) in float viewDistance;
layout(push_constant) uniform Camera { mat4 viewProjection; vec4 offset; vec4 sun; vec4 sky; vec4 fog; } camera;
layout(set = 0, binding = 0) uniform sampler2D atlas;
layout(location = 0) out vec4 outColor;
void main() {
    if(untextured>0.5&&untextured<1.5) { outColor=vec4(vertexColor,1);return; }
    vec4 albedo=untextured>2.5?vec4(vertexColor,vertexOpacity):texture(atlas,textureUv)*vec4(vertexColor,vertexOpacity);
    if(untextured>1.5&&untextured<2.5) { outColor=albedo;return; } // Textured UI is unlit and unfogged.
    vec3 n=dot(surfaceNormal,surfaceNormal)>0.5?normalize(surfaceNormal):vec3(0,1,0);
    float direct=max(0,dot(n,camera.sun.xyz))*camera.sun.w*illumination.x;
    vec3 color=albedo.rgb*(0.04+camera.sky.w*illumination.x+0.8*direct);
    color+=albedo.rgb*vec3(1,0.72,0.42)*illumination.y;
    float fog=clamp((viewDistance-camera.fog.x)/(camera.fog.y-camera.fog.x),0,1);
    outColor=vec4(mix(color,camera.sky.rgb,fog),albedo.a);
}
