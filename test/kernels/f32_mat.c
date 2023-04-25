// RUN: source %S/path.sh
// RUN: %llamx-clang %s -o %t
// RUN: cc %t -o %t
// RUN: %t | FileCheck %s

// CHECK: fma32_mat: PASS

#include "aarch64.h"
#include "amx.h"

#include <stdio.h>
#include <string.h>

// Computes Z += Y^T@X
void matmul32(float Z[256], float X[16][16], float Y[16][16]) {

  AMX_SET();
  amx_ldz_col2(65, Z);
  for (int i = 0; i < 16; i++) {
    amx_ldx(66, X[i]);
    amx_ldy(67, Y[i]);
    amx_fma32_mat(65, 66, 67);
  }
  amx_stz_col2(Z, 65);
  AMX_CLR();
}

int main() {
  float X[16][16];
  float Y[16][16];
  float Z[256];
  float ref[256];

  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 16; j++) {
      X[i][j] = i + j;
      Y[i][j] = i - j;
      Z[16 * i + j] = i;
      ref[16 * i + j] = 1240 + 120 * (j - i) - 16 * i * j + i;
    }
  }

  matmul32(Z, X, Y);

  printf("Z:\n");
  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 16; j++) {
      printf("%f ", Z[16 * i + j]);
    }
    printf("\n");
  }

  printf("ref:\n");
  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 16; j++) {
      printf("%f ", ref[16 * i + j]);
    }
    printf("\n");
  }

  printf("%s: %s\n", "fma32_mat", memcmp(Z, ref, 1024) ? "FAIL" : "PASS");
}
