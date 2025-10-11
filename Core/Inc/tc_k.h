#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- Public types ---------- */
typedef struct {
  /* Front-end gain: Vout / Vin_TC (dimensionless). */
  float gain;

  /* Front-end output offset at the ADC input [V] corresponding to 0 µV TC emf
     (e.g., mid-rail bias). Set 0.0f if you have a truly zero-EMF output at 0°C CJ. */
  float v_offset;
} tc_k_cfg_t;

typedef struct {
  tc_k_cfg_t cfg;
} tc_k_ctx_t;

/* ---------- API ---------- */

/* Initialize context (pure data; thread-safe if callers use separate ctx). */
void tc_k_init(tc_k_ctx_t *ctx, tc_k_cfg_t cfg);

/* Convert one thermocouple reading to hot-junction temperature [°C].
   Arguments:
     ctx     : initialized context (gain, offset)
     v_adc   : ADC-side voltage [V] (post-gain, including any bias)
     cj_degC : cold junction temperature [°C]
   Returns:
     Hot-junction temperature [°C] (clamped to [-270, 1372]). */
float tc_k_convert(const tc_k_ctx_t *ctx, float v_adc, float cj_degC);

/* Vector helpers (convenient if you later run this in a thread). */
void tc_k_convert4(const tc_k_ctx_t *ctx,
                   const float v_adc[4],
                   const float cj_degC[4],   /* per-channel CJ (or all equal) */
                   float t_hot_degC[4]);

/* Low-level primitives (ITS-90; units noted in names) */
float tc_k_emf_uV_from_temp_C(float t_degC);   /* T[°C] -> E[µV] */
float tc_k_temp_C_from_emf_uV(float e_uV);     /* E[µV] -> T[°C] */

/* Optional utility if you prefer to start from ADC counts */
static inline float tc_counts_to_volts_u16(uint16_t counts, float vref_V) {
  return (counts * (vref_V / 65535.0f));
}

#ifdef __cplusplus
}
#endif
