#version 450

layout(binding = 0) uniform UBO {
  mat4 model;
  mat4 view;
  mat4 proj;
  mat4 invView;
  mat4 invProj;
  vec4 params;
  vec4 cameraData;
  vec4 weatherData;
  vec4 torchMeta;
  vec4 torchLights[16];
} ubo;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 vColor;
layout(location = 1) out vec2 vUV;
layout(location = 2) out vec3 vWorldPos;
layout(location = 3) out float vUiPass;

void main() {
  gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPos, 1.0);
  vColor = inColor;
  vUV = inUV;
  vWorldPos = inPos;
  vUiPass = 0.0;
}
