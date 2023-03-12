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
#endif // AMX_H
