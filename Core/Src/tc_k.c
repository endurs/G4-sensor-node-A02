#include "tc_k.h"
#include <math.h>

/* ========== Internal helpers ========== */

static inline float clampf(float x, float lo, float hi) {
  return (x < lo) ? lo : (x > hi) ? hi : x;
}

/* Horner evaluation for float polynomials: c[0] + c[1]x + ... + c[n-1]x^{n-1}  */
static inline float hornerf(const float *c, int n, float x) {
  float y = c[n-1];
  for (int i = n-2; i >= 0; --i) y = y * x + c[i];
  return y;
}

/* ========= ITS-90 Type K (units exactly as noted) =========
   Direct T->E (microvolts) with the extra exp term above 0°C,
   and inverse E->T over three ranges.

   Coefficients from ITS-90/NIST (Type K):
   - Direct (T->E): E[µV] = Σ c_i * T^i  (T in °C), with α0*exp(α1*(T-126.9686)^2) added for T >= 0°C
   - Inverse (E->T): T[°C] = Σ d_i * E^i (E in µV), piecewise for E ranges
*/

/* Direct T->E coefficients (microvolts). */
static const float K_DIR_NEG_C[] = { /* -270°C .. 0°C */
  0.0000000000e+0f,
  3.9450128025e+1f,
  2.3622373598e-2f,
 -3.2858906784e-4f,
 -4.9904828777e-6f,
 -6.7509059173e-8f,
 -5.7410327428e-10f,
 -3.1088872894e-12f,
 -1.0451609365e-14f,
 -1.9889266878e-17f,
 -1.6322697486e-20f
};

static const float K_DIR_POS_C[] = { /* 0°C .. 1372°C */
 -1.7600413686e+1f,
  3.8921204975e+1f,
  1.8558770032e-2f,
 -9.9457592874e-5f,
  3.1840945719e-7f,
 -5.6072844889e-10f,
  5.6075059059e-13f,
 -3.2020720003e-16f,
  9.7151147152e-20f,
 -1.2104721275e-23f
};

/* Magnetic-ordering Gaussian term (only for T >= 0°C) */
static const float K_DIR_ALPHA0 = 1.185976e+2f;
static const float K_DIR_ALPHA1 = -1.183432e-4f;

/* Inverse E->T coefficients (microvolts to degC), three ranges. */
static const float K_INV_E_NEG[] = { /* -5891 µV .. 0 µV  (T ≈ -200°C .. 0°C) */
  0.0000000e+0f,
  2.5173462e-2f,
 -1.1662878e-6f,
 -1.0833638e-9f,
 -8.9773540e-13f,
 -3.7342377e-16f,
 -8.6632643e-20f,
 -1.0450598e-23f,
 -5.1920577e-29f
};

static const float K_INV_E_MID[] = { /* 0 .. 20644 µV (T ≈ 0°C .. 500°C) */
  0.0000000e+0f,
  2.5083550e-2f,
  7.8601060e-8f,
 -2.5031310e-10f,
  8.3152700e-14f,
 -1.2280340e-17f,
  9.8040360e-22f,
 -4.4130300e-26f,
  1.0577340e-30f,
 -1.0527550e-35f
};

static const float K_INV_E_HI[] = { /* 20644 .. 54886 µV (T ≈ 500°C .. 1372°C) */
 -1.3180580e+2f,
  4.8302220e-2f,
 -1.6460310e-6f,
  5.4647310e-11f,
 -9.6507150e-16f,
  8.8021930e-21f,
 -3.1108100e-26f
};

/* ========= Public functions ========= */

void tc_k_init(tc_k_ctx_t *ctx, tc_k_cfg_t cfg) {
  if (!ctx) return;
  ctx->cfg = cfg;
}

float tc_k_emf_uV_from_temp_C(float t_degC) {
  /* Clamp to ITS-90 range to avoid runaway exponent. */
  float T = clampf(t_degC, -270.0f, 1372.0f);

  if (T < 0.0f) {
    /* Horner on negative-range polynomial */
    return hornerf(K_DIR_NEG_C, (int)(sizeof(K_DIR_NEG_C)/sizeof(K_DIR_NEG_C[0])), T);
  } else {
    float poly = hornerf(K_DIR_POS_C, (int)(sizeof(K_DIR_POS_C)/sizeof(K_DIR_POS_C[0])), T);
    float gauss = K_DIR_ALPHA0 * expf(K_DIR_ALPHA1 * (T - 126.9686f) * (T - 126.9686f));
    return poly + gauss;
  }
}

float tc_k_temp_C_from_emf_uV(float e_uV) {
  /* Clamp to valid E range for Type K direct tables. */
  float E = clampf(e_uV, -5891.0f, 54886.0f);

  if (E < 0.0f) {
    return hornerf(K_INV_E_NEG, (int)(sizeof(K_INV_E_NEG)/sizeof(K_INV_E_NEG[0])), E);
  } else if (E < 20644.0f) {
    return hornerf(K_INV_E_MID, (int)(sizeof(K_INV_E_MID)/sizeof(K_INV_E_MID[0])), E);
  } else {
    return hornerf(K_INV_E_HI,  (int)(sizeof(K_INV_E_HI)/sizeof(K_INV_E_HI[0])),  E);
  }
}

float tc_k_convert(const tc_k_ctx_t *ctx, float v_adc, float cj_degC) {
  /* 1) Remove output-side bias, then un-gain to get TC emf (at the junction) in volts. */
  float v_out = v_adc - (ctx ? ctx->cfg.v_offset : 0.0f);   /* [V] */
  float gain  = (ctx && ctx->cfg.gain > 0.0f) ? ctx->cfg.gain : 1.0f;
  float v_tc  = v_out / gain;                                /* [V] at the actual TC (emf) */

  /* 2) Convert to µV (ITS-90 polynomials use microvolts). */
  float e_tc_uV = v_tc * 1.0e6f;

  /* 3) Convert CJ temperature to its equivalent emf (µV), add to measured emf. */
  float e_cj_uV = tc_k_emf_uV_from_temp_C(cj_degC);
  float e_sum_uV = e_tc_uV + e_cj_uV;

  /* 4) Inverse polynomial: E_total -> hot-junction temp [°C]. */
  float t_hot = tc_k_temp_C_from_emf_uV(e_sum_uV);

  /* Final clamp to physical range. */
  return clampf(t_hot, -270.0f, 1372.0f);
}

void tc_k_convert4(const tc_k_ctx_t *ctx,
                   const float v_adc[4],
                   const float cj_degC[4],
                   float t_hot_degC[4]) {
  for (int i = 0; i < 4; ++i) {
    t_hot_degC[i] = tc_k_convert(ctx, v_adc[i], cj_degC[i]);
  }
}
