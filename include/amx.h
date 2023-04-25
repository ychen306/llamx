#ifndef AMX_H
#define AMX_H

// Simple loads and stores
void amx_ldx(unsigned dst, void *src_ptr); // Loads 64 bytes to an x register
void amx_ldy(unsigned dst, void *src_ptr); // Loads 64 bytes to a y register
void amx_stx(void *dst_ptr, unsigned src); // Stores 64 bytes from an x register
void amx_sty(void *dst_ptr, unsigned src); // Stores 64 bytes from a y register
void amx_ldz(unsigned dst, void *src_ptr); // Loads 64 bytes to a z register
void amx_stz(void *dst_ptr, unsigned src); // Stores 64 bytes from a z register
void amx_ldzi(unsigned dst, void *src_ptr); // Loads 64 bytes to a half of two z registers
void amx_stzi(void *dst_ptr, unsigned src); // Stores 64 bytes from a half of two z registers

// Column loads and stores
void amx_ldz_col(unsigned dst, void *src_ptr); // Loads 512 bytes to a z column (8 z registers with the same lower 3 bits)
void amx_ldz_col2(unsigned dst, void *src_ptr); // Loads 1024 bytes to two z columns (16 z registers with the same lower 2 bits)
void amx_ldz_col4(unsigned dst, void *src_ptr); // Loads 2048 bytes to four z columns (32 z registers with the same lowest bit)
void amx_ldz_col8(unsigned dst, void *src_ptr); // Loads 4096 bytes to the entire z grid
void amx_stz_col(void *dst_ptr, unsigned src); // Stores 512 bytes from a z column (8 z registers with the same lower 3 bits)
void amx_stz_col2(void *dst_ptr, unsigned src); // Stores 1024 bytes from two z columns (16 z registers with the same lower 2 bits)
void amx_stz_col4(void *dst_ptr, unsigned src); // Stores 2048 bytes from four z columns (32 z registers with the same lowest bit)
void amx_stz_col8(void *dst_ptr, unsigned src); // Stores 4096 bytes from the entire z grid

// Moves
void amx_mvxy(unsigned dst, unsigned src); // Move x register to y register
void amx_mvyx(unsigned dst, unsigned src); // Move y register to x register
void amx_mvxz(unsigned dst, unsigned src); // Move x register to z register
void amx_mvyz(unsigned dst, unsigned src); // Move y register to z register
void amx_mvzx(unsigned dst, unsigned src); // Move z register to x register
void amx_mvzy(unsigned dst, unsigned src); // Move z register to y register

// Floating-point operations
void amx_fma64_mat(unsigned z_col, unsigned x_reg, unsigned y_reg); // Outer product, full fma only
void amx_fma32_mat(unsigned z_col2, unsigned x_reg, unsigned y_reg); // Outer product, full fma only
void amx_fma16_mat(unsigned z_col4, unsigned x_reg, unsigned y_reg); // Outer product, full fma only
void amx_fms64_mat(unsigned z_col, unsigned x_reg, unsigned y_reg); // Outer product, full fms only
void amx_fms32_mat(unsigned z_col2, unsigned x_reg, unsigned y_reg); // Outer product, full fms only
void amx_fms16_mat(unsigned z_col4, unsigned x_reg, unsigned y_reg); // Outer product, full fms only
void amx_fma64_vec(unsigned z_reg, unsigned x_reg, unsigned y_reg); // Elementwise product, full fma only
void amx_fma32_vec(unsigned z_reg, unsigned x_reg, unsigned y_reg); // Elementwise product, full fma only
void amx_fma16_vec(unsigned z_reg, unsigned x_reg, unsigned y_reg); // Elementwise product, full fma only
void amx_fms64_vec(unsigned z_reg, unsigned x_reg, unsigned y_reg); // Elementwise product, full fms only
void amx_fms32_vec(unsigned z_reg, unsigned x_reg, unsigned y_reg); // Elementwise product, full fms only
void amx_fms16_vec(unsigned z_reg, unsigned x_reg, unsigned y_reg); // Elementwise product, full fms only

#endif // AMX_H
