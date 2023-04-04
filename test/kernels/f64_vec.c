// RUN: source path.sh
// RUN: llamx-clang %s -c -O1 -I %S/../.. -o %t
// RUN: cc %t -o %t
// RUN: %t | FileCheck %s

// CHECK: fma64_vec: PASS
// CHECK: fms64_vec: PASS

#include "aarch64.h"
#include "amx.h"

#include <stdio.h>
#include <string.h>

// Computes Z += X * Y
void fma64_vec(double Z[8], double X[8], double Y[8]) {
  AMX_SET();
  amx_ldx(65, X);
  amx_ldy(66, Y);
  amx_ldz(67, Z);
  amx_fma64_vec(67, 65, 66);
  amx_stz(Z, 67);
  AMX_CLR();
}

// Computes Z -= X * Y
void fms64_vec(double Z[8], double X[8], double Y[8]) {
  AMX_SET();
  amx_ldx(65, X);
  amx_ldy(66, Y);
  amx_ldz(67, Z);
  amx_fms64_vec(67, 65, 66);
  amx_stz(Z, 67);
  AMX_CLR();
}

int main() {
  double X[8];
  double Y[8];
  double Z[8];
  double ref[8];

  for (int i = 0; i < 8; i++) {
    X[i] = i;
    Y[i] = -i;
    Z[i] = 2 * i;
    ref[i] = Z[i] + X[i] * Y[i];
  }

  fma64_vec(Z, X, Y);
  printf("%s: %s\n", "fma64_vec", memcmp(Z, ref, 64) ? "FAIL" : "PASS");

  for (int i = 0; i < 8; i++) {
    X[i] = i;
    Y[i] = -i;
    Z[i] = 2 * i;
    ref[i] = Z[i] - X[i] * Y[i];
  }

  fms64_vec(Z, X, Y);
  printf("%s: %s\n", "fms64_vec", memcmp(Z, ref, 64) ? "FAIL" : "PASS");
}
