// RUN: source path.sh
// RUN: llamx-clang %s -c -O1 -I %S/../.. -o %t
// RUN: cc %t -o %t
// RUN: %t | FileCheck %s

// CHECK: fma32_vec: PASS
// CHECK: fms32_vec: PASS

#include "aarch64.h"
#include "amx.h"

#include <stdio.h>
#include <string.h>

// Computes Z += X * Y
void fma32_vec(float Z[16], float X[16], float Y[16]) {
  AMX_SET();
  amx_ldx(65, X);
  amx_ldy(66, Y);
  amx_ldz(67, Z);
  amx_fma32_vec(67, 65, 66);
  amx_stz(Z, 67);
  AMX_CLR();
}

// Computes Z -= X * Y
void fms32_vec(float Z[16], float X[16], float Y[16]) {
  AMX_SET();
  amx_ldx(65, X);
  amx_ldy(66, Y);
  amx_ldz(67, Z);
  amx_fms32_vec(67, 65, 66);
  amx_stz(Z, 67);
  AMX_CLR();
}

int main() {
  float X[16];
  float Y[16];
  float Z[16];
  float ref[16];

  for (int i = 0; i < 16; i++) {
    X[i] = i;
    Y[i] = -i;
    Z[i] = 2 * i;
    ref[i] = Z[i] + X[i] * Y[i];
  }

  fma32_vec(Z, X, Y);
  printf("%s: %s\n", "fma32_vec", memcmp(Z, ref, 64) ? "FAIL" : "PASS");

  for (int i = 0; i < 16; i++) {
    X[i] = i;
    Y[i] = -i;
    Z[i] = 2 * i;
    ref[i] = Z[i] - X[i] * Y[i];
  }

  fms32_vec(Z, X, Y);
  printf("%s: %s\n", "fms32_vec", memcmp(Z, ref, 64) ? "FAIL" : "PASS");
}
