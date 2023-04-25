#ifndef LLAMX_H
#define LLAMX_H

enum AMXOpcode {
  LDX = 0,
  LDY = 1,
  STX = 2,
  STY = 3,
  LDZ = 4,
  STZ = 5,
  LDZI = 6,
  STZI = 7,
  EXTRX = 8,
  EXTRY = 9,
  FMA64 = 10,
  FMS64 = 11,
  FMA32 = 12,
  FMS32 = 13,
  MAC16 = 14,
  FMA16 = 15,
  FMS16 = 16,
  SETCLR = 17,
  VECINT = 18,
  VECFP = 19,
  MATINT = 20,
  MATFP = 21,
  GENLUT = 22
};

#endif // LLAMX_H