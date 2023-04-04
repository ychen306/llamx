#ifndef AMX_H
#define AMX_H
// Loads and stores
void amx_ldx(unsigned dst, void *src_ptr);
void amx_ldy(unsigned dst, void *src_ptr);
void amx_stx(void *dst_ptr, unsigned src);
void amx_sty(void *dst_ptr, unsigned src);
void amx_ldz(unsigned dst, void *src_ptr);
void amx_stz(void *dst_ptr, unsigned src);
void amx_ldzi(unsigned dst, void *src_ptr);
void amx_stzi(void *dst_ptr, unsigned src);

// Moves
void amx_mvxy(unsigned dst, unsigned src); // Move x register to y register
void amx_mvyx(unsigned dst, unsigned src); // Move y register to x register
void amx_mvxz(unsigned dst, unsigned src); // Move x register to z register
void amx_mvyz(unsigned dst, unsigned src); // Move y register to z register
void amx_mvzx(unsigned dst, unsigned src); // Move z register to x register
void amx_mvzy(unsigned dst, unsigned src); // Move z register to y register

// Floating-point operations
void amx_fma64_mat(unsigned dst, unsigned x_reg, unsigned y_reg); // Outer product, full fma only
void amx_fma32_mat(unsigned dst, unsigned x_reg, unsigned y_reg); // Outer product, full fma only
void amx_fma16_mat(unsigned dst, unsigned x_reg, unsigned y_reg); // Outer product, full fma only
void amx_fms64_mat(unsigned dst, unsigned x_reg, unsigned y_reg); // Outer product, full fms only
void amx_fms32_mat(unsigned dst, unsigned x_reg, unsigned y_reg); // Outer product, full fms only
void amx_fms16_mat(unsigned dst, unsigned x_reg, unsigned y_reg); // Outer product, full fms only
void amx_fma64_vec(unsigned dst, unsigned x_reg, unsigned y_reg); // Elementwise product, full fma only
void amx_fma32_vec(unsigned dst, unsigned x_reg, unsigned y_reg); // Elementwise product, full fma only
void amx_fma16_vec(unsigned dst, unsigned x_reg, unsigned y_reg); // Elementwise product, full fma only
void amx_fms64_vec(unsigned dst, unsigned x_reg, unsigned y_reg); // Elementwise product, full fms only
void amx_fms32_vec(unsigned dst, unsigned x_reg, unsigned y_reg); // Elementwise product, full fms only
void amx_fms16_vec(unsigned dst, unsigned x_reg, unsigned y_reg); // Elementwise product, full fms only

#endif // AMX_H
