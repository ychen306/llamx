// RUN: source %S/path.sh
// RUN: %llamx-clang %s -o %t
// RUN: cc %t -o %t
// RUN: %t | FileCheck %s

// CHECK: Z: PASS
// CHECK: Zcol: PASS

#include "aarch64.h"
#include "amx.h"

#include <stdio.h>
#include <string.h>

// Computes Zcol += Y^T@X and Z += Y*X at the same time
void kernel(double Zcol[64], double Z[8][8], double X[8][8], double Y[8][8]) {
  AMX_SET();
  amx_ldz_col(65, Zcol);
  #pragma clang loop unroll(full)
  for (int i = 0; i < 8; i++) {
    int virtreg = 65 + i;
    amx_ldx(virtreg, X[i]);
    amx_ldy(virtreg, Y[i]);
    amx_ldz(virtreg, Z[i]);
    amx_fma64_mat(65, virtreg, virtreg);
    amx_fma64_vec(virtreg, virtreg, virtreg);
  }
  amx_stz_col(Zcol, 65);
  #pragma clang loop unroll(full)
  for (int i = 0; i < 8; i++) {
    int virtreg = 65 + i;
    amx_stz(Z[i], virtreg);
  }
  AMX_CLR();
}

int main() {
  double X[8][8];
  double Y[8][8];
  double Z[8][8];
  double Z_ref[8][8];
  double Zcol[64];
  double Zcol_ref[64];

  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 8; j++) {
      X[i][j] = i + j;
      Y[i][j] = i - j;
      Z[i][j] = i;
      Z_ref[i][j] = Z[i][j] + X[i][j] * Y[i][j];
      Zcol[8 * i + j] = i;
      Zcol_ref[8 * i + j] = 140 + 28 * (j - i) - 8 * i * j + i;
    }
  }

  kernel(Zcol, Z, X, Y);

  printf("%s: %s\n", "Z", memcmp(Z, Z_ref, 512) ? "FAIL" : "PASS");
  printf("%s: %s\n", "Zcol", memcmp(Zcol, Zcol_ref, 512) ? "FAIL" : "PASS");
}
