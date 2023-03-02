// x-run: ~/scripts/runc.sh % -lraylib -ltcc -lm --- mushrooms.c
#include <math.h>
#include <raylib.h>
#include <raymath.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <libtcc.h>
#include <assert.h>
#include <sys/types.h>
#define STB_DS_IMPLEMENTATION
#include <stb/stb_ds.h>
#include "ibmfont.h"

#define BUFFER_SIZE 2048
#define SAMPLE_RATE 192000

typedef void (*soundgen_fun_t)(double t, double dt, float *l, float *r);

struct input_param {
  char name[16];
  double min, max;
  double *value;
  double target;
};

static struct generator {
  TCCState *tcc;
  soundgen_fun_t func;
  struct input_param *params;
  char **errors; // TODO: make use of it
} generator;

static int width = 800, height = 600;
static float samples_buffer[BUFFER_SIZE * 2];
off_t samples_offset = 0;
double time = 0.0l;

void soundgen_thread(short *buf, unsigned int frames);
bool compile_generator(const char *contents);
void tcc_error_callback(void *opaque, const char *error);

double approachExp(double a, double b, double ratio) {
  return a+(b-a)*ratio;
}
double approachLinear(double a, double b, double max) {
  return (a > b) ?
    (a - b < max ? b : a-max) :
    (b - a < max ? b : a+max);
}

static Vector2 plus_shape_offsets[4] = {
  { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 }
};

const struct theme {
  struct theme_oscilloscope {
    Color bg, fg, grid, text, over;
  } osci;
} theme = {
  .osci.bg = { 0x07, 0x0a, 0x09, 0xff },    // #070a09
  .osci.fg = { 0x77, 0xff, 0x99, 0xff },    // #77ff99
  .osci.grid = { 0x80, 0x80, 0xe0, 0x40 },  // #8080e0
  .osci.text = { 0x52, 0x52, 0xdf, 0xff },  // #5252df
  .osci.over = { 0xdf, 0x52, 0x52, 0xff },  // #df5252
};

FILE *file_out = NULL;

int main(int argc, char **argv) {
  SetConfigFlags(FLAG_WINDOW_RESIZABLE);
  InitWindow(width, height, "");
  InitAudioDevice();
  SetTargetFPS(120);

  Font font = LoadFont_IBM();
  RenderTexture2D current = LoadRenderTexture(fmin(width, height), fmin(width, height));

  AudioStream soundgen = LoadAudioStream(SAMPLE_RATE, 16, 2);
  SetAudioStreamCallback(soundgen, (AudioCallback)soundgen_thread);
  PlayAudioStream(soundgen);

  {
    char *text = LoadFileText(argv[1]);
    printf("Loaded file %s: %p\n", argv[1], text);
    assert(compile_generator(text));
    UnloadFileText(text);
  }

  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(BLACK);
    Vector2 mouse = GetMousePosition();
    {
      if (IsWindowResized()) {
        width = GetScreenWidth();
        height = GetScreenHeight();
        int sz = fmin(width, height);
        UnloadRenderTexture(current);
        current = LoadRenderTexture(sz, sz);
      }

      if (IsKeyPressed(KEY_W)) {
        if (file_out) {
          fflush(file_out);
          fclose(file_out);
          file_out = NULL;
        } else {
          file_out = fopen("samples.data", "wb");
        }
      }

      int n_params = 0;
      for (; generator.params[n_params].value; n_params++) ;

      if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_C)) {
        int buf_size = (sizeof(double) + 16 + 8) * n_params, compr_size, b64_size;
        struct {
          char name[16];
          double value;
          uint8_t dummy[8];
        } *buffer = MemAlloc(buf_size);
        for (int i = 0; generator.params[i].value; i++) {
          strncpy(buffer[i].name, generator.params[i].name, 16);
          buffer[i].value = *generator.params[i].value;
        }

        uint8_t *compressed = CompressData((void*)buffer, buf_size, &compr_size);
        MemFree(buffer);

        char *b64 = EncodeDataBase64(compressed, compr_size, &b64_size);
        MemFree(compressed);
        b64[b64_size] = '\0';

        SetClipboardText(TextFormat("OSCI#%s", b64));
        MemFree(b64);
      }

      if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_V)) {
        int compr_size, data_size;
        const char *clip = GetClipboardText();
        if (0 == strncmp(clip, "OSCI#", 5)) {
          uint8_t *compressed = DecodeDataBase64((uint8_t*)clip + 5, &compr_size);
          struct {
            char name[16];
            double value;
            uint8_t dummy[8];
          } *buffer = (void *)DecompressData(compressed, compr_size, &data_size);
          MemFree(compressed);
          for (int i = 0; i < data_size / sizeof(buffer[0]); i++) {
            for (int j = 0; j < n_params; j++) {
              if (0 == strncmp(buffer[i].name, generator.params[j].name, 16)) {
                generator.params[j].target = buffer[i].value;
              }
            }
          }
          MemFree(buffer);
        }
      }

      if (IsKeyPressed(KEY_R)) {
        for (int i = 0; generator.params[i].value; i++) {
          double range = generator.params[i].max - generator.params[i].min;
          generator.params[i].target = generator.params[i].min + GetRandomValue(0, 10000) * range / 10000.0l;
        }
      }

      BeginTextureMode(current);
      BeginBlendMode(BLEND_ADDITIVE);
      {
        Vector2 sz = { current.texture.width, current.texture.height };
        Vector2 center = { sz.x / 2.0f, sz.y / 2.0f };
        ClearBackground(theme.osci.bg);

        const int steps = 10;
        for (int i = -steps; i <= steps; i++) {
          DrawLine(
              center.x - sz.x * 0.375, center.y - sz.y * 0.75 * i / (float)steps,
              center.x + sz.x * 0.375, center.y - sz.y * 0.75 * i / (float)steps,
              theme.osci.grid);
          DrawLine(
              center.x - sz.x * 0.75 * i / (float)steps, center.y - sz.y * 0.375,
              center.x - sz.x * 0.75 * i / (float)steps, center.y + sz.y * 0.375,
              theme.osci.grid);
        }

        DrawTextEx(font, "-1", (Vector2) { center.x - sz.x * 0.375 - 16, center.y - 3 }, 8, 0, theme.osci.text);
        DrawTextEx(font, "+1", (Vector2) { center.x + sz.x * 0.375 + 1, center.y - 3 },  8, 0,  theme.osci.text);
        DrawTextEx(font, "-1", (Vector2) { center.x - 8, center.y + sz.y * 0.375 + 1 },  8, 0,  theme.osci.text);
        DrawTextEx(font, "+1", (Vector2) { center.x - 8, center.y - sz.y * 0.375 - 8 }, 8, 0, theme.osci.text);

        for (int i = 0; i < BUFFER_SIZE; i++) {
          Vector2 val = { samples_buffer[i * 2], samples_buffer[i * 2 + 1] };
          Vector2 point = {
            center.x + 0.75 * val.x * sz.x / 2.0,
            center.y - 0.75 * val.y * sz.y / 2.0
          };

          float brightness = powf(i / (float)BUFFER_SIZE, 2.0) * 0.25;
          Color color = theme.osci.fg;
          if (fabsf(val.x) >= 1.0 || fabsf(val.y) >= 1.0) {
            color = theme.osci.over;
            brightness = 1.0;
          }

          DrawPixelV(point, Fade(color, brightness));
          for (int j = 0; j < 4; j++) {
            DrawPixelV(Vector2Add(point, plus_shape_offsets[j]), Fade(color, brightness * 0.25));
          }
        }
      }
      EndBlendMode();
      EndTextureMode();

      DrawTextureRec(current.texture,
          (Rectangle) { 0, 0, current.texture.width, -current.texture.height },
          (Vector2) { 0, 0 }, WHITE);

      for (int i = 0; generator.params[i].value; i++) {
        Rectangle rec = { 0, i * 20, 400, 18 };
        struct input_param *param = &generator.params[i];
        bool selected = CheckCollisionPointRec(mouse, rec);

        DrawRectangle(rec.x, rec.y, rec.width * (*param->value - param->min) / (param->max - param->min), rec.height, Fade(DARKPURPLE, 0.25));
        DrawRectangleLinesEx(rec, 1.0, Fade(selected ? BLUE : DARKBLUE, 0.25));

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && selected) {
          param->target = param->min + ((mouse.x - rec.x) / rec.width) * (param->max - param->min);
        }

        double range = param->max - param->min;

        float scroll = GetMouseWheelMove() * range / 10000.0l;
        if (IsKeyDown(KEY_LEFT_CONTROL)) scroll *= 10.0f;
        if (IsKeyDown(KEY_LEFT_SHIFT)) scroll /= 10.0f;

        if (selected) param->target += scroll;

        if (param->target > param->max) param->target = param->max;
        if (param->target < param->min) param->target = param->min;

        DrawTextEx(font,
            TextFormat("%16s: %14.6lf (%14.6lf)", param->name, param->target, *param->value),
            (Vector2) { rec.x + 2, rec.y + 5 }, 8, 0, Fade(WHITE, selected ? 0.5 : 0.25));
      }

      DrawTextEx(font, TextFormat("%3dFPS", GetFPS()), (Vector2) { 8, height - 16 }, 8, 0, RED);
      if (file_out != NULL) {
        DrawTextEx(font, "REC", (Vector2) { 8, height - 32 }, 8, 0, RED);
      }
    }
    EndDrawing();
  }
}

bool compile_generator(const char *code) {
  if (generator.tcc != NULL)
    tcc_delete(generator.tcc);

  if (generator.errors != NULL) {
    for (int i = 0; i < arrlen(generator.errors); i++)
      free(generator.errors[i]);
    arrfree(generator.errors);
  }

  if ((generator.tcc = tcc_new()) == NULL)
    return false;

  tcc_set_output_type(generator.tcc, TCC_OUTPUT_MEMORY);

  tcc_set_error_func(generator.tcc, NULL, tcc_error_callback);

  tcc_add_library(generator.tcc, "m");

  if (-1 == tcc_compile_string(generator.tcc, code))
    return false;

  if (-1 == tcc_relocate(generator.tcc, TCC_RELOCATE_AUTO))
    return false;

  generator.func = tcc_get_symbol(generator.tcc, "generate");
  assert(generator.func != NULL);
  generator.params = tcc_get_symbol(generator.tcc, "params");
  assert(generator.params != NULL);

  return true;
}

void tcc_error_callback(void *opaque, const char *error) {
  printf("\033[91mSHIT HAPPENED: %s\033[0m\n", error);
  arrput(generator.errors, strdup(error));
}

void soundgen_thread(short *buf, unsigned int frames) {
  float l, r;

  for (int i = 0; i < frames; i++) {
    for (int j = 0; generator.params[j].value; j++) {
      struct input_param param = generator.params[j];
      if (param.target >= param.min) {
        *param.value = approachExp(*param.value, param.target, 25.0 / (float)SAMPLE_RATE);
      }
    }

    generator.func(time, 1.0l / SAMPLE_RATE, &l, &r);
    samples_buffer[(i * 2 + 0 + samples_offset) % (BUFFER_SIZE * 2)] = l;
    samples_buffer[(i * 2 + 1 + samples_offset) % (BUFFER_SIZE * 2)] = r;
    buf[i * 2 + 0] = l * 32767.0f;
    buf[i * 2 + 1] = r * 32767.0f;
    time += 1.0l / SAMPLE_RATE;
  }

  if (file_out != NULL) {
    fwrite(buf, sizeof(short) * 2, frames, file_out);
  }

  samples_offset += frames * 2;
}
