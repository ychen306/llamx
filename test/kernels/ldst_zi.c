// RUN: source path.sh
// RUN: llamx-clang %s -c -O1 -I %S/../.. -o %t
// RUN: cc %t -o %t
// RUN: %t | FileCheck %s

// CHECK: ldst_zi: PASS

#include "aarch64.h"
#include "amx.h"

#include <stdio.h>
#include <string.h>

typedef void ldfn_t(unsigned, void*);
typedef void stfn_t(void*, unsigned);

void ldst_test(ldfn_t ldfn, stfn_t stfn, char* test_name) {
  char a[128];
  char b[128];
  char ref[128];
  memset(a, 'A', 128);
  memset(b, 'B', 128);
  memset(ref, 'A', 64); // only 64 bytes will get copied
  memset(ref+64, 'B', 64);

  AMX_SET();
  ldfn(67, a);
  stfn(b, 67);
  AMX_CLR();

  printf("%s: %s\n", test_name, memcmp(b, ref, 128) ? "FAIL" : "PASS");
}

int main() {
  ldst_test(amx_ldzi, amx_stzi, "ldst_zi");
}
