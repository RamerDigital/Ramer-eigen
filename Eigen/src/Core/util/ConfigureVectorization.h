// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2008-2018 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2020, Arm Limited and Contributors
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef EIGEN_CONFIGURE_VECTORIZATION_H
#define EIGEN_CONFIGURE_VECTORIZATION_H

//------------------------------------------------------------------------------------------
// Static and dynamic alignment control
//
// The main purpose of this section is to define EIGEN_MAX_ALIGN_BYTES and EIGEN_MAX_STATIC_ALIGN_BYTES
// as the maximal boundary in bytes on which dynamically and statically allocated data may be alignment respectively.
// The values of EIGEN_MAX_ALIGN_BYTES and EIGEN_MAX_STATIC_ALIGN_BYTES can be specified by the user. If not,
// a default value is automatically computed based on architecture, compiler, and OS.
//
// This section also defines macros EIGEN_ALIGN_TO_BOUNDARY(N) and the shortcuts EIGEN_ALIGN{8,16,32,_MAX}
// to be used to declare statically aligned buffers.
//------------------------------------------------------------------------------------------

/* EIGEN_ALIGN_TO_BOUNDARY(n) forces data to be n-byte aligned. This is used to satisfy SIMD requirements.
 * However, we do that EVEN if vectorization (EIGEN_VECTORIZE) is disabled,
 * so that vectorization doesn't affect binary compatibility.
 *
 * If we made alignment depend on whether or not EIGEN_VECTORIZE is defined, it would be impossible to link
 * vectorized and non-vectorized code.
 */
#if (defined EIGEN_CUDACC)
#define EIGEN_ALIGN_TO_BOUNDARY(n) __align__(n)
#define EIGEN_ALIGNOF(x) __alignof(x)
#else
#define EIGEN_ALIGN_TO_BOUNDARY(n) alignas(n)
#define EIGEN_ALIGNOF(x) alignof(x)
#endif

// Align to the boundary that avoids false sharing.
//   https://en.cppreference.com/w/cpp/thread/hardware_destructive_interference_size
// There is a bug in android NDK < r26 where the macro is defined but std::hardware_destructive_interference_size
// still does not exist.
#if defined(__cpp_lib_hardware_interference_size) && __cpp_lib_hardware_interference_size >= 201603 && \
    (!EIGEN_OS_ANDROID || __NDK_MAJOR__ + 0 >= 26)
#include <new>
#define EIGEN_ALIGN_TO_AVOID_FALSE_SHARING EIGEN_ALIGN_TO_BOUNDARY(std::hardware_destructive_interference_size)
#else
// Overalign for the cache line size of 128 bytes (Apple M1)
#define EIGEN_ALIGN_TO_AVOID_FALSE_SHARING EIGEN_ALIGN_TO_BOUNDARY(128)
#endif

// If the user explicitly disable vectorization, then we also disable alignment
#if defined(EIGEN_DONT_VECTORIZE)
#if defined(EIGEN_GPUCC)
// GPU code is always vectorized and requires memory alignment for
// statically allocated buffers.
#define EIGEN_IDEAL_MAX_ALIGN_BYTES 16
#else
#define EIGEN_IDEAL_MAX_ALIGN_BYTES 0
#endif
#else
#define EIGEN_IDEAL_MAX_ALIGN_BYTES 16
#endif

// EIGEN_MIN_ALIGN_BYTES defines the minimal value for which the notion of explicit alignment makes sense
#define EIGEN_MIN_ALIGN_BYTES 16

// Defined the boundary (in bytes) on which the data needs to be aligned. Note
// that unless EIGEN_ALIGN is defined and not equal to 0, the data may not be
// aligned at all regardless of the value of this #define.

#if (defined(EIGEN_DONT_ALIGN_STATICALLY) || defined(EIGEN_DONT_ALIGN)) && defined(EIGEN_MAX_STATIC_ALIGN_BYTES) && \
    EIGEN_MAX_STATIC_ALIGN_BYTES > 0
#error EIGEN_MAX_STATIC_ALIGN_BYTES and EIGEN_DONT_ALIGN[_STATICALLY] are both defined with EIGEN_MAX_STATIC_ALIGN_BYTES!=0. Use EIGEN_MAX_STATIC_ALIGN_BYTES=0 as a synonym of EIGEN_DONT_ALIGN_STATICALLY.
#endif

// EIGEN_DONT_ALIGN_STATICALLY and EIGEN_DONT_ALIGN are deprecated
// They imply EIGEN_MAX_STATIC_ALIGN_BYTES=0
#if defined(EIGEN_DONT_ALIGN_STATICALLY) || defined(EIGEN_DONT_ALIGN)
#ifdef EIGEN_MAX_STATIC_ALIGN_BYTES
#undef EIGEN_MAX_STATIC_ALIGN_BYTES
#endif
#define EIGEN_MAX_STATIC_ALIGN_BYTES 0
#endif

#ifndef EIGEN_MAX_STATIC_ALIGN_BYTES

// Try to automatically guess what is the best default value for EIGEN_MAX_STATIC_ALIGN_BYTES

// 16 byte alignment is only useful for vectorization. Since it affects the ABI, we need to enable
// 16 byte alignment on all platforms where vectorization might be enabled. In theory we could always
// enable alignment, but it can be a cause of problems on some platforms, so we just disable it in
// certain common platform (compiler+architecture combinations) to avoid these problems.
// Only static alignment is really problematic (relies on nonstandard compiler extensions),
// try to keep heap alignment even when we have to disable static alignment.
#if EIGEN_COMP_GNUC && !(EIGEN_ARCH_i386_OR_x86_64 || EIGEN_ARCH_ARM_OR_ARM64 || EIGEN_ARCH_PPC || EIGEN_ARCH_IA64 || \
                         EIGEN_ARCH_MIPS || EIGEN_ARCH_LOONGARCH64)
#define EIGEN_GCC_AND_ARCH_DOESNT_WANT_STACK_ALIGNMENT 1
#else
#define EIGEN_GCC_AND_ARCH_DOESNT_WANT_STACK_ALIGNMENT 0
#endif

// static alignment is completely disabled with GCC 3, Sun Studio, and QCC/QNX
#if !EIGEN_GCC_AND_ARCH_DOESNT_WANT_STACK_ALIGNMENT && !EIGEN_COMP_SUNCC && !EIGEN_OS_QNX
#define EIGEN_ARCH_WANTS_STACK_ALIGNMENT 1
#else
#define EIGEN_ARCH_WANTS_STACK_ALIGNMENT 0
#endif

#if EIGEN_ARCH_WANTS_STACK_ALIGNMENT
#define EIGEN_MAX_STATIC_ALIGN_BYTES EIGEN_IDEAL_MAX_ALIGN_BYTES
#else
#define EIGEN_MAX_STATIC_ALIGN_BYTES 0
#endif

#endif

// If EIGEN_MAX_ALIGN_BYTES is defined, then it is considered as an upper bound for EIGEN_MAX_STATIC_ALIGN_BYTES
#if defined(EIGEN_MAX_ALIGN_BYTES) && EIGEN_MAX_ALIGN_BYTES < EIGEN_MAX_STATIC_ALIGN_BYTES
#undef EIGEN_MAX_STATIC_ALIGN_BYTES
#define EIGEN_MAX_STATIC_ALIGN_BYTES EIGEN_MAX_ALIGN_BYTES
#endif

#if EIGEN_MAX_STATIC_ALIGN_BYTES == 0 && !defined(EIGEN_DISABLE_UNALIGNED_ARRAY_ASSERT)
#define EIGEN_DISABLE_UNALIGNED_ARRAY_ASSERT
#endif

// At this stage, EIGEN_MAX_STATIC_ALIGN_BYTES>0 is the true test whether we want to align arrays on the stack or not.
// It takes into account both the user choice to explicitly enable/disable alignment (by setting
// EIGEN_MAX_STATIC_ALIGN_BYTES) and the architecture config (EIGEN_ARCH_WANTS_STACK_ALIGNMENT). Henceforth, only
// EIGEN_MAX_STATIC_ALIGN_BYTES should be used.

// Shortcuts to EIGEN_ALIGN_TO_BOUNDARY
#define EIGEN_ALIGN8 EIGEN_ALIGN_TO_BOUNDARY(8)
#define EIGEN_ALIGN16 EIGEN_ALIGN_TO_BOUNDARY(16)
#define EIGEN_ALIGN32 EIGEN_ALIGN_TO_BOUNDARY(32)
#define EIGEN_ALIGN64 EIGEN_ALIGN_TO_BOUNDARY(64)
#if EIGEN_MAX_STATIC_ALIGN_BYTES > 0
#define EIGEN_ALIGN_MAX EIGEN_ALIGN_TO_BOUNDARY(EIGEN_MAX_STATIC_ALIGN_BYTES)
#else
#define EIGEN_ALIGN_MAX
#endif

// Dynamic alignment control

#if defined(EIGEN_DONT_ALIGN) && defined(EIGEN_MAX_ALIGN_BYTES) && EIGEN_MAX_ALIGN_BYTES > 0
#error EIGEN_MAX_ALIGN_BYTES and EIGEN_DONT_ALIGN are both defined with EIGEN_MAX_ALIGN_BYTES!=0. Use EIGEN_MAX_ALIGN_BYTES=0 as a synonym of EIGEN_DONT_ALIGN.
#endif

#ifdef EIGEN_DONT_ALIGN
#ifdef EIGEN_MAX_ALIGN_BYTES
#undef EIGEN_MAX_ALIGN_BYTES
#endif
#define EIGEN_MAX_ALIGN_BYTES 0
#elif !defined(EIGEN_MAX_ALIGN_BYTES)
#define EIGEN_MAX_ALIGN_BYTES EIGEN_IDEAL_MAX_ALIGN_BYTES
#endif

#if EIGEN_IDEAL_MAX_ALIGN_BYTES > EIGEN_MAX_ALIGN_BYTES
#define EIGEN_DEFAULT_ALIGN_BYTES EIGEN_IDEAL_MAX_ALIGN_BYTES
#else
#define EIGEN_DEFAULT_ALIGN_BYTES EIGEN_MAX_ALIGN_BYTES
#endif

#ifndef EIGEN_UNALIGNED_VECTORIZE
#define EIGEN_UNALIGNED_VECTORIZE 1
#endif

//----------------------------------------------------------------------

// if alignment is disabled, then disable vectorization. Note: EIGEN_MAX_ALIGN_BYTES is the proper check, it takes into
// account both the user's will (EIGEN_MAX_ALIGN_BYTES,EIGEN_DONT_ALIGN) and our own platform checks
#if EIGEN_MAX_ALIGN_BYTES == 0
#ifndef EIGEN_DONT_VECTORIZE
#define EIGEN_DONT_VECTORIZE
#endif
#endif

#if !(defined(EIGEN_DONT_VECTORIZE) || defined(EIGEN_GPUCC))

#if (defined __ARM_NEON) || (defined __ARM_NEON__)

#define EIGEN_VECTORIZE
#define EIGEN_VECTORIZE_NEON
#include <arm_neon.h>

#endif

#endif

// Following the Arm ACLE arm_neon.h should also include arm_fp16.h but not all
// compilers seem to follow this. We therefore include it explicitly.
// See also: https://bugs.llvm.org/show_bug.cgi?id=47955
#if defined(EIGEN_HAS_ARM64_FP16_SCALAR_ARITHMETIC)
#include <arm_fp16.h>
#endif

// Enable FMA for ARM.
#if defined(__ARM_FEATURE_FMA)
#define EIGEN_VECTORIZE_FMA
#endif



/** \brief Namespace containing all symbols from the %Eigen library. */
// IWYU pragma: private
#include "../InternalHeaderCheck.h"

namespace Eigen {

inline static const char *SimdInstructionSetsInUse(void) {
#if defined(EIGEN_VECTORIZE_NEON)
  return "ARM NEON";
#else
  return "None";
#endif
}

}  // end namespace Eigen

#endif  // EIGEN_CONFIGURE_VECTORIZATION_H
