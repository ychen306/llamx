// RUN: source path.sh
// RUN: llamx-clang %s -c -O1 -I %S/../.. -o %t
// RUN: cc %t -o %t
// RUN: %t | FileCheck %s

// CHECK: ldst_z_col: PASS
// CHECK: ldst_z_col2: PASS
// CHECK: ldst_z_col4: PASS
// CHECK: ldst_z_col8: PASS

#include "aarch64.h"
#include "amx.h"

#include <stdio.h>
#include <string.h>

typedef void ldfn_t(unsigned, void*);
typedef void stfn_t(void*, unsigned);

void ldst_test(ldfn_t ldfn, stfn_t stfn, int col, int ldst_size, char* test_name) {
  char a[4160];
  char b[4160];
  char ref[4160];
  memset(a, 'A', 4160);
  memset(b, 'B', 4160);
  memset(ref, 'A', ldst_size);
  memset(ref+ldst_size, 'B', 4160 - ldst_size);

  AMX_SET();
  ldfn(col, a);
  stfn(b, col);
  AMX_CLR();

  printf("%s: %s\n", test_name, memcmp(b, ref, 4160) ? "FAIL" : "PASS");
}

int main() {
  ldst_test(amx_ldz_col, amx_stz_col, 65, 512, "ldst_z_col");
  ldst_test(amx_ldz_col2, amx_stz_col2, 66, 1024, "ldst_z_col2");
  ldst_test(amx_ldz_col4, amx_stz_col4, 67, 2048, "ldst_z_col4");
  ldst_test(amx_ldz_col8, amx_stz_col8, 68, 4096, "ldst_z_col8");
}
