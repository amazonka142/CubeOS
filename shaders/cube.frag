#version 450

layout(location = 0) in vec3 vColor;
layout(location = 1) in vec2 vUV;
layout(location = 2) in vec3 vWorldPos;
layout(location = 3) in float vUiPass;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform UBO {
  mat4 model;
  mat4 view;
  mat4 proj;
  vec4 params;
  vec4 cameraData;
} ubo;

layout(binding = 1) uniform sampler2D texSampler;

float hash12(vec2 p) {
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float valueNoise(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);

  float a = hash12(i);
  float b = hash12(i + vec2(1.0, 0.0));
  float c = hash12(i + vec2(0.0, 1.0));
  float d = hash12(i + vec2(1.0, 1.0));

  vec2 u = f * f * (3.0 - 2.0 * f);
  return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float fbm(vec2 p) {
  float sum = 0.0;
  float amp = 0.5;
  float freq = 1.0;
  for (int i = 0; i < 4; ++i) {
    sum += valueNoise(p * freq) * amp;
    freq *= 2.0;
    amp *= 0.5;
  }
  return sum;
}

void main() {
  if (vColor.g < -0.5 && vColor.r >= 0.0) {
    // Texture-free UI primitive path (text glyphs/markers).
    float green = clamp(-vColor.g - 1.0, 0.0, 1.0);
    outColor = vec4(clamp(vColor.r, 0.0, 1.0), green, clamp(vColor.b, 0.0, 1.0), 1.0);
    return;
  }

  if (vColor.r < -0.5) {
    if (ubo.cameraData.w > 0.5) {
      outColor = vec4(0.0);
      return;
    }

    vec2 resolution = vec2(max(ubo.params.y, 1.0), max(ubo.params.z, 1.0));
    vec2 ndc = vec2(gl_FragCoord.x / resolution.x, gl_FragCoord.y / resolution.y) * 2.0 - 1.0;
    float t = ubo.params.x;

    vec4 clipPos = vec4(ndc, 1.0, 1.0);
    vec4 viewPos = inverse(ubo.proj) * clipPos;
    vec3 viewDir = normalize(viewPos.xyz / max(viewPos.w, 0.0001));
    vec3 worldDir = normalize((inverse(ubo.view) * vec4(viewDir, 0.0)).xyz);

    if (worldDir.y <= 0.03) {
      outColor = vec4(0.0);
      return;
    }

    vec2 domeUv = worldDir.xz / max(worldDir.y, 0.08);
    vec2 wind = vec2(t * 0.020, t * 0.007);
    float cloud = fbm(domeUv * 1.9 + wind);
    float puffs = smoothstep(0.56, 0.72, cloud);
    float horizonFade = smoothstep(0.05, 0.22, worldDir.y);
    float alpha = puffs * horizonFade * 0.46;

    outColor = vec4(vec3(0.95, 0.97, 1.0), alpha);
    return;
  }

  vec2 uv = vUV;
  vec4 texColor = texture(texSampler, uv);

  bool looksLikeWater =
    texColor.a < 0.95 &&
    vColor.b > (vColor.g + 0.12) &&
    vColor.g > vColor.r;

  if (!looksLikeWater && texColor.a < 0.05) {
    discard;
  }

  if (looksLikeWater) {
    float t = ubo.params.x;
    float waveX = sin((vUV.x + vUV.y) * 42.0 + t * 2.20) * 0.0026;
    float waveY = cos((vUV.y - vUV.x) * 55.0 - t * 1.85) * 0.0021;
    uv += vec2(waveX, waveY);
    texColor = texture(texSampler, uv);

    float shimmer = 0.5 + 0.5 * sin(t * 1.7 + vUV.x * 70.0 + vUV.y * 31.0);
    vec3 waterTint = vColor + vec3(0.03, 0.07, 0.11) * shimmer;
    outColor = vec4(waterTint, 1.0) * texColor;
    if (ubo.cameraData.w > 0.5) {
      outColor.a = max(outColor.a, 0.84);
    } else {
      // Keep water almost opaque from above to avoid ore "x-ray" effect.
      outColor.a = max(outColor.a, 0.97);
      outColor.rgb = mix(outColor.rgb, vec3(0.10, 0.26, 0.50), 0.20);
    }
  } else {
    outColor = vec4(vColor, 1.0) * texColor;
  }

  if (ubo.cameraData.w > 0.5 && vUiPass < 0.5) {
    float distToCamera = distance(vWorldPos, ubo.cameraData.xyz);
    // Minecraft-like underwater look: readable nearby blocks + strong blue falloff.
    float fog = 1.0 - exp(-max(0.0, distToCamera - 0.9) * 1.15);
    fog = clamp(fog, 0.0, 1.0);
    vec3 fogColor = vec3(0.08, 0.33, 0.56);
    vec3 tinted = outColor.rgb * vec3(0.84, 0.93, 1.05);
    outColor.rgb = mix(tinted, fogColor, fog);
    outColor.a = 1.0;
  }
}
