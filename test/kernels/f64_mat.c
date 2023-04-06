// RUN: source path.sh
// RUN: llamx-clang %s -c -O1 -I %S/../.. -o %t
// RUN: cc %t -o %t
// RUN: %t | FileCheck %s

// CHECK: fma64_mat: PASS

#include "aarch64.h"
#include "amx.h"

#include <stdio.h>
#include <string.h>

// Computes Z += Y^T@X
void matmul64(double Z[64], double X[8][8], double Y[8][8]) {

  AMX_SET();
  amx_ldz_col(65, Z);
  for (int i = 0; i < 8; i++) {
    amx_ldx(66, X[i]);
    amx_ldy(67, Y[i]);
    amx_fma64_mat(65, 66, 67);
  }
  amx_stz_col(Z, 65);
  AMX_CLR();
}

int main() {
  double X[8][8];
  double Y[8][8];
  double Z[64];
  double ref[64];

  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 8; j++) {
      X[i][j] = i + j;
      Y[i][j] = i - j;
      Z[8 * i + j] = i;
      ref[8 * i + j] = 140 + 28 * (j - i) - 8 * i * j + i;
    }
  }

  matmul64(Z, X, Y);

  printf("%s: %s\n", "fma64_mat", memcmp(Z, ref, 512) ? "FAIL" : "PASS");
}
