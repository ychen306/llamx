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

// Computes Zcol2 += y^T@x and z += y*x at the same time
void kernel(float Zcol2[256], float Z[16], float X[16], float Y[16]) {
  AMX_SET();
  amx_ldz_col2(65, Zcol2);
  amx_ldx(66, X);
  amx_ldy(67, Y);
  amx_ldz(68, Z);
  amx_fma32_mat(65, 66, 67);
  amx_fma32_vec(68, 66, 67);
  amx_stz_col2(Zcol2, 65);
  amx_stz(Z, 68);
  AMX_CLR();
}

int main() {
  float X[16];
  float Y[16];
  float Z[16];
  float Z_ref[16];
  float Zcol2[256];
  float Zcol2_ref[256];

  for (int i = 0; i < 16; i++) {
    X[i] = i;
    Y[i] = -i;
    Z[i] = 2 * i;
    Z_ref[i] = Z[i] + X[i] * Y[i];
    for (int j = 0; j < 16; j++) {
      Zcol2[16 * i + j] = i;
      Zcol2_ref[16 * i + j] = i - i * j;
    }
  }

  kernel(Zcol2, Z, X, Y);

  printf("%s: %s\n", "Z", memcmp(Z, Z_ref, 64) ? "FAIL" : "PASS");
  printf("%s: %s\n", "Zcol", memcmp(Zcol2, Zcol2_ref, 1024) ? "FAIL" : "PASS");
}
