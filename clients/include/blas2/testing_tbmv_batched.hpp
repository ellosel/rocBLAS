/* ************************************************************************
 * Copyright (C) 2018-2024 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell cop-
 * ies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IM-
 * PLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNE-
 * CTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 *
 * ************************************************************************ */

#pragma once

#include "testing_common.hpp"

template <typename T>
void testing_tbmv_batched_bad_arg(const Arguments& arg)
{
    auto rocblas_tbmv_batched_fn
        = arg.api & c_API_FORTRAN ? rocblas_tbmv_batched<T, true> : rocblas_tbmv_batched<T, false>;
    auto rocblas_tbmv_batched_fn_64 = arg.api & c_API_FORTRAN ? rocblas_tbmv_batched_64<T, true>
                                                              : rocblas_tbmv_batched_64<T, false>;

    const int64_t           N                 = 100;
    const int64_t           K                 = 5;
    const int64_t           lda               = 100;
    const int64_t           incx              = 1;
    const int64_t           batch_count       = 5;
    const int64_t           banded_matrix_row = K + 1;
    const rocblas_fill      uplo              = rocblas_fill_upper;
    const rocblas_operation transA            = rocblas_operation_none;
    const rocblas_diagonal  diag              = rocblas_diagonal_non_unit;

    rocblas_local_handle handle{arg};

    // Allocate device memory
    device_batch_matrix<T> dAb(banded_matrix_row, N, lda, batch_count);
    device_batch_vector<T> dx(N, incx, batch_count);

    // Check device memory allocation
    CHECK_DEVICE_ALLOCATION(dAb.memcheck());
    CHECK_DEVICE_ALLOCATION(dx.memcheck());

    DAPI_EXPECT(rocblas_status_invalid_value,
                rocblas_tbmv_batched_fn,
                (handle,
                 rocblas_fill_full,
                 transA,
                 diag,
                 N,
                 K,
                 dAb.ptr_on_device(),
                 lda,
                 dx.ptr_on_device(),
                 incx,
                 batch_count));
    // arg_checks code shared so transA, diag tested only in non-batched

    DAPI_EXPECT(
        rocblas_status_invalid_pointer,
        rocblas_tbmv_batched_fn,
        (handle, uplo, transA, diag, N, K, nullptr, lda, dx.ptr_on_device(), incx, batch_count));

    DAPI_EXPECT(
        rocblas_status_invalid_pointer,
        rocblas_tbmv_batched_fn,
        (handle, uplo, transA, diag, N, K, dAb.ptr_on_device(), lda, nullptr, incx, batch_count));

    DAPI_EXPECT(rocblas_status_invalid_handle,
                rocblas_tbmv_batched_fn,
                (nullptr,
                 uplo,
                 transA,
                 diag,
                 N,
                 K,
                 dAb.ptr_on_device(),
                 lda,
                 dx.ptr_on_device(),
                 incx,
                 batch_count));

    // Adding test to check that if batch_count == 0 we can pass in nullptrs and get a success.
    DAPI_EXPECT(rocblas_status_success,
                rocblas_tbmv_batched_fn,
                (handle, uplo, transA, diag, N, K, nullptr, lda, nullptr, incx, 0));
}

template <typename T>
void testing_tbmv_batched(const Arguments& arg)
{
    auto rocblas_tbmv_batched_fn
        = arg.api & c_API_FORTRAN ? rocblas_tbmv_batched<T, true> : rocblas_tbmv_batched<T, false>;
    auto rocblas_tbmv_batched_fn_64 = arg.api & c_API_FORTRAN ? rocblas_tbmv_batched_64<T, true>
                                                              : rocblas_tbmv_batched_64<T, false>;

    int64_t           N                 = arg.N;
    int64_t           K                 = arg.K;
    int64_t           lda               = arg.lda;
    int64_t           incx              = arg.incx;
    char              char_uplo         = arg.uplo;
    char              char_diag         = arg.diag;
    rocblas_fill      uplo              = char2rocblas_fill(char_uplo);
    rocblas_operation transA            = char2rocblas_operation(arg.transA);
    rocblas_diagonal  diag              = char2rocblas_diagonal(char_diag);
    int64_t           batch_count       = arg.batch_count;
    const int64_t     banded_matrix_row = K + 1;

    rocblas_local_handle handle{arg};

    // argument sanity check before allocating invalid memory
    bool invalid_size = N < 0 || K < 0 || lda < banded_matrix_row || !incx || batch_count < 0;
    if(invalid_size || !N || !batch_count)
    {
        DAPI_EXPECT(invalid_size ? rocblas_status_invalid_size : rocblas_status_success,
                    rocblas_tbmv_batched_fn,
                    (handle, uplo, transA, diag, N, K, nullptr, lda, nullptr, incx, batch_count));

        return;
    }

    // Naming: `h` is in CPU (host) memory(eg hAb), `d` is in GPU (device) memory (eg dAb).
    // Allocate host memory
    host_batch_matrix<T> hAb(banded_matrix_row, N, lda, batch_count);
    host_batch_vector<T> hx(N, incx, batch_count);
    host_batch_vector<T> hx_gold(N, incx, batch_count);

    // Check host memory allocation
    CHECK_HIP_ERROR(hAb.memcheck());
    CHECK_HIP_ERROR(hx.memcheck());
    CHECK_HIP_ERROR(hx_gold.memcheck());

    // Allocate device memory
    device_batch_matrix<T> dAb(banded_matrix_row, N, lda, batch_count);
    device_batch_vector<T> dx(N, incx, batch_count);

    // Check device memory allocation
    CHECK_DEVICE_ALLOCATION(dx.memcheck());
    CHECK_DEVICE_ALLOCATION(dAb.memcheck());

    // Initialize data on host memory
    // Initializing the banded-matrix 'hAb' as a general matrix as the banded matrix is not triangular
    rocblas_init_matrix(
        hAb, arg, rocblas_client_never_set_nan, rocblas_client_general_matrix, true);
    rocblas_init_vector(hx, arg, rocblas_client_never_set_nan, false, true);

    hx_gold.copy_from(hx);

    // Copy data from CPU to device
    CHECK_HIP_ERROR(dAb.transfer_from(hAb));
    CHECK_HIP_ERROR(dx.transfer_from(hx));

    double cpu_time_used;
    double rocblas_error = 0.0;

    /* =====================================================================
           ROCBLAS
    =================================================================== */

    if(arg.unit_check || arg.norm_check)
    {
        // pointer mode shouldn't matter here
        CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_device));
        handle.pre_test(arg);
        DAPI_CHECK(rocblas_tbmv_batched_fn,
                   (handle,
                    uplo,
                    transA,
                    diag,
                    N,
                    K,
                    dAb.ptr_on_device(),
                    lda,
                    dx.ptr_on_device(),
                    incx,
                    batch_count));
        handle.post_test(arg);

        if(arg.repeatability_check)
        {
            host_batch_vector<T> hx_copy(N, incx, batch_count);
            CHECK_HIP_ERROR(hx_copy.memcheck());
            CHECK_HIP_ERROR(hx.transfer_from(dx));

            // multi-GPU support
            int device_id, device_count;
            CHECK_HIP_ERROR(hipGetDeviceCount(&device_count));
            for(int dev_id = 0; dev_id < device_count; dev_id++)
            {
                CHECK_HIP_ERROR(hipGetDevice(&device_id));
                if(device_id != dev_id)
                    CHECK_HIP_ERROR(hipSetDevice(dev_id));

                //New rocblas handle for new device
                rocblas_local_handle handle_copy{arg};

                // Allocate device memory
                device_batch_matrix<T> dAb_copy(banded_matrix_row, N, lda, batch_count);
                device_batch_vector<T> dx_copy(N, incx, batch_count);

                // Check device memory allocation
                CHECK_DEVICE_ALLOCATION(dx_copy.memcheck());
                CHECK_DEVICE_ALLOCATION(dAb_copy.memcheck());

                // copy data from CPU to device
                CHECK_HIP_ERROR(dAb_copy.transfer_from(hAb));

                CHECK_ROCBLAS_ERROR(
                    rocblas_set_pointer_mode(handle_copy, rocblas_pointer_mode_device));

                for(int runs = 0; runs < arg.iters; runs++)
                {
                    CHECK_HIP_ERROR(dx_copy.transfer_from(hx_gold));
                    DAPI_CHECK(rocblas_tbmv_batched_fn,
                               (handle_copy,
                                uplo,
                                transA,
                                diag,
                                N,
                                K,
                                dAb_copy.ptr_on_device(),
                                lda,
                                dx_copy.ptr_on_device(),
                                incx,
                                batch_count));
                    CHECK_HIP_ERROR(hx_copy.transfer_from(dx_copy));
                    unit_check_general<T>(1, N, incx, hx, hx_copy, batch_count);
                }
            }
            return;
        }

        // CPU BLAS
        cpu_time_used = get_time_us_no_sync();
        for(int64_t b = 0; b < batch_count; b++)
            ref_tbmv<T>(uplo, transA, diag, N, K, hAb[b], lda, hx_gold[b], incx);

        cpu_time_used = get_time_us_no_sync() - cpu_time_used;

        // copy output from device to CPU
        CHECK_HIP_ERROR(hx.transfer_from(dx));

        if(arg.unit_check)
        {
            unit_check_general<T>(1, N, incx, hx_gold, hx, batch_count);
        }

        if(arg.norm_check)
        {
            rocblas_error = norm_check_general<T>('F', 1, N, incx, hx_gold, hx, batch_count);
        }
    }

    if(arg.timing)
    {
        double gpu_time_used;
        int    number_cold_calls = arg.cold_iters;
        int    total_calls       = number_cold_calls + arg.iters;

        hipStream_t stream;
        CHECK_ROCBLAS_ERROR(rocblas_get_stream(handle, &stream));

        for(int iter = 0; iter < total_calls; iter++)
        {
            if(iter == number_cold_calls)
                gpu_time_used = get_time_us_sync(stream); // in microseconds

            DAPI_DISPATCH(rocblas_tbmv_batched_fn,
                          (handle,
                           uplo,
                           transA,
                           diag,
                           N,
                           K,
                           dAb.ptr_on_device(),
                           lda,
                           dx.ptr_on_device(),
                           incx,
                           batch_count));
        }

        gpu_time_used = get_time_us_sync(stream) - gpu_time_used;

        ArgumentModel<e_uplo, e_transA, e_diag, e_N, e_K, e_lda, e_incx, e_batch_count>{}
            .log_args<T>(rocblas_cout,
                         arg,
                         gpu_time_used,
                         tbmv_gflop_count<T>(N, K),
                         tbmv_gbyte_count<T>(N, K),
                         cpu_time_used,
                         rocblas_error);
    }
}
