#include <stddef.h>
#include <stdint.h>
#include "crypto_kem.h"
#include "randombytes.h"
#include "crypto_hash_sha3256.h"

#define NTRU_N 701
#define NTRU_LOGQ 13
#define PAD32(X) ((((X) + 31) / 32) * 32)
#define NTRU_Q (1 << NTRU_LOGQ)
#define MODQ(X) ((X) & (NTRU_Q - 1))
#define NTRU_SEEDBYTES 32
#define NTRU_PRFKEYBYTES 32
#define NTRU_SHAREDKEYBYTES 32
#define NTRU_SAMPLE_IID_BYTES (NTRU_N - 1)
#define NTRU_SAMPLE_FG_BYTES (2 * NTRU_SAMPLE_IID_BYTES)
#define NTRU_SAMPLE_RM_BYTES (2 * NTRU_SAMPLE_IID_BYTES)
#define NTRU_PACK_DEG (NTRU_N - 1)
#define NTRU_PACK_TRINARY_BYTES ((NTRU_PACK_DEG + 4) / 5)
#define NTRU_OWCPA_MSGBYTES (2 * NTRU_PACK_TRINARY_BYTES)
#define NTRU_OWCPA_SECRETKEYBYTES (2 * NTRU_PACK_TRINARY_BYTES + crypto_kem_PUBLICKEYBYTES)

typedef struct {
  uint16_t coeffs[NTRU_N];
} poly;

static void poly_Rq_mul(poly *r, const poly *a, const poly *b) {
  int k, i;
  for (k = 0; k < NTRU_N; k++) {
    r->coeffs[k] = 0;
    for (i = 1; i < NTRU_N - k; i++) r->coeffs[k] += a->coeffs[k + i] * b->coeffs[NTRU_N - i];
    for (i = 0; i < k + 1; i++) r->coeffs[k] += a->coeffs[k - i] * b->coeffs[i];
  }
}

static int16_t both_negative_mask(int16_t x, int16_t y) { return (x & y) >> 15; }

static uint16_t mod3(uint16_t a) {
  uint16_t r;
  int16_t t, c;
  r = (a >> 8) + (a & 0xff);
  r = (r >> 4) + (r & 0xf);
  r = (r >> 2) + (r & 0x3);
  r = (r >> 2) + (r & 0x3);
  t = r - 3;
  c = t >> 15;
  return (c & r) ^ (~c & t);
}

static void poly_mod_3_Phi_n(poly *r) {
  int i;
  for (i = 0; i < NTRU_N; i++) r->coeffs[i] = mod3(r->coeffs[i] + 2 * r->coeffs[NTRU_N - 1]);
}

static void poly_mod_q_Phi_n(poly *r) {
  int i;
  for (i = 0; i < NTRU_N; i++) r->coeffs[i] = r->coeffs[i] - r->coeffs[NTRU_N - 1];
}

static void poly_Rq_to_S3(poly *r, const poly *a) {
  int i;
  uint16_t flag;
  for (i = 0; i < NTRU_N; i++) {
    r->coeffs[i] = MODQ(a->coeffs[i]);
    flag = r->coeffs[i] >> (NTRU_LOGQ - 1);
    r->coeffs[i] += flag << (1 - (NTRU_LOGQ & 1));
  }
  poly_mod_3_Phi_n(r);
}

static void poly_Z3_to_Zq(poly *r) {
  int i;
  for (i = 0; i < NTRU_N; i++) r->coeffs[i] = r->coeffs[i] | ((-(r->coeffs[i] >> 1)) & (NTRU_Q - 1));
}

static void poly_trinary_Zq_to_Z3(poly *r) {
  int i;
  for (i = 0; i < NTRU_N; i++) {
    r->coeffs[i] = MODQ(r->coeffs[i]);
    r->coeffs[i] = 3 & (r->coeffs[i] ^ (r->coeffs[i] >> (NTRU_LOGQ - 1)));
  }
}

static void poly_Sq_mul(poly *r, const poly *a, const poly *b) {
  poly_Rq_mul(r, a, b);
  poly_mod_q_Phi_n(r);
}

static void poly_S3_mul(poly *r, const poly *a, const poly *b) {
  poly_Rq_mul(r, a, b);
  poly_mod_3_Phi_n(r);
}

static uint8_t tiny_mod3(uint8_t a) {
  int16_t t, c;
  a = (a >> 2) + (a & 3);
  t = a - 3;
  c = t >> 5;
  return (uint8_t)(t ^ (c & (a ^ t)));
}

static void poly_S3_inv(poly *r, const poly *a) {
  poly f, g, v, w;
  size_t i, loop;
  int16_t delta, sign, swap, t;
  for (i = 0; i < NTRU_N; ++i) v.coeffs[i] = 0;
  for (i = 0; i < NTRU_N; ++i) w.coeffs[i] = 0;
  w.coeffs[0] = 1;
  for (i = 0; i < NTRU_N; ++i) f.coeffs[i] = 1;
  for (i = 0; i < NTRU_N - 1; ++i) g.coeffs[NTRU_N - 2 - i] = tiny_mod3((a->coeffs[i] & 3) + 2 * (a->coeffs[NTRU_N - 1] & 3));
  g.coeffs[NTRU_N - 1] = 0;
  delta = 1;
  for (loop = 0; loop < 2 * (NTRU_N - 1) - 1; ++loop) {
    for (i = NTRU_N - 1; i > 0; --i) v.coeffs[i] = v.coeffs[i - 1];
    v.coeffs[0] = 0;
    sign = tiny_mod3((uint8_t)(2 * g.coeffs[0] * f.coeffs[0]));
    swap = both_negative_mask(-delta, -(int16_t)g.coeffs[0]);
    delta ^= swap & (delta ^ -delta);
    delta += 1;
    for (i = 0; i < NTRU_N; ++i) {
      t = swap & (f.coeffs[i] ^ g.coeffs[i]);
      f.coeffs[i] ^= t;
      g.coeffs[i] ^= t;
      t = swap & (v.coeffs[i] ^ w.coeffs[i]);
      v.coeffs[i] ^= t;
      w.coeffs[i] ^= t;
    }
    for (i = 0; i < NTRU_N; ++i) g.coeffs[i] = tiny_mod3((uint8_t)(g.coeffs[i] + sign * f.coeffs[i]));
    for (i = 0; i < NTRU_N; ++i) w.coeffs[i] = tiny_mod3((uint8_t)(w.coeffs[i] + sign * v.coeffs[i]));
    for (i = 0; i < NTRU_N - 1; ++i) g.coeffs[i] = g.coeffs[i + 1];
    g.coeffs[NTRU_N - 1] = 0;
  }
  sign = f.coeffs[0];
  for (i = 0; i < NTRU_N - 1; ++i) r->coeffs[i] = tiny_mod3((uint8_t)(sign * v.coeffs[NTRU_N - 2 - i]));
  r->coeffs[NTRU_N - 1] = 0;
}

static void poly_R2_inv(poly *r, const poly *a) {
  poly f, g, v, w;
  size_t i, loop;
  int16_t delta, sign, swap, t;
  for (i = 0; i < NTRU_N; ++i) v.coeffs[i] = 0;
  for (i = 0; i < NTRU_N; ++i) w.coeffs[i] = 0;
  w.coeffs[0] = 1;
  for (i = 0; i < NTRU_N; ++i) f.coeffs[i] = 1;
  for (i = 0; i < NTRU_N - 1; ++i) g.coeffs[NTRU_N - 2 - i] = (a->coeffs[i] ^ a->coeffs[NTRU_N - 1]) & 1;
  g.coeffs[NTRU_N - 1] = 0;
  delta = 1;
  for (loop = 0; loop < 2 * (NTRU_N - 1) - 1; ++loop) {
    for (i = NTRU_N - 1; i > 0; --i) v.coeffs[i] = v.coeffs[i - 1];
    v.coeffs[0] = 0;
    sign = g.coeffs[0] & f.coeffs[0];
    swap = both_negative_mask(-delta, -(int16_t)g.coeffs[0]);
    delta ^= swap & (delta ^ -delta);
    delta += 1;
    for (i = 0; i < NTRU_N; ++i) {
      t = swap & (f.coeffs[i] ^ g.coeffs[i]);
      f.coeffs[i] ^= t;
      g.coeffs[i] ^= t;
      t = swap & (v.coeffs[i] ^ w.coeffs[i]);
      v.coeffs[i] ^= t;
      w.coeffs[i] ^= t;
    }
    for (i = 0; i < NTRU_N; ++i) g.coeffs[i] = g.coeffs[i] ^ (sign & f.coeffs[i]);
    for (i = 0; i < NTRU_N; ++i) w.coeffs[i] = w.coeffs[i] ^ (sign & v.coeffs[i]);
    for (i = 0; i < NTRU_N - 1; ++i) g.coeffs[i] = g.coeffs[i + 1];
    g.coeffs[NTRU_N - 1] = 0;
  }
  for (i = 0; i < NTRU_N - 1; ++i) r->coeffs[i] = v.coeffs[NTRU_N - 2 - i];
  r->coeffs[NTRU_N - 1] = 0;
}

static void poly_R2_inv_to_Rq_inv(poly *r, const poly *ai, const poly *a) {
  int i;
  poly b, c, s;
  for (i = 0; i < NTRU_N; i++) b.coeffs[i] = -(a->coeffs[i]);
  for (i = 0; i < NTRU_N; i++) r->coeffs[i] = ai->coeffs[i];
  poly_Rq_mul(&c, r, &b);
  c.coeffs[0] += 2;
  poly_Rq_mul(&s, &c, r);
  poly_Rq_mul(&c, &s, &b);
  c.coeffs[0] += 2;
  poly_Rq_mul(r, &c, &s);
  poly_Rq_mul(&c, r, &b);
  c.coeffs[0] += 2;
  poly_Rq_mul(&s, &c, r);
  poly_Rq_mul(&c, &s, &b);
  c.coeffs[0] += 2;
  poly_Rq_mul(r, &c, &s);
}

static void poly_Rq_inv(poly *r, const poly *a) {
  poly ai2;
  poly_R2_inv(&ai2, a);
  poly_R2_inv_to_Rq_inv(r, &ai2, a);
}

static void poly_lift(poly *r, const poly *a) {
  int i;
  poly b;
  uint16_t t, zj;
  t = 3 - (NTRU_N % 3);
  b.coeffs[0] = a->coeffs[0] * (2 - t) + a->coeffs[1] * 0 + a->coeffs[2] * t;
  b.coeffs[1] = a->coeffs[1] * (2 - t) + a->coeffs[2] * 0;
  b.coeffs[2] = a->coeffs[2] * (2 - t);
  zj = 0;
  for (i = 3; i < NTRU_N; i++) {
    b.coeffs[0] += a->coeffs[i] * (zj + 2 * t);
    b.coeffs[1] += a->coeffs[i] * (zj + t);
    b.coeffs[2] += a->coeffs[i] * zj;
    zj = (zj + t) % 3;
  }
  b.coeffs[1] += a->coeffs[0] * (zj + t);
  b.coeffs[2] += a->coeffs[0] * zj;
  b.coeffs[2] += a->coeffs[1] * (zj + t);
  b.coeffs[0] = b.coeffs[0];
  b.coeffs[1] = b.coeffs[1];
  b.coeffs[2] = b.coeffs[2];
  for (i = 3; i < NTRU_N; i++) b.coeffs[i] = b.coeffs[i - 3] + 2 * (a->coeffs[i] + a->coeffs[i - 1] + a->coeffs[i - 2]);
  poly_mod_3_Phi_n(&b);
  poly_Z3_to_Zq(&b);
  r->coeffs[0] = -(b.coeffs[0]);
  for (i = 0; i < NTRU_N - 1; i++) r->coeffs[i + 1] = b.coeffs[i] - b.coeffs[i + 1];
}

static void poly_S3_tobytes(unsigned char *msg, const poly *a) {
  int i;
  unsigned char c;
  for (i = 0; i < NTRU_PACK_DEG / 5; i++) {
    c = a->coeffs[5 * i + 4] & 255;
    c = (3 * c + a->coeffs[5 * i + 3]) & 255;
    c = (3 * c + a->coeffs[5 * i + 2]) & 255;
    c = (3 * c + a->coeffs[5 * i + 1]) & 255;
    c = (3 * c + a->coeffs[5 * i + 0]) & 255;
    msg[i] = c;
  }
}

static void poly_S3_frombytes(poly *r, const unsigned char msg[NTRU_OWCPA_MSGBYTES]) {
  int i;
  unsigned char c;
  for (i = 0; i < NTRU_PACK_DEG / 5; i++) {
    c = msg[i];
    r->coeffs[5 * i + 0] = c;
    r->coeffs[5 * i + 1] = c * 171 >> 9;
    r->coeffs[5 * i + 2] = c * 57 >> 9;
    r->coeffs[5 * i + 3] = c * 19 >> 9;
    r->coeffs[5 * i + 4] = c * 203 >> 14;
  }
  r->coeffs[NTRU_N - 1] = 0;
  poly_mod_3_Phi_n(r);
}

static void poly_Sq_tobytes(unsigned char *r, const poly *a) {
  int i, j;
  uint16_t t[8];
  for (i = 0; i < NTRU_PACK_DEG / 8; i++) {
    for (j = 0; j < 8; j++) t[j] = MODQ(a->coeffs[8 * i + j]);
    r[13 * i + 0] = (unsigned char)(t[0] & 0xff);
    r[13 * i + 1] = (unsigned char)((t[0] >> 8) | ((t[1] & 0x07) << 5));
    r[13 * i + 2] = (unsigned char)((t[1] >> 3) & 0xff);
    r[13 * i + 3] = (unsigned char)((t[1] >> 11) | ((t[2] & 0x3f) << 2));
    r[13 * i + 4] = (unsigned char)((t[2] >> 6) | ((t[3] & 0x01) << 7));
    r[13 * i + 5] = (unsigned char)((t[3] >> 1) & 0xff);
    r[13 * i + 6] = (unsigned char)((t[3] >> 9) | ((t[4] & 0x0f) << 4));
    r[13 * i + 7] = (unsigned char)((t[4] >> 4) & 0xff);
    r[13 * i + 8] = (unsigned char)((t[4] >> 12) | ((t[5] & 0x7f) << 1));
    r[13 * i + 9] = (unsigned char)((t[5] >> 7) | ((t[6] & 0x03) << 6));
    r[13 * i + 10] = (unsigned char)((t[6] >> 2) & 0xff);
    r[13 * i + 11] = (unsigned char)((t[6] >> 10) | ((t[7] & 0x1f) << 3));
    r[13 * i + 12] = (unsigned char)((t[7] >> 5));
  }
  for (j = 0; j < NTRU_PACK_DEG - 8 * i; j++) t[j] = MODQ(a->coeffs[8 * i + j]);
  for (; j < 8; j++) t[j] = 0;
  switch (NTRU_PACK_DEG - 8 * (NTRU_PACK_DEG / 8)) {
    case 4:
      r[13 * i + 0] = (unsigned char)(t[0] & 0xff);
      r[13 * i + 1] = (unsigned char)(t[0] >> 8) | ((t[1] & 0x07) << 5);
      r[13 * i + 2] = (unsigned char)(t[1] >> 3) & 0xff;
      r[13 * i + 3] = (unsigned char)(t[1] >> 11) | ((t[2] & 0x3f) << 2);
      r[13 * i + 4] = (unsigned char)(t[2] >> 6) | ((t[3] & 0x01) << 7);
      r[13 * i + 5] = (unsigned char)(t[3] >> 1) & 0xff;
      r[13 * i + 6] = (unsigned char)(t[3] >> 9) | ((t[4] & 0x0f) << 4);
      break;
    case 2:
      r[13 * i + 0] = (unsigned char)(t[0] & 0xff);
      r[13 * i + 1] = (unsigned char)(t[0] >> 8) | ((t[1] & 0x07) << 5);
      r[13 * i + 2] = (unsigned char)(t[1] >> 3) & 0xff;
      r[13 * i + 3] = (unsigned char)(t[1] >> 11) | ((t[2] & 0x3f) << 2);
  }
}

static void poly_Sq_frombytes(poly *r, const unsigned char *a) {
  int i;
  for (i = 0; i < NTRU_PACK_DEG / 8; i++) {
    r->coeffs[8 * i + 0] = a[13 * i + 0] | (((uint16_t)a[13 * i + 1] & 0x1f) << 8);
    r->coeffs[8 * i + 1] = (a[13 * i + 1] >> 5) | (((uint16_t)a[13 * i + 2]) << 3) | (((uint16_t)a[13 * i + 3] & 0x03) << 11);
    r->coeffs[8 * i + 2] = (a[13 * i + 3] >> 2) | (((uint16_t)a[13 * i + 4] & 0x7f) << 6);
    r->coeffs[8 * i + 3] = (a[13 * i + 4] >> 7) | (((uint16_t)a[13 * i + 5]) << 1) | (((uint16_t)a[13 * i + 6] & 0x0f) << 9);
    r->coeffs[8 * i + 4] = (a[13 * i + 6] >> 4) | (((uint16_t)a[13 * i + 7]) << 4) | (((uint16_t)a[13 * i + 8] & 0x01) << 12);
    r->coeffs[8 * i + 5] = (a[13 * i + 8] >> 1) | (((uint16_t)a[13 * i + 9] & 0x3f) << 7);
    r->coeffs[8 * i + 6] = (a[13 * i + 9] >> 6) | (((uint16_t)a[13 * i + 10]) << 2) | (((uint16_t)a[13 * i + 11] & 0x07) << 10);
    r->coeffs[8 * i + 7] = (a[13 * i + 11] >> 3) | (((uint16_t)a[13 * i + 12]) << 5);
  }
  switch (NTRU_PACK_DEG & 0x07) {
    case 4:
      r->coeffs[8 * i + 0] = a[13 * i + 0] | (((uint16_t)a[13 * i + 1] & 0x1f) << 8);
      r->coeffs[8 * i + 1] = (a[13 * i + 1] >> 5) | (((uint16_t)a[13 * i + 2]) << 3) | (((uint16_t)a[13 * i + 3] & 0x03) << 11);
      r->coeffs[8 * i + 2] = (a[13 * i + 3] >> 2) | (((uint16_t)a[13 * i + 4] & 0x7f) << 6);
      r->coeffs[8 * i + 3] = (a[13 * i + 4] >> 7) | (((uint16_t)a[13 * i + 5]) << 1) | (((uint16_t)a[13 * i + 6] & 0x0f) << 9);
      break;
    case 2:
      r->coeffs[8 * i + 0] = a[13 * i + 0] | (((uint16_t)a[13 * i + 1] & 0x1f) << 8);
      r->coeffs[8 * i + 1] = (a[13 * i + 1] >> 5) | (((uint16_t)a[13 * i + 2]) << 3) | (((uint16_t)a[13 * i + 3] & 0x03) << 11);
  }
  r->coeffs[NTRU_N - 1] = 0;
}

static void poly_Rq_sum_zero_frombytes(poly *r, const unsigned char *a) {
  int i;
  poly_Sq_frombytes(r, a);
  r->coeffs[NTRU_N - 1] = 0;
  for (i = 0; i < NTRU_PACK_DEG; i++) r->coeffs[NTRU_N - 1] -= r->coeffs[i];
}

static void sample_iid(poly *r, const unsigned char uniformbytes[NTRU_SAMPLE_IID_BYTES]) {
  int i;
  for (i = 0; i < NTRU_N - 1; i++) r->coeffs[i] = mod3(uniformbytes[i]);
  r->coeffs[NTRU_N - 1] = 0;
}

static void sample_iid_plus(poly *r, const unsigned char uniformbytes[NTRU_SAMPLE_IID_BYTES]) {
  int i;
  uint16_t s = 0;
  sample_iid(r, uniformbytes);
  for (i = 0; i < NTRU_N - 1; i++) r->coeffs[i] = r->coeffs[i] | (-(r->coeffs[i] >> 1));
  for (i = 0; i < NTRU_N - 1; i++) s += (uint16_t)((uint32_t)r->coeffs[i + 1] * (uint32_t)r->coeffs[i]);
  s = 1 | (-(s >> 15));
  for (i = 0; i < NTRU_N; i += 2) r->coeffs[i] = (uint16_t)((uint32_t)s * (uint32_t)r->coeffs[i]);
  for (i = 0; i < NTRU_N; i++) r->coeffs[i] = 3 & (r->coeffs[i] ^ (r->coeffs[i] >> 15));
}

static void sample_fg(poly *f, poly *g, const unsigned char uniformbytes[NTRU_SAMPLE_FG_BYTES]) {
  sample_iid_plus(f, uniformbytes);
  sample_iid_plus(g, uniformbytes + NTRU_SAMPLE_IID_BYTES);
}

static void sample_rm(poly *r, poly *m, const unsigned char uniformbytes[NTRU_SAMPLE_RM_BYTES]) {
  sample_iid(r, uniformbytes);
  sample_iid(m, uniformbytes + NTRU_SAMPLE_IID_BYTES);
}

static void cmov(unsigned char *r, const unsigned char *x, size_t len, unsigned char b) {
  size_t i;
  b = (~b + 1);
  for (i = 0; i < len; i++) r[i] ^= b & (x[i] ^ r[i]);
}

static int owcpa_check_ciphertext(const unsigned char *ciphertext) {
  uint16_t t = ciphertext[crypto_kem_CIPHERTEXTBYTES - 1];
  t &= 0xff << (8 - (7 & (NTRU_LOGQ * NTRU_PACK_DEG)));
  return (int)(1 & ((~t + 1) >> 15));
}

static int owcpa_check_r(const poly *r) {
  int i;
  uint32_t t = 0;
  uint16_t c;
  for (i = 0; i < NTRU_N - 1; i++) {
    c = r->coeffs[i];
    t |= (c + 1) & (NTRU_Q - 4);
    t |= (c + 2) & 4;
  }
  t |= r->coeffs[NTRU_N - 1];
  return (int)(1 & ((~t + 1) >> 31));
}

static void owcpa_keypair(unsigned char *pk, unsigned char *sk, const unsigned char seed[NTRU_SAMPLE_FG_BYTES]) {
  int i;
  poly x1, x2, x3, x4, x5;
  poly *f = &x1, *g = &x2, *invf_mod3 = &x3, *gf = &x3, *invgf = &x4, *tmp = &x5, *invh = &x3, *h = &x3;
  sample_fg(f, g, seed);
  poly_S3_inv(invf_mod3, f);
  poly_S3_tobytes(sk, f);
  poly_S3_tobytes(sk + NTRU_PACK_TRINARY_BYTES, invf_mod3);
  poly_Z3_to_Zq(f);
  poly_Z3_to_Zq(g);
  for (i = NTRU_N - 1; i > 0; i--) g->coeffs[i] = 3 * (g->coeffs[i - 1] - g->coeffs[i]);
  g->coeffs[0] = -(3 * g->coeffs[0]);
  poly_Rq_mul(gf, g, f);
  poly_Rq_inv(invgf, gf);
  poly_Rq_mul(tmp, invgf, f);
  poly_Sq_mul(invh, tmp, f);
  poly_Sq_tobytes(sk + 2 * NTRU_PACK_TRINARY_BYTES, invh);
  poly_Rq_mul(tmp, invgf, g);
  poly_Rq_mul(h, tmp, g);
  poly_Sq_tobytes(pk, h);
}

static void owcpa_enc(unsigned char *c, const poly *r, const poly *m, const unsigned char *pk) {
  int i;
  poly x1, x2;
  poly *h = &x1, *liftm = &x1, *ct = &x2;
  poly_Rq_sum_zero_frombytes(h, pk);
  poly_Rq_mul(ct, r, h);
  poly_lift(liftm, m);
  for (i = 0; i < NTRU_N; i++) ct->coeffs[i] = ct->coeffs[i] + liftm->coeffs[i];
  poly_Sq_tobytes(c, ct);
}

static int owcpa_dec(unsigned char *rm, const unsigned char *ciphertext, const unsigned char *secretkey) {
  int i, fail = 0;
  poly x1, x2, x3, x4;
  poly *c = &x1, *f = &x2, *cf = &x3, *mf = &x2, *finv3 = &x3, *m = &x4, *liftm = &x2, *invh = &x3, *r = &x4, *b = &x1;
  poly_Rq_sum_zero_frombytes(c, ciphertext);
  poly_S3_frombytes(f, secretkey);
  poly_Z3_to_Zq(f);
  poly_Rq_mul(cf, c, f);
  poly_Rq_to_S3(mf, cf);
  poly_S3_frombytes(finv3, secretkey + NTRU_PACK_TRINARY_BYTES);
  poly_S3_mul(m, mf, finv3);
  poly_S3_tobytes(rm + NTRU_PACK_TRINARY_BYTES, m);
  fail |= owcpa_check_ciphertext(ciphertext);
  poly_lift(liftm, m);
  for (i = 0; i < NTRU_N; i++) b->coeffs[i] = c->coeffs[i] - liftm->coeffs[i];
  poly_Sq_frombytes(invh, secretkey + 2 * NTRU_PACK_TRINARY_BYTES);
  poly_Sq_mul(r, b, invh);
  fail |= owcpa_check_r(r);
  poly_trinary_Zq_to_Z3(r);
  poly_S3_tobytes(rm, r);
  return fail;
}

int crypto_kem_keypair(unsigned char *pk, unsigned char *sk) {
  unsigned char seed[NTRU_SAMPLE_FG_BYTES];
  randombytes(seed, NTRU_SAMPLE_FG_BYTES);
  owcpa_keypair(pk, sk, seed);
  randombytes(sk + NTRU_OWCPA_SECRETKEYBYTES, NTRU_PRFKEYBYTES);
  return 0;
}

int crypto_kem_enc(unsigned char *c, unsigned char *k, const unsigned char *pk) {
  poly r, m;
  unsigned char rm[NTRU_OWCPA_MSGBYTES], rm_seed[NTRU_SAMPLE_RM_BYTES];
  randombytes(rm_seed, NTRU_SAMPLE_RM_BYTES);
  sample_rm(&r, &m, rm_seed);
  poly_S3_tobytes(rm, &r);
  poly_S3_tobytes(rm + NTRU_PACK_TRINARY_BYTES, &m);
  crypto_hash_sha3256(k, rm, NTRU_OWCPA_MSGBYTES);
  poly_Z3_to_Zq(&r);
  owcpa_enc(c, &r, &m, pk);
  return 0;
}

int crypto_kem_dec(unsigned char *k, const unsigned char *c, const unsigned char *sk) {
  int i, fail;
  unsigned char rm[NTRU_OWCPA_MSGBYTES + NTRU_PACK_TRINARY_BYTES], buf[NTRU_PRFKEYBYTES + crypto_kem_CIPHERTEXTBYTES];
  fail = owcpa_dec(rm, c, sk);
  crypto_hash_sha3256(k, rm, NTRU_OWCPA_MSGBYTES);
  for (i = 0; i < NTRU_PRFKEYBYTES; i++) buf[i] = sk[i + NTRU_OWCPA_SECRETKEYBYTES];
  for (i = 0; i < crypto_kem_CIPHERTEXTBYTES; i++) buf[NTRU_PRFKEYBYTES + i] = c[i];
  crypto_hash_sha3256(rm, buf, NTRU_PRFKEYBYTES + crypto_kem_CIPHERTEXTBYTES);
  cmov(k, rm, NTRU_SHAREDKEYBYTES, (unsigned char)fail);
  return 0;
}