// RUN: source path.sh
// RUN: llamx-clang %s -c -O1 -I %S/../.. -o %t
// RUN: cc %t -o %t
// RUN: %t | FileCheck %s

// CHECK: mvxy: PASS
// CHECK: mvyx: PASS

#include "aarch64.h"
#include "amx.h"

#include <stdio.h>
#include <string.h>

typedef void ldfn_t(unsigned, void*);
typedef void stfn_t(void*, unsigned);
typedef void mvfn_t(unsigned, unsigned);

void mv_test(ldfn_t ldfn, stfn_t stfn, mvfn_t mvfn, char* test_name) {
  char a[128];
  char b[128];
  char ref[128];
  memset(a, 'A', 128);
  memset(b, 'B', 128);
  memset(ref, 'A', 64); // only 64 bytes will get copied
  memset(ref+64, 'B', 64);

  AMX_SET();
  ldfn(1, a);
  mvfn(2, 1);
  stfn(b, 2);
  AMX_CLR();

  printf("%s: %s\n", test_name, memcmp(b, ref, 128) ? "FAIL" : "PASS");
}

int main() {
  mv_test(amx_ldx, amx_sty, amx_mvxy, "mvxy");
  // mv_test(amx_ldx, amx_stz, amx_mvxz, "mvxz");
  mv_test(amx_ldy, amx_stx, amx_mvyx, "mvyx");
  // mv_test(amx_ldy, amx_stz, amx_mvyz, "mvyz");
  // mv_test(amx_ldz, amx_stx, amx_mvzx, "mvzx");
  // mv_test(amx_ldz, amx_sty, amx_mvzy, "mvzy");
}
