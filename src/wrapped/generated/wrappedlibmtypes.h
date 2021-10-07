/*******************************************************************
 * File automatically generated by rebuild_wrappers.py (v2.0.0.10) *
 *******************************************************************/
#ifndef __wrappedlibmTYPES_H_
#define __wrappedlibmTYPES_H_

#ifndef LIBNAME
#error You should only #include this file inside a wrapped*.c file
#endif
#ifndef ADDED_FUNCTIONS
#define ADDED_FUNCTIONS() 
#endif

typedef uint64_t (*UFs_t)(void*);
typedef float (*fFf_t)(float);
typedef double (*dFd_t)(double);
typedef double (*KFK_t)(double);
typedef float (*fFff_t)(float, float);
typedef double (*dFdd_t)(double, double);
typedef double (*KFKK_t)(double, double);
typedef double (*KFKp_t)(double, void*);
typedef void* (*pFps_t)(void*, void*);
typedef uint64_t (*UFsvvs_t)(void*, void, void, void*);
typedef void* (*pFpsvvvvs_t)(void*, void*, void, void, void, void, void*);

#define SUPER() ADDED_FUNCTIONS() \
	GO(cacosf, UFs_t) \
	GO(cacoshf, UFs_t) \
	GO(casinf, UFs_t) \
	GO(casinhf, UFs_t) \
	GO(catanf, UFs_t) \
	GO(catanhf, UFs_t) \
	GO(ccosf, UFs_t) \
	GO(ccoshf, UFs_t) \
	GO(cexpf, UFs_t) \
	GO(clogf, UFs_t) \
	GO(cprojf, UFs_t) \
	GO(csinf, UFs_t) \
	GO(csinhf, UFs_t) \
	GO(csqrtf, UFs_t) \
	GO(ctanf, UFs_t) \
	GO(ctanhf, UFs_t) \
	GO(__acosf_finite, fFf_t) \
	GO(__acoshf_finite, fFf_t) \
	GO(__asinf_finite, fFf_t) \
	GO(__coshf_finite, fFf_t) \
	GO(__exp2f_finite, fFf_t) \
	GO(__expf_finite, fFf_t) \
	GO(__log10f_finite, fFf_t) \
	GO(__log2f_finite, fFf_t) \
	GO(__logf_finite, fFf_t) \
	GO(__sinhf_finite, fFf_t) \
	GO(__sqrtf_finite, fFf_t) \
	GO(__acos_finite, dFd_t) \
	GO(__acosh_finite, dFd_t) \
	GO(__asin_finite, dFd_t) \
	GO(__cosh_finite, dFd_t) \
	GO(__exp2_finite, dFd_t) \
	GO(__exp_finite, dFd_t) \
	GO(__log10_finite, dFd_t) \
	GO(__log2_finite, dFd_t) \
	GO(__log_finite, dFd_t) \
	GO(__sinh_finite, dFd_t) \
	GO(__sqrt_finite, dFd_t) \
	GO(acoshl, KFK_t) \
	GO(acosl, KFK_t) \
	GO(asinhl, KFK_t) \
	GO(asinl, KFK_t) \
	GO(atanhl, KFK_t) \
	GO(cbrtl, KFK_t) \
	GO(erfcl, KFK_t) \
	GO(erfl, KFK_t) \
	GO(ldexpl, KFK_t) \
	GO(lgammal, KFK_t) \
	GO(logl, KFK_t) \
	GO(tgammal, KFK_t) \
	GO(__atan2f_finite, fFff_t) \
	GO(__hypotf_finite, fFff_t) \
	GO(__powf_finite, fFff_t) \
	GO(__atan2_finite, dFdd_t) \
	GO(__hypot_finite, dFdd_t) \
	GO(__pow_finite, dFdd_t) \
	GO(fmodl, KFKK_t) \
	GO(powl, KFKK_t) \
	GO(frexpl, KFKp_t) \
	GO(lgammal_r, KFKp_t) \
	GO(cpowf, UFsvvs_t)

#endif // __wrappedlibmTYPES_H_
