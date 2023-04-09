// x-run: ~/scripts/runc.sh % -lraylib -lm -lfftw3
#include <fftw3.h>
#include <math.h>
#include <raylib.h>
#include <raymath.h>
#include <string.h>
#include "ibmfont.h"

#define N_SAMPLES 4096

static struct {
  RenderTexture2D screen;
  struct {
    Color border;
    Color window_bg;
    Color title_bg;
    Color body_bg;
    Color bingle_bg;
  } colors;
  struct {
    Shader shader;
    int loc_size, loc_time, loc_curvature, loc_vignette, loc_fft;
    float curvature, vignette;
    long mod_time;
    float fft[N_SAMPLES / 2];
  } shader;
  Font font;
} state = {
  .colors.border = { 0x90, 0xf4, 0xe4, 0xff },
  .colors.window_bg = { 0x4d, 0x23, 0xcf, 0xff },
  .colors.title_bg = { 0xf0, 0xd1, 0xf1, 0xff },
  .colors.bingle_bg = { 0xea, 0xa0, 0xef, 0xff },
  .shader.curvature = 0.1,
  .shader.vignette = 4.0,
  .shader.mod_time = 0
};

Rectangle DrawWindowOutside(Rectangle rec);
bool ShaderHotReload(Shader *shader, const char *filename, long *lastmod);

Rectangle newRectangle(float x, float y, float w, float h) {
  return (Rectangle) { x, y, w, h };
}

static double sound_samples[N_SAMPLES], sound_freqs[N_SAMPLES];
long sound_offset = 0;

void audio_consumer(void *buffer, unsigned int frames) {
  float *buf = buffer;
  for (int i = 0; i < frames; i++) {
    sound_samples[(i + sound_offset) % N_SAMPLES] = buf[i * 2];
  }
  sound_offset += frames;
}

int main(void) {
  SetConfigFlags(FLAG_WINDOW_TRANSPARENT | FLAG_WINDOW_UNDECORATED);
  InitWindow(800, 600, "Windose.exe");
  SetWindowMinSize(640, 480);
  SetTargetFPS(60);
  HideCursor();
  InitAudioDevice();
  state.font = LoadFont_IBM();

  const char *text = "Hey, this mod is made by EXPL0Si0NTiM3R\n"
                      "Modarchive ID: 91674, he's currently known as exiphase\n"
                      "Say hi to him if you can!\n"
                      "Um anyways, vfx made by me, kc/hkc/hatkidchan.\n"
                      "Song name is \"ame\" btw. Modarchive ID: 181858.";
  char *textbuf = strdup(text);
  memset(textbuf, 0, strlen(text));
  int text_sleep = -1;

  state.screen = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());

  Texture2D prev_frame;


  Music music = LoadMusicStream("./assets/expl0si0ntim3r_-_ame.xm");
  music.looping = true;
  AttachAudioStreamProcessor(music.stream, (AudioCallback)audio_consumer);

  for (int frame = 0; !WindowShouldClose(); frame++) {

    if (IsKeyPressed(KEY_SPACE)) {
      PlayMusicStream(music);
      text_sleep = 10;
    }

    UpdateMusicStream(music);
    fftw_plan fft_plan = fftw_plan_r2r_1d(N_SAMPLES, sound_samples, sound_freqs, FFTW_DHT, FFTW_ESTIMATE);
    fftw_execute(fft_plan);

    if ((frame % 30) == 0) {
      ShaderHotReload(&state.shader.shader, "windose.frag", &state.shader.mod_time);
      int size[2] = { state.screen.texture.width, state.screen.texture.height };
      state.shader.loc_size = GetShaderLocation(state.shader.shader, "screenSize");
      state.shader.loc_time = GetShaderLocation(state.shader.shader, "time");
      state.shader.loc_curvature = GetShaderLocation(state.shader.shader, "curvature");
      state.shader.loc_vignette = GetShaderLocation(state.shader.shader, "vignette_w");
      state.shader.loc_fft = GetShaderLocation(state.shader.shader, "fft");
      SetShaderValue(state.shader.shader, state.shader.loc_size, size, SHADER_UNIFORM_IVEC2);
    }

    if (IsWindowResized() || frame == 0) {
      UnloadRenderTexture(state.screen);
      UnloadTexture(prev_frame);
      state.screen = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
      Image image = LoadImageFromScreen();
      prev_frame = LoadTextureFromImage(image);
      UnloadImage(image);
      int size[2] = { state.screen.texture.width, state.screen.texture.height };
      SetShaderValue(state.shader.shader, state.shader.loc_size, size, SHADER_UNIFORM_IVEC2);
    }
    Vector2 mouse = GetMousePosition();

    if (strlen(textbuf) < strlen(text) & text_sleep >= 0) {
      if (text_sleep-- <= 0) {
        char sym = text[strlen(textbuf)];
        switch (sym ) {
          case '\n': text_sleep = 60; break;
          case ',':
          case '/':
          case '.': text_sleep = 15; break;
          default: text_sleep = sym & 7; break;
        }
        textbuf[strlen(textbuf)] = sym;
      }
    }

    BeginDrawing();

    {
      Image screen = LoadImageFromScreen();
      UpdateTexture(prev_frame, screen.data);
      UnloadImage(screen);
    }


    BeginTextureMode(state.screen);
    {
      ClearBackground(BLANK);
      Rectangle dest = {
        0, 0, state.screen.texture.width, state.screen.texture.height
      };
      Rectangle rec = DrawWindowOutside(dest);
      DrawTexturePro(prev_frame, newRectangle(0, 0, prev_frame.width, prev_frame.height), rec, Vector2Zero(), 0.0, state.colors.window_bg);

      DrawTextEx(state.font, textbuf, (Vector2) { rec.x + 8, rec.y + 8 }, 8, 0, WHITE);

      double fft_max = (float)N_SAMPLES / 2.0;
      /*for (int i = 0; i < N_SAMPLES / 2; i++) {*/
      /*  if (fabs(sound_freqs[i]) > fft_max) fft_max = fabs(sound_freqs[i]);*/
      /*}*/
      /*if (fft_max == 0.0) fft_max = 1.0;*/

      static Vector2 line[N_SAMPLES / 2];
      for (int i = 0; i < N_SAMPLES / 2; i++) {
        line[i].x = rec.width * (float)i / (float)(N_SAMPLES / 2.0) + rec.x;

        float v = state.shader.fft[i] = sound_freqs[i] / fft_max;
        line[i].y = (0.5 - pow(v, 0.5) * 0.5) * rec.height + rec.y;
      }
      DrawLineStrip(line, N_SAMPLES / 2, WHITE);

      if (CheckCollisionPointRec(mouse, dest)) {
        if (CheckCollisionPointRec(mouse, rec)) {
          // inside
        } else {
          // outside
        }
      }

      DrawCircleV(mouse, 3.0, WHITE);
      /*DrawFPS(8, 8);*/
    }
    EndTextureMode();

    float time = GetTime();
    SetShaderValue(state.shader.shader, state.shader.loc_time, &time, SHADER_UNIFORM_FLOAT);
    SetShaderValue(state.shader.shader, state.shader.loc_curvature, &state.shader.curvature, SHADER_UNIFORM_FLOAT);
    SetShaderValue(state.shader.shader, state.shader.loc_vignette, &state.shader.vignette, SHADER_UNIFORM_FLOAT);
    SetShaderValueV(state.shader.shader, state.shader.loc_fft, state.shader.fft, SHADER_UNIFORM_FLOAT, N_SAMPLES / 2);
    BeginShaderMode(state.shader.shader);
    ClearBackground(BLANK);
    DrawTexture(state.screen.texture, 0, 0, WHITE);
    EndShaderMode();

    EndDrawing();
    fftw_destroy_plan(fft_plan);
  }
}

Rectangle DrawWindowOutside(Rectangle rec) {
  DrawRectangleRec(rec, state.colors.window_bg);
  DrawRectangleLinesEx(rec, 2.0, state.colors.border);

  DrawRectangleRec(newRectangle(rec.x + 6, rec.y + 6, rec.width - 12, 30), state.colors.title_bg);
  DrawRectangleLinesEx(newRectangle(rec.x + 6, rec.y + 6, rec.width - 12, 30), 2, state.colors.border);

  DrawRectangleRec(newRectangle(rec.x + 6, rec.y + 40, rec.width - 12, rec.height - 56), GRAY);
  DrawRectangleLinesEx(newRectangle(rec.x + 6, rec.y + 40, rec.width - 12, rec.height - 56), 2, state.colors.border);

  DrawRectangleRec(newRectangle(rec.x + 6, rec.y + rec.height - 12, 36, 12), state.colors.bingle_bg);
  DrawRectangleLinesEx(newRectangle(rec.x + 6, rec.y + rec.height - 12, 36, 12), 2, state.colors.border);

  return newRectangle(rec.x + 8, rec.y + 42, rec.width - 16, rec.height - 60);
}

bool ShaderHotReload(Shader *shader, const char *filename, long *lastmod) {
  long modified = GetFileModTime(filename);

  if (modified > *lastmod) {
    *lastmod = modified;

    UnloadShader(*shader);

    *shader = LoadShader(0, filename);

    return true;
  }

  return false;
}
