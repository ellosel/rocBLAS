/* ************************************************************************
 * Copyright 2016-2021 Advanced Micro Devices, Inc.
 * ************************************************************************ */

#pragma once

#include "check_numerics_matrix.hpp"
#include "check_numerics_vector.hpp"
#include "handle.hpp"

template <rocblas_int DIM_X,
          rocblas_int DIM_Y,
          rocblas_int WIN,
          bool        CONJ,
          typename T,
          typename U,
          typename V,
          typename W>
__global__ __launch_bounds__(DIM_X* DIM_Y) void ger_kernel(rocblas_int    m,
                                                           rocblas_int    n,
                                                           W              alpha_device_host,
                                                           rocblas_stride stride_alpha,
                                                           const U __restrict__ xa,
                                                           ptrdiff_t      shiftx,
                                                           rocblas_int    incx,
                                                           rocblas_stride stridex,
                                                           const U __restrict__ ya,
                                                           ptrdiff_t      shifty,
                                                           rocblas_int    incy,
                                                           rocblas_stride stridey,
                                                           V              Aa,
                                                           ptrdiff_t      shifta,
                                                           rocblas_int    lda,
                                                           rocblas_stride strideA)
{
    __shared__ T xdata[DIM_X];
    __shared__ T ydata[DIM_Y * WIN];

    auto alpha = load_scalar(alpha_device_host, hipBlockIdx_z, stride_alpha);
    if(!alpha)
        return;

    const T* __restrict__ x = load_ptr_batch(xa, hipBlockIdx_z, shiftx, stridex);
    const T* __restrict__ y = load_ptr_batch(ya, hipBlockIdx_z, shifty, stridey);

    T* A = load_ptr_batch(Aa, hipBlockIdx_z, shifta, strideA);

    int tx = hipBlockIdx_x * hipBlockDim_x + hipThreadIdx_x;
    int ty = hipBlockIdx_y * hipBlockDim_y + hipThreadIdx_y;
    ty *= WIN;

    // shared data base index
    int tyi = hipThreadIdx_y * WIN;

    if(hipThreadIdx_y == 0)
    {
        xdata[hipThreadIdx_x] = tx < m ? x[tx * incx] : 0;
    }

    if(hipThreadIdx_x < WIN)
    {
        ydata[tyi + hipThreadIdx_x]
            = (ty + hipThreadIdx_x < n) ? y[(ty + hipThreadIdx_x) * incy] : 0;
    }

    __syncthreads();

    if(tx < m)
    {
        T x_value = alpha * xdata[hipThreadIdx_x];

        for(int i = 0; i < WIN; i++)
        {
            int yi = ty + i;
            if(yi < n)
                A[tx + size_t(lda) * yi]
                    += x_value * (CONJ ? conj(ydata[tyi + i]) : ydata[tyi + i]);
        }
    }
}

template <bool CONJ, typename T, typename U, typename V, typename W>
inline rocblas_status rocblas_ger_arg_check(rocblas_int    m,
                                            rocblas_int    n,
                                            const W*       alpha,
                                            rocblas_stride stride_alpha,
                                            const U*       x,
                                            rocblas_int    offsetx,
                                            rocblas_int    incx,
                                            rocblas_int    stridex,
                                            const U*       y,
                                            rocblas_int    offsety,
                                            rocblas_int    incy,
                                            rocblas_int    stridey,
                                            V*             A,
                                            rocblas_int    offsetA,
                                            rocblas_int    lda,
                                            rocblas_int    strideA,
                                            rocblas_int    batch_count)
{
    if(m < 0 || n < 0 || !incx || !incy || lda < m || lda < 1 || batch_count < 0)
        return rocblas_status_invalid_size;

    if(!m || !n || !batch_count)
        return rocblas_status_success;

    if(!alpha || !x || !y || !A)
        return rocblas_status_invalid_pointer;

    return rocblas_status_continue;
}

template <bool CONJ, typename T, typename U, typename V, typename W>
ROCBLAS_EXPORT_NOINLINE rocblas_status rocblas_ger_template(rocblas_handle handle,
                                                            rocblas_int    m,
                                                            rocblas_int    n,
                                                            const W*       alpha,
                                                            rocblas_stride stride_alpha,
                                                            const U*       x,
                                                            rocblas_int    offsetx,
                                                            rocblas_int    incx,
                                                            rocblas_int    stridex,
                                                            const U*       y,
                                                            rocblas_int    offsety,
                                                            rocblas_int    incy,
                                                            rocblas_int    stridey,
                                                            V*             A,
                                                            rocblas_int    offsetA,
                                                            rocblas_int    lda,
                                                            rocblas_int    strideA,
                                                            rocblas_int    batch_count)
{
    // Quick return if possible. Not Argument error
    if(!m || !n || !batch_count)
        return rocblas_status_success;

    hipStream_t rocblas_stream = handle->get_stream();

    // in case of negative inc shift pointer to end of data for negative indexing tid*inc
    auto shiftx = incx < 0 ? offsetx - ptrdiff_t(incx) * (m - 1) : offsetx;
    auto shifty = incy < 0 ? offsety - ptrdiff_t(incy) * (n - 1) : offsety;

    static constexpr int DIM_X   = 32;
    static constexpr int DIM_Y   = 32;
    static constexpr int WIN     = 8; // work item number of elements to process
    rocblas_int          blocksX = (m - 1) / DIM_X + 1;
    rocblas_int          blocksY = (n - 1) / (DIM_Y * WIN) + 1; // WIN columns/work item

    dim3 grid(blocksX, blocksY, batch_count);
    dim3 threads(DIM_X, DIM_Y);

    if(handle->pointer_mode == rocblas_pointer_mode_device)
        hipLaunchKernelGGL((ger_kernel<DIM_X, DIM_Y, WIN, CONJ, T>),
                           grid,
                           threads,
                           0,
                           rocblas_stream,
                           m,
                           n,
                           alpha,
                           stride_alpha,
                           x,
                           shiftx,
                           incx,
                           stridex,
                           y,
                           shifty,
                           incy,
                           stridey,
                           A,
                           offsetA,
                           lda,
                           strideA);
    else
        hipLaunchKernelGGL((ger_kernel<DIM_X, DIM_Y, WIN, CONJ, T>),
                           grid,
                           threads,
                           0,
                           rocblas_stream,
                           m,
                           n,
                           *alpha,
                           stride_alpha,
                           x,
                           shiftx,
                           incx,
                           stridex,
                           y,
                           shifty,
                           incy,
                           stridey,
                           A,
                           offsetA,
                           lda,
                           strideA);
    return rocblas_status_success;
}

template <typename T, typename U>
rocblas_status rocblas_ger_check_numerics(const char*    function_name,
                                          rocblas_handle handle,
                                          rocblas_int    m,
                                          rocblas_int    n,
                                          U              A,
                                          rocblas_int    offset_a,
                                          rocblas_int    lda,
                                          rocblas_stride stride_a,
                                          T              x,
                                          rocblas_int    offset_x,
                                          rocblas_int    inc_x,
                                          rocblas_stride stride_x,
                                          T              y,
                                          rocblas_int    offset_y,
                                          rocblas_int    inc_y,
                                          rocblas_stride stride_y,
                                          rocblas_int    batch_count,
                                          const int      check_numerics,
                                          bool           is_input)
{
    rocblas_status check_numerics_status
        = rocblas_check_numerics_ge_matrix_template(function_name,
                                                    handle,
                                                    rocblas_operation_none,
                                                    m,
                                                    n,
                                                    A,
                                                    offset_a,
                                                    lda,
                                                    stride_a,
                                                    batch_count,
                                                    check_numerics,
                                                    is_input);
    if(check_numerics_status != rocblas_status_success)
        return check_numerics_status;

    check_numerics_status = rocblas_check_numerics_vector_template(function_name,
                                                                   handle,
                                                                   m,
                                                                   x,
                                                                   offset_x,
                                                                   inc_x,
                                                                   stride_x,
                                                                   batch_count,
                                                                   check_numerics,
                                                                   is_input);
    if(check_numerics_status != rocblas_status_success)
        return check_numerics_status;

    check_numerics_status = rocblas_check_numerics_vector_template(function_name,
                                                                   handle,
                                                                   n,
                                                                   y,
                                                                   offset_y,
                                                                   inc_y,
                                                                   stride_y,
                                                                   batch_count,
                                                                   check_numerics,
                                                                   is_input);

    return check_numerics_status;
}
