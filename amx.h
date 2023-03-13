#ifndef AMX_H
#define AMX_H
void amx_ldx(unsigned dst, void *src_ptr);
void amx_ldy(unsigned dst, void *src_ptr);
void amx_stx(void *dst_ptr, unsigned src);
void amx_sty(void *dst_ptr, unsigned src);
void amx_ldz(unsigned dst, void *src_ptr);
void amx_stz(void *dst_ptr, unsigned src);
void amx_ldzi(unsigned dst, void *src_ptr);
void amx_stzi(void *dst_ptr, unsigned src);
void amx_fma64(unsigned zcol, unsigned xreg, unsigned yreg); // Outer product, full fma only
void amx_fma32(unsigned zcol, unsigned xreg, unsigned yreg); // Outer product, full fma only
void amx_fma16(unsigned zcol, unsigned xreg, unsigned yreg); // Outer product, full fma only
#endif // AMX_H
