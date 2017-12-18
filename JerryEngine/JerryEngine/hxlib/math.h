/**
 * Implementation of floating point operations.
 * Only a subset of math operations are implemented currently,will be
 * added more routines in the future.
 */

#ifndef __MATH_H__
#define __MATH_H__

#include "__hxcomm.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#ifdef __LITTLE_ENDIAN
#define __HI(x) *(1 + (int *) &x)
#define __LO(x) *(int *) &x
#else /* !__LITTLE_ENDIAN */
#define __HI(x) *(int *) &x
#define __LO(x) *(1 + (int *) &x)
#endif /* __LITTLE_ENDIAN */

#if 0 /* The following definitions can not pass VS's compiling,replace them.*/
	/* General Constants. */
#define INFINITY    (1.0/0.0)
#define NAN         (0.0/0.0)
#define HUGE_VAL    INFINITY
#endif

	/**
	 * The following definitions are got from Visual Studio's C
	 * Runtime Library souce code.
	 */
#ifndef _HUGE_ENUF
#define _HUGE_ENUF 1e+300   /* _HUGE_ENUF * _HUGE_ENUF must overflow.*/
#endif //_HUGE_ENUF

#define INFINITY ((float)(_HUGE_ENUF * _HUGE_ENUF)) /* cause warning C4756.*/
#define HUGE_VALD ((double)INFINITY)
#define HUGE_VALF ((float)INFINITY)
#define HUGE_VALL ((long double)INFINITY)
#define NAN ((float)(INFINITY * 0.0F))

#define isnan(x)    ((x) != (x))
#define isinf(x)    (((x) == INFINITY) || ((x) == -INFINITY))
#define isfinite(x) (!(isinf(x)) && (x != NAN))

	/* Exponential and Logarithmic constants. */
#define M_E        2.7182818284590452353602874713526625
#define M_SQRT2    1.4142135623730950488016887242096981
#define M_SQRT1_2  0.7071067811865475244008443621048490
#define M_LOG2E    1.4426950408889634073599246810018921
#define M_LOG10E   0.4342944819032518276511289189166051
#define M_LN2      0.6931471805599453094172321214581765
#define M_LN10     2.3025850929940456840179914546843642

	/* Trigonometric Constants. */
#define M_PI       3.1415926535897932384626433832795029
#define M_PI_2     1.5707963267948966192313216916397514
#define M_PI_4     0.7853981633974483096156608458198757
#define M_1_PI     0.3183098861837906715377675267450287
#define M_2_PI     0.6366197723675813430755350534900574
#define M_2_SQRTPI 1.1283791670955125738961589031215452

	/* Trigonometric functions. */
	double cos(double);
	double sin(double);
	double tan(double);
	double acos(double);
	double asin(double);
	double atan(double);
	double atan2(double, double);

	/* Exponential and logarithmic functions. */
	double exp(double);
	double log(double);

	/* Power functions. */
	double pow(double, double);
	double sqrt(double);

	/* Rounding and remainder functions. */
	double ceil(double);
	double floor(double);

	/* Other functions. */
	double fabs(double);
	double fmod(double, double);

	double nextafter(double, double);

	/*
	* Functions callable from C, intended to support IEEE arithmetic.
	*/
	double copysign(double x, double y);
	double scalbn(double x, int n);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif //__MATH_H__
