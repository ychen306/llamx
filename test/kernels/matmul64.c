// RUN: source path.sh
// RUN: llamx-clang %s -c -O1 -I %S/../.. -o %t
// RUN: cc %t -o %t
// RUN: %t | FileCheck %s

// CHECK: matmul64: PASS

#include "aarch64.h"
#include "amx.h"

#include <stdio.h>
#include <string.h>

// Computes Z = Y^T@X
void matmul64(double Z[8][8], double X[8][8], double Y[8][8]) {
  unsigned zcol = 3;

  AMX_SET();
  for (int i = 0; i < 8; i++) {
    amx_ldx(i, X[i]);
    amx_ldy(i, Y[i]);
    amx_fma64_mat(zcol, i, i);
  }
  for (int j = 0; j < 8; j++) {
    amx_stz(Z[j], 8 * j + zcol);
  }
  AMX_CLR();
}

int main() {
  double X[8][8];
  double Y[8][8];
  double Z[8][8];
  double ref[8][8];

  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 8; j++) {
      X[i][j] = i + j;
      Y[i][j] = i - j;
      ref[i][j] = 140 + 28 * (j - i) - 8 * i * j;
    }
  }

  matmul64(Z, X, Y);

  printf("%s: %s\n", "matmul64", memcmp(Z, ref, 512) ? "FAIL" : "PASS");
}
