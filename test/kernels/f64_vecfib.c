// RUN: source %S/path.sh
// RUN: %llamx-clang %s -o %t
// RUN: cc %t -o %t
// RUN: %t | FileCheck %s

// CHECK: f64_vecfib: PASS

#include "aarch64.h"
#include "amx.h"

#include <stdio.h>
#include <string.h>

// f(0) = X
// f(1) = Y
// f(n) = f(n-1) + f(n-2)
// return fn in Z
void vecfib(int n, double Z[8], double X[8], double Y[8]) {
  if (n == 0) {
    amx_ldx(65, X);
    amx_mvxz(66, 65);
    amx_stz(Z, 66);
  } else if (n == 1) {
    amx_ldx(65, Y);
    amx_mvxz(66, 65);
    amx_stz(Z, 66);
  } else {
    double ones[8] = {1, 1, 1, 1, 1, 1, 1, 1};
    double Z1[8];
    double Z2[8];
    vecfib(n-1, Z1, X, Y);
    vecfib(n-2, Z2, X, Y);
    amx_ldz(65, Z1);
    amx_ldx(66, Z2);
    amx_ldy(67, ones);
    amx_fma64_vec(65, 66, 67);
    amx_stz(Z, 65);
  }
}

int main() {
  double X[8];
  double Y[8];
  double Z[8];
  double ref[8];

  for (int i = 0; i < 8; i++) {
    X[i] = i;
    Y[i] = 1;
    ref[i] = 3 * i + 5;
  }

  // i 1 i+1 i+2 2i+3 3i+5

  AMX_SET();
  vecfib(5, Z, X, Y);
  AMX_CLR();

  for (int i = 0; i < 8; i++) {
    printf("%lf %lf\n", Z[i], ref[i]);
  }
  printf("%s: %s\n", "f64_vecfib", memcmp(Z, ref, 64) ? "FAIL" : "PASS");
}
