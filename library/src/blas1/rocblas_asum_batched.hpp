/* ************************************************************************
 * Copyright 2016-2022 Advanced Micro Devices, Inc.
 * ************************************************************************ */

#pragma once

#include "rocblas_asum.hpp"
#include "rocblas_reduction_impl.hpp"

template <rocblas_int NB, typename U, typename To>
rocblas_status rocblas_asum_batched_template(rocblas_handle handle,
                                             rocblas_int    n,
                                             U              x,
                                             rocblas_stride shiftx,
                                             rocblas_int    incx,
                                             rocblas_int    batch_count,
                                             To*            workspace,
                                             To*            results)
{
    static constexpr bool           isbatched = true;
    static constexpr rocblas_stride stridex_0 = 0;
    return rocblas_reduction_template<NB,
                                      isbatched,
                                      rocblas_fetch_asum<To>,
                                      rocblas_reduce_sum,
                                      rocblas_finalize_identity>(
        handle, n, x, shiftx, incx, stridex_0, batch_count, results, workspace);
}
