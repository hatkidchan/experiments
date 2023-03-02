// x-run: ./raylib-audio-visualization %
#include <math.h>

static double freq = 440.0l;
static double scan_freq_mul = 0.0l;
static double stem_freq_mul = 0.0l;
static double hat_freq_mul = 0.0l;
static double disjoint_x_freq_mul = 0.0l;
static double disjoint_y_freq_mul = 0.0l;

static double ampl = 0.0l;
static double scan_ampl = 0.0l;
static double stem_ampl = 0.0l;
static double hat_ampl = 0.25l;
static double disjoint_x_ampl = 0.0l;
static double disjoint_y_ampl = 0.0l;

static double hat_perc = 0.25l;

static double scan_phase = 0.0l;
static double stem_phase = 0.0l;
static double hat_phase = 0.0l;
static double disjoint_x_phase = 0.0l;
static double disjoint_y_phase = 0.0l;

struct input_param {
  char name[16];
  double min, max;
  double *value;
  double target;
} params[] = {
  { "freq", 0.0l, 10000.0l, &freq, 4000.0l },
  { "hat_f", 0.0l, 2.0l, &hat_freq_mul, 1.0l },
  { "hat_x", 0.0l, 1.0l, &hat_ampl, 0.25l },
  { "hat_p", 0.0l, 1.0l, &hat_perc, 0.6l },
  { "stem_f", 0.0l, 2.0l, &stem_freq_mul, 1.0l },
  { "stem_x", 0.0l, 0.1l, &stem_ampl, 0.01l },
  { "scan_f", 0.0l, 2.0l, &scan_freq_mul, 0.1l },
  { "scan_x", 0.0l, 1.0l, &scan_ampl, 0.25l },
  { "ampl_all", 0.0l, 1.0l, &ampl, 1.0l },
  { "disj_x", 0.0l, 0.5l, &disjoint_x_ampl, 0.0l },
  { "disj_y", 0.0l, 0.5l, &disjoint_y_ampl, 0.0l },
  { "disj_xf", 0.0l, 1.0l, &disjoint_x_freq_mul, 0.5l },
  { "disj_yf", 0.0l, 1.0l, &disjoint_y_freq_mul, 0.25l },
  { "", 0.0, 0.0, 0 },
};

double saw(double dt) {
  return fmod(dt, M_PI * 2.0) / M_PI - 1.0;
}

double square(double dt) {
  return sin(dt) < 0.0 ? -1.0 : 1.0;
}

static double lfo_phase = 0.0l;
void generate(double t, double dt, float *l, float *r) {
  /*lfo_phase  += M_PI * 2.0 * dt * 0.1;*/
  /*params[0].target = sin(lfo_phase) * 1000.0l + 3000.0l;*/
  scan_phase += M_PI * 2.0 * dt * freq * scan_freq_mul;
  stem_phase += M_PI * 2.0 * dt * freq * stem_freq_mul;
  hat_phase  += M_PI * 2.0 * dt * freq * hat_freq_mul;
  disjoint_x_phase += M_PI * 2.0 * dt * freq * disjoint_x_freq_mul * scan_freq_mul;
  disjoint_y_phase += M_PI * 2.0 * dt * freq * disjoint_y_freq_mul * scan_freq_mul;

  if ((saw(scan_phase) + 1.0) <= hat_perc * 2.0)
    *l = sin(stem_phase) * stem_ampl * ampl;
  else
    *l = sin(hat_phase) * hat_ampl * sin(scan_phase) * ampl;
  *l += square(disjoint_x_phase) * disjoint_x_ampl;
  *r = saw(scan_phase) * scan_ampl * ampl + square(disjoint_y_phase) * disjoint_y_ampl;
}
