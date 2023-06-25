// x-run: ~/scripts/runc.sh % -lraylib -lpthread
#include <stdint.h>
#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#define WORLD_WIDTH 1366
#define WORLD_HEIGHT 768

#define W_GET(ARR, X, Y) (((X) < 0 || (Y) < 0 || (X) >= WORLD_WIDTH || (Y) >= WORLD_HEIGHT) ? (union color_rgb565){ .color = 0 } : ARR[(X) + (Y) * WORLD_WIDTH])
#define W_SET(ARR, X, Y, VAL) { \
  if ((X) >= 0 && (Y) >= 0 && (X) < WORLD_WIDTH && (Y) < WORLD_HEIGHT) { \
    ARR[(X) + (Y) * WORLD_WIDTH].color = VAL.color;\
  }\
}

union color_rgb565 {
  uint16_t color;
  struct rgb565 {
    uint16_t r : 5;
    uint16_t g : 6;
    uint16_t b : 5;
  } rgb;
};

struct v2i { int x, y; };

struct world {
  union color_rgb565 prev[WORLD_WIDTH * WORLD_HEIGHT];
  union color_rgb565 curr[WORLD_WIDTH * WORLD_HEIGHT];

  RenderTexture rtex;
  pthread_mutex_t mut_world;
} world = {
  .mut_world = PTHREAD_MUTEX_INITIALIZER
};

Color color_565rgb(union color_rgb565);
union color_rgb565 color_rgb565(Color v);
void mutate_color(union color_rgb565 *c) {
  union color_rgb565 d = { .color = rand() & 0xFFFF };
  d.rgb.r >>= 3;
  d.rgb.g >>= 4;
  d.rgb.b >>= 3;

  if (rand() % 100 < 20) { c->rgb.r = c->rgb.r + d.rgb.r - 1; }
  if (rand() % 100 < 20) { c->rgb.g = c->rgb.g + d.rgb.g - 2; }
  if (rand() % 100 < 20) { c->rgb.b = c->rgb.b + d.rgb.b - 1; }

}

void *world_update(void *_) {

  while (true) {
    pthread_mutex_lock(&world.mut_world);
    for (int j = 0; j < 1000; j++) {
      int i = rand() % (WORLD_WIDTH * WORLD_HEIGHT);
      int x = i % WORLD_WIDTH, y = i / WORLD_WIDTH;
      union color_rgb565 c = world.curr[i];
      if (c.color != 0) {
        for (int ox = -1; ox <= 1; ox++) {
          for (int oy = -1; oy <= 1; oy++) {
            if (ox == 0 && oy == 0) continue;
            /*if ((ox * ox + oy * oy) > 1) continue;*/
            if (W_GET(world.curr, x + ox, y + oy).color != 0) continue;
            mutate_color(&c);
            W_SET(world.curr, x + ox, y + oy, c);
          }
        }
        mutate_color(&c);
        /*if (rand() % 100 <= 1)*/
        /*  W_SET(world.curr, x, y, c);*/
      }
    }
    pthread_mutex_unlock(&world.mut_world);
    usleep(1);
  }
  return NULL;
}

int main(void) {
  InitWindow(WORLD_WIDTH, WORLD_HEIGHT, "rgbgene");
  SetTargetFPS(60);

  srand(548392265);

  world.rtex = LoadRenderTexture(WORLD_WIDTH, WORLD_HEIGHT);

  for (int i = 0; i < 16; i++) {
    world.curr[rand() % (WORLD_WIDTH * WORLD_HEIGHT)].color = rand() & 0xFFFF;
  }

  pthread_t thrd_world;
  pthread_create(&thrd_world, NULL, world_update, NULL);

  while (!WindowShouldClose()) {
    BeginDrawing();

    BeginTextureMode(world.rtex);
    pthread_mutex_lock(&world.mut_world);
    for (int y = 0; y < WORLD_HEIGHT; y++) {
      for (int x = 0; x < WORLD_WIDTH; x++) {
        int i = x + y * WORLD_WIDTH;
        if (world.curr[i].color != world.prev[i].color) {
          DrawPixel(x, y, color_565rgb(world.curr[i]));
        }
      }
    }
    memcpy(world.prev, world.curr, sizeof(world.prev));
    pthread_mutex_unlock(&world.mut_world);
    EndTextureMode();

    if (IsKeyPressed(KEY_S)) {
      Image img = LoadImageFromTexture(world.rtex.texture);
      ExportImage(img, "rgbgene.png");
      UnloadImage(img);
    }

    {
      ClearBackground(BLACK);
      DrawTexture(world.rtex.texture, 0, 0, WHITE);
    }
    EndDrawing();
  }
}

Color color_565rgb(union color_rgb565 v) {
  return (Color) {
    .r = v.rgb.r << 3,
    .g = v.rgb.g << 2,
    .b = v.rgb.b << 3,
    .a = 255
  };
}

union color_rgb565 color_rgb565(Color v) {
  return (union color_rgb565){
    .rgb.r = v.r >> 3,
    .rgb.g = v.g >> 2,
    .rgb.b = v.b >> 3,
  };
}
