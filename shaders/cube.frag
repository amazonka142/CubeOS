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
  mat4 invView;
  mat4 invProj;
  vec4 params;
  vec4 cameraData;
  vec4 weatherData;
  vec4 torchMeta;
  vec4 torchLights[16];
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

float starField(vec2 domeUv, float time) {
  vec2 starUv = domeUv * 128.0;
  vec2 cell = floor(starUv);
  float rnd = hash12(cell);
  float star = step(0.9968, rnd);
  float twinkle = 0.55 + 0.45 * sin(time * 3.4 + rnd * 71.0);
  return star * twinkle;
}

void main() {
  float daylight = clamp(ubo.params.w, 0.0, 1.0);
  float weather = clamp(ubo.weatherData.x, 0.0, 1.0);
  float cloudCover = clamp(max(ubo.weatherData.y, weather), 0.0, 1.0);

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
    vec4 viewPos = ubo.invProj * clipPos;
    vec3 viewDir = normalize(viewPos.xyz / max(viewPos.w, 0.0001));
    vec3 worldDir = normalize((ubo.invView * vec4(viewDir, 0.0)).xyz);

    if (worldDir.y <= 0.03) {
      outColor = vec4(0.0);
      return;
    }

    vec2 domeUv = worldDir.xz / max(worldDir.y, 0.08);
    vec2 wind = vec2(t * 0.020, t * 0.007);
    float cloud = fbm(domeUv * (1.7 + cloudCover * 0.6) + wind);
    float threshold = mix(0.70, 0.50, cloudCover);
    float puffs = smoothstep(threshold - 0.09, threshold + 0.09, cloud);
    float horizonFade = smoothstep(0.05, 0.22, worldDir.y);
    float cloudAlpha = puffs * horizonFade * mix(0.28, 0.78, cloudCover);
    vec3 cloudColor = mix(vec3(0.15, 0.18, 0.28), vec3(0.95, 0.97, 1.0), daylight);
    cloudColor = mix(cloudColor, vec3(0.46, 0.48, 0.53), weather * 0.72);

    float cycle = fract(ubo.weatherData.z);
    float sunPhase = sin((cycle - 0.25) * 6.28318530718);
    float twilight = smoothstep(0.20, 0.95, 1.0 - abs(sunPhase));
    twilight *= (1.0 - smoothstep(0.15, 0.70, worldDir.y));
    twilight *= (1.0 - weather * 0.65);
    float twilightAlpha = twilight * 0.36;

    float stars = starField(domeUv + vec2(cycle * 2.3, 0.0), t);
    stars *= smoothstep(0.14, 0.74, worldDir.y);
    stars *= pow(1.0 - daylight, 1.8);
    stars *= (1.0 - cloudCover * 0.55) * (1.0 - weather);
    float starAlpha = stars * 0.72;

    float overlayAlpha = clamp(cloudAlpha + twilightAlpha + starAlpha, 0.0, 0.96);
    vec3 overlayColor = vec3(0.0);
    if (overlayAlpha > 0.0001) {
      overlayColor += cloudColor * cloudAlpha;
      overlayColor += vec3(0.96, 0.49, 0.19) * twilightAlpha;
      overlayColor += vec3(0.86, 0.90, 1.0) * starAlpha;
      overlayColor /= overlayAlpha;
    }

    outColor = vec4(overlayColor, overlayAlpha);
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
      outColor.a = clamp(outColor.a * 0.34, 0.16, 0.34);
      outColor.rgb = mix(outColor.rgb, vec3(0.09, 0.24, 0.44), 0.10);
    } else {
      // Keep water almost opaque from above to avoid ore "x-ray" effect.
      outColor.a = max(outColor.a, 0.97);
      outColor.rgb = mix(outColor.rgb, vec3(0.10, 0.26, 0.50), 0.20);
    }
  } else {
    outColor = vec4(vColor, 1.0) * texColor;
  }

  if (vUiPass < 0.5) {
    float ambient = mix(0.24, 1.0, daylight);
    ambient *= mix(1.0, 0.78, weather);

    float torchLight = 0.0;
    int torchCount = int(ubo.torchMeta.x + 0.5);
    for (int i = 0; i < 16; ++i) {
      if (i >= torchCount) {
        break;
      }
      vec4 light = ubo.torchLights[i];
      float range = max(light.w, 0.001);
      float dist = distance(vWorldPos, light.xyz);
      if (dist >= range) {
        continue;
      }
      float falloff = 1.0 - (dist / range);
      torchLight = max(torchLight, falloff * falloff);
    }

    if (looksLikeWater) {
      ambient = mix(ambient, min(1.12, ambient + 0.10), 0.45);
      torchLight *= 0.45;
    }
    vec3 baseLit = outColor.rgb * ambient;
    vec3 torchTint = vec3(1.18, 0.94, 0.62);
    baseLit += outColor.rgb * torchLight * 1.55 * torchTint;
    outColor.rgb = baseLit;
  }

  if (ubo.cameraData.w <= 0.5 && vUiPass < 0.5) {
    float distToCamera = distance(vWorldPos, ubo.cameraData.xyz);
    float rainFog = (1.0 - exp(-max(0.0, distToCamera - 5.0) * 0.06)) * weather;
    float nightFog = (1.0 - exp(-max(0.0, distToCamera - 12.0) * 0.025)) * (1.0 - daylight) * 0.45;
    float fog = clamp(rainFog + nightFog, 0.0, 0.82);
    vec3 fogColor = mix(vec3(0.06, 0.08, 0.14), vec3(0.67, 0.75, 0.85), daylight);
    fogColor = mix(fogColor, vec3(0.39, 0.42, 0.47), weather * 0.75);
    outColor.rgb = mix(outColor.rgb, fogColor, fog);
  }

  if (ubo.cameraData.w > 0.5 && vUiPass < 0.5) {
    float distToCamera = distance(vWorldPos, ubo.cameraData.xyz);
    // Keep the underwater look blue, but preserve nearby readability.
    float fogStart = looksLikeWater ? 1.8 : 2.8;
    float fogDensity = looksLikeWater ? 0.16 : 0.20;
    float fog = 1.0 - exp(-max(0.0, distToCamera - fogStart) * fogDensity);
    fog = clamp(fog, 0.0, looksLikeWater ? 0.42 : 0.76);
    vec3 fogColor = vec3(0.09, 0.30, 0.50);
    vec3 tinted = outColor.rgb * vec3(0.94, 0.98, 1.03) + vec3(0.008, 0.018, 0.028);
    outColor.rgb = mix(tinted, fogColor, fog);
    if (!looksLikeWater) {
      outColor.a = 1.0;
    }
  }
}
