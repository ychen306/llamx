// RUN: source %S/path.sh
// RUN: %llamx-clang %s -o %t
// RUN: cc %t -o %t
// RUN: %t | FileCheck %s

// CHECK: Z: PASS
// CHECK: Zcol2: PASS

#include "aarch64.h"
#include "amx.h"

#include <stdio.h>
#include <string.h>

// Computes Zcol2 += Y^T@X and Z += Y*X at the same time
void kernel(float Zcol2[256], float Z[16][16], float X[16][16], float Y[16][16]) {
  AMX_SET();
  amx_ldz_col2(65, Zcol2);
  #pragma clang loop unroll(full)
  for (int i = 0; i < 16; i++) {
    int virtreg = 65 + i;
    amx_ldx(virtreg, X[i]);
    amx_ldy(virtreg, Y[i]);
    amx_ldz(virtreg, Z[i]);
  }
  #pragma clang loop unroll(full)
  for (int i = 0; i < 16; i++) {
    int virtreg = 65 + i;
    amx_fma32_mat(65, virtreg, virtreg);
    amx_fma32_vec(virtreg, virtreg, virtreg);
  }
  amx_stz_col2(Zcol2, 65);
  #pragma clang loop unroll(full)
  for (int i = 0; i < 16; i++) {
    int virtreg = 65 + i;
    amx_stz(Z[i], virtreg);
  }
  AMX_CLR();
}

int main() {
  float X[16][16];
  float Y[16][16];
  float Z[16][16];
  float Z_ref[16][16];
  float Zcol2[256];
  float Zcol2_ref[256];

  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 16; j++) {
      X[i][j] = i + j;
      Y[i][j] = i - j;
      Z[i][j] = i;
      Z_ref[i][j] = Z[i][j] + X[i][j] * Y[i][j];
      Zcol2[16 * i + j] = i;
      Zcol2_ref[16 * i + j] = 1240 + 120 * (j - i) - 16 * i * j + i;
    }
  }

  kernel(Zcol2, Z, X, Y);

  printf("%s: %s\n", "Z", memcmp(Z, Z_ref, 1024) ? "FAIL" : "PASS");
  printf("%s: %s\n", "Zcol2", memcmp(Zcol2, Zcol2_ref, 1024) ? "FAIL" : "PASS");
}
