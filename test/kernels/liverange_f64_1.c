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

// Computes Zcol += y^T@x and z += y*x at the same time
void kernel(double Zcol[64], double Z[8], double X[8], double Y[8]) {
  AMX_SET();
  amx_ldz_col(65, Zcol);
  amx_ldx(66, X);
  amx_ldy(67, Y);
  amx_ldz(68, Z);
  amx_fma64_mat(65, 66, 67);
  amx_fma64_vec(68, 66, 67);
  amx_stz_col(Zcol, 65);
  amx_stz(Z, 68);
  AMX_CLR();
}

int main() {
  double X[8];
  double Y[8];
  double Z[8];
  double Z_ref[8];
  double Zcol[64];
  double Zcol_ref[64];

  for (int i = 0; i < 8; i++) {
    X[i] = i;
    Y[i] = -i;
    Z[i] = 2 * i;
    Z_ref[i] = Z[i] + X[i] * Y[i];
    for (int j = 0; j < 8; j++) {
      Zcol[8 * i + j] = i;
      Zcol_ref[8 * i + j] = i - i * j;
    }
  }

  kernel(Zcol, Z, X, Y);

  printf("%s: %s\n", "Z", memcmp(Z, Z_ref, 64) ? "FAIL" : "PASS");
  printf("%s: %s\n", "Zcol", memcmp(Zcol, Zcol_ref, 512) ? "FAIL" : "PASS");
}
