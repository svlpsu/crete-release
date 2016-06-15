// QEMU Helper function stubs

#include "fpu/softfloat.h"
#include "cpu.h"

// *** FPU ***

//floatx80 float64_to_floatx80( float64 a STATUS_PARAM )
//{
//	return make_floatx80(0, 0);
//}

//float64 floatx80_to_float64( floatx80 a STATUS_PARAM )
//{
//    return make_float64(0);
//}

// *** OP_HELPER ***

//typedef uint64_t target_ulong; // Just declare it as 64bit, just stubs anyway.

// ** FPU **

void helper_flds_FT0(uint32_t val)
{
    (void)val;
}

void helper_fldl_FT0(uint64_t val)
{
    (void)val;
}

void helper_fildl_FT0(int32_t val)
{
    (void)val;
}

void helper_flds_ST0(uint32_t val)
{
    (void)val;
}

void helper_fldl_ST0(uint64_t val)
{
    (void)val;
}

void helper_fildl_ST0(int32_t val)
{
    (void)val;
}

void helper_fildll_ST0(int64_t val)
{
    (void)val;
}

uint32_t helper_fsts_ST0(void)
{
    return 0;
}

uint64_t helper_fstl_ST0(void)
{
    return 0;
}

int32_t helper_fist_ST0(void)
{
    return 0;
}

int32_t helper_fistl_ST0(void)
{
    return 0;
}

int64_t helper_fistll_ST0(void)
{
    return 0;
}

int32_t helper_fistt_ST0(void)
{
    return 0;
}

int32_t helper_fisttl_ST0(void)
{
    return 0;
}

int64_t helper_fisttll_ST0(void)
{
    return 0;
}

void helper_fldt_ST0(target_ulong ptr)
{
    (void)ptr;
}

void helper_fstt_ST0(target_ulong ptr)
{
    (void)ptr;
}

void helper_fpush(void)
{
}

void helper_fpop(void)
{
}

void helper_fdecstp(void)
{
}

void helper_fincstp(void)
{
}

/* FPU move */

void helper_ffree_STN(int st_index)
{
    (void)st_index;
}

void helper_fmov_ST0_FT0(void)
{
}

void helper_fmov_FT0_STN(int st_index)
{
    (void)st_index;
}

void helper_fmov_ST0_STN(int st_index)
{
    (void)st_index;
}

void helper_fmov_STN_ST0(int st_index)
{
    (void)st_index;
}

void helper_fxchg_ST0_STN(int st_index)
{
    (void)st_index;
}

/* FPU operations */

void helper_fcom_ST0_FT0(void)
{
}

void helper_fucom_ST0_FT0(void)
{
}

void helper_fcomi_ST0_FT0(void)
{
}

void helper_fucomi_ST0_FT0(void)
{
}

void helper_fadd_ST0_FT0(void)
{
}

void helper_fmul_ST0_FT0(void)
{
}

void helper_fsub_ST0_FT0(void)
{
}

void helper_fsubr_ST0_FT0(void)
{
}

void helper_fdiv_ST0_FT0(void)
{
}

void helper_fdivr_ST0_FT0(void)
{
}

/* fp operations between STN and ST0 */

void helper_fadd_STN_ST0(int st_index)
{
    (void)st_index;
}

void helper_fmul_STN_ST0(int st_index)
{
    (void)st_index;
}

void helper_fsub_STN_ST0(int st_index)
{
    (void)st_index;
}

void helper_fsubr_STN_ST0(int st_index)
{
    (void)st_index;
}

void helper_fdiv_STN_ST0(int st_index)
{
    (void)st_index;
}

void helper_fdivr_STN_ST0(int st_index)
{
    (void)st_index;
}

/* misc FPU operations */
void helper_fchs_ST0(void)
{
}

void helper_fabs_ST0(void)
{
}

void helper_fld1_ST0(void)
{
}

void helper_fldl2t_ST0(void)
{
}

void helper_fldl2e_ST0(void)
{
}

void helper_fldpi_ST0(void)
{
}

void helper_fldlg2_ST0(void)
{
}

void helper_fldln2_ST0(void)
{
}

void helper_fldz_ST0(void)
{
}

void helper_fldz_FT0(void)
{
}

uint32_t helper_fnstsw(void)
{
    return 0;
}

uint32_t helper_fnstcw(void)
{
    return 0;
}

static void update_fp_status(void)
{
}

void helper_fldcw(uint32_t val)
{
    (void)val;
}

void helper_fclex(void)
{
}

void helper_fwait(void)
{
}

void helper_fninit(void)
{
}

/* broken thread support */
void helper_lock(void)
{
}

void helper_unlock(void)
{
}

// TEMPORARY!

void cpu_loop_exit(CPUState *env1) // TODO: only temporary until generic excepion handler is implemented!
{
    (void)env1;
}

// TEMPORARY! XMM MMX Testing
/*
typedef XMMReg Reg;

void helper_movl_mm_T0_xmm(Reg *d, uint32_t val)
{
}

void helper_pshufd_xmm(Reg *d, Reg *s, int order)
{
}

void helper_pxor_xmm(Reg *d, Reg *s)
{
}
*/
