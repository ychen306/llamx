// RUN: source path.sh
// RUN: llamx-clang %s -c -O1 -I %S/../.. -o %t
// RUN: cc %t -o %t
// RUN: %t | FileCheck %s

// CHECK: matmul32: PASS

#include "aarch64.h"
#include "amx.h"

#include <stdio.h>
#include <string.h>

// Computes Z = Y^T@X
void matmul32(float Z[16][16], float X[16][16], float Y[16][16]) {
  unsigned reg = 3;

  AMX_SET();
  for (int i = 0; i < 16; i++) {
    amx_ldx(reg, X[i]);
    amx_ldy(reg, Y[i]);
    amx_fma32(reg, reg, reg);
  }
  for (int j = 0; j < 16; j++) {
    amx_stz(Z[j], 4 * j + reg);
  }
  AMX_CLR();
}

int main() {
  float X[16][16];
  float Y[16][16];
  float Z[16][16];
  float ref[16][16];

  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 16; j++) {
      X[i][j] = i + j;
      Y[i][j] = i - j;
      ref[i][j] = 1240 + 120 * (j - i) - 16 * i * j;
    }
  }
  
  matmul32(Z, X, Y);

  printf("%s: %s\n", "matmul32", memcmp(Z, ref, 1024) ? "FAIL" : "PASS");
}
