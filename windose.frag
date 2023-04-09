#version 330
#define M_PI 3.14159265358979323846

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform ivec2 screenSize;
uniform float time;
uniform float curvature;
uniform float vignette_w;
uniform float fft[2048];

out vec4 finalColor;

void main() {
  vec2 uv = vec2(fragTexCoord.x, 1.0 - fragTexCoord.y);

  uv = (uv * 2.0f - 1.0f);
  vec2 off = uv.yx / (1.0 / curvature);
  uv += uv * off * off;
  uv = uv * 0.5f + 0.5f;

  uv.x += fft[int(uv.y * 1024) % 128] * 0.005;

  finalColor = texture(texture0, uv);
  if (uv.x < 0.0 || uv.y < 0.0 || uv.x >= 1.0 || uv.y >= 1.0) {
    finalColor.a = 0.0;
  }

  vec2 vignette = vec2(vignette_w) / vec2(screenSize);
  vignette = smoothstep(vec2(0.0), vignette, vec2(1.0) - abs(uv));

  float phase = uv.y * float(screenSize.y) * 2.0 + uv.x;
  phase += floor(cos(time * M_PI * 16.0 * 2.0)) * M_PI;
  finalColor.rg *= (sin(phase) * 0.25 + 0.75);
  finalColor.b *= (cos(phase) * 0.25 + 0.75);

  finalColor.rgba *= vignette.x * vignette.y;
  finalColor.r *= 1.0 - abs(fft[int((1.0 - uv.y) * 128)]);
  finalColor.gb *= 1.0 + abs(fft[int((1.0 - uv.y) * 128)]);
  finalColor.rgb *= (cos(time * M_PI * 44.0 * 2.0) * 0.025 + 0.975);
}
