/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 08.508.001
* Copyright (C) 2012 Sony Interactive Entertainment Inc.
*/

#ifndef __HOST_STDINT_H__
#define  __HOST_STDINT_H__

#ifdef _WIN32
#ifdef _MSC_VER 

#if _MSC_VER < 1600

/* for VS2005 and VS2008 */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long long int64_t;

#else /* _MSC_VER < 1600 */

/* for VS2010 later */
#include <stdint.h>

#endif /* _MSC_VER < 1600 */
#else /* _MSC_VER */
/* for msys */
#include <stdint.h>

#endif /* _MSC_VER */

#else /* WIN32 */

/* for Linux */
#include <stdint.h>  

#endif /* WIN32*/
#endif /* __HOST_STDINT_H__*/
