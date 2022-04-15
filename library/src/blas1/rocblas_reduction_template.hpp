/* ************************************************************************
 * Copyright 2016-2022 Advanced Micro Devices, Inc.
 * ************************************************************************ */

#pragma once

#include "handle.hpp"
#include "reduction_strided_batched.hpp"

template <rocblas_int NB,
          bool        ISBATCHED,
          typename FETCH,
          typename REDUCE,
          typename FINALIZE,
          typename U,
          typename Tr,
          typename Tw>
rocblas_status rocblas_reduction_template(rocblas_handle handle,
                                          rocblas_int    n,
                                          U              x,
                                          rocblas_stride shiftx,
                                          rocblas_int    incx,
                                          rocblas_stride stridex,
                                          rocblas_int    batch_count,
                                          Tr*            results,
                                          Tw*            workspace)
{
    return rocblas_reduction_strided_batched<NB, FETCH, REDUCE, FINALIZE>(
        handle, n, x, shiftx, incx, stridex, batch_count, workspace, results);
}
