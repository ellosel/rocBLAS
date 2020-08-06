/* ************************************************************************
 * Copyright 2018-2020 Advanced Micro Devices, Inc.
 *
 * ************************************************************************ */

#include "cblas_interface.hpp"
#include "flops.hpp"
#include "near.hpp"
#include "norm.hpp"
#include "rocblas.hpp"
#include "rocblas_datatype2string.hpp"
#include "rocblas_init.hpp"
#include "rocblas_math.hpp"
#include "rocblas_random.hpp"
#include "rocblas_test.hpp"
#include "rocblas_vector.hpp"
#include "unit.hpp"
#include "utility.hpp"

template <typename T>
void testing_trmv_bad_arg(const Arguments& arg)
{
    const bool FORTRAN         = arg.fortran;
    auto       rocblas_trmv_fn = FORTRAN ? rocblas_trmv<T, true> : rocblas_trmv<T, false>;

    const rocblas_int       M      = 100;
    const rocblas_int       lda    = 100;
    const rocblas_int       incx   = 1;
    const rocblas_operation transA = rocblas_operation_none;
    const rocblas_fill      uplo   = rocblas_fill_lower;
    const rocblas_diagonal  diag   = rocblas_diagonal_non_unit;

    rocblas_local_handle handle(arg.atomics_mode);

    size_t size_A = lda * size_t(M);
    size_t size_x = M * size_t(incx);

    host_vector<T> hA(size_A);
    CHECK_HIP_ERROR(hA.memcheck());
    host_vector<T> hx(size_x);
    CHECK_HIP_ERROR(hx.memcheck());
    device_vector<T> dA(size_A);
    CHECK_DEVICE_ALLOCATION(dA.memcheck());
    device_vector<T> dx(size_x);
    CHECK_DEVICE_ALLOCATION(dx.memcheck());

    //
    // Checks.
    //
    EXPECT_ROCBLAS_STATUS(rocblas_trmv_fn(handle, uplo, transA, diag, M, nullptr, lda, dx, incx),
                          rocblas_status_invalid_pointer);

    EXPECT_ROCBLAS_STATUS(rocblas_trmv_fn(handle, uplo, transA, diag, M, dA, lda, nullptr, incx),
                          rocblas_status_invalid_pointer);

    EXPECT_ROCBLAS_STATUS(rocblas_trmv_fn(nullptr, uplo, transA, diag, M, dA, lda, dx, incx),
                          rocblas_status_invalid_handle);
}

template <typename T>
void testing_trmv(const Arguments& arg)
{
    const bool FORTRAN         = arg.fortran;
    auto       rocblas_trmv_fn = FORTRAN ? rocblas_trmv<T, true> : rocblas_trmv<T, false>;

    rocblas_int M = arg.M, lda = arg.lda, incx = arg.incx;

    char char_uplo = arg.uplo, char_transA = arg.transA, char_diag = arg.diag;

    rocblas_fill         uplo   = char2rocblas_fill(char_uplo);
    rocblas_operation    transA = char2rocblas_operation(char_transA);
    rocblas_diagonal     diag   = char2rocblas_diagonal(char_diag);
    rocblas_local_handle handle(arg.atomics_mode);

    bool invalid_size = M < 0 || lda < M || lda < 1 || !incx;
    if(invalid_size || !M)
    {
        EXPECT_ROCBLAS_STATUS(
            rocblas_trmv_fn(handle, uplo, transA, diag, M, nullptr, lda, nullptr, incx),
            invalid_size ? rocblas_status_invalid_size : rocblas_status_success);

        return;
    }

    size_t size_A = lda * size_t(M);
    size_t size_x, dim_x, abs_incx;
    dim_x = M;

    abs_incx = incx >= 0 ? incx : -incx;
    size_x   = dim_x * abs_incx;

    host_vector<T> hA(size_A);
    CHECK_HIP_ERROR(hA.memcheck());
    host_vector<T> hx(size_x);
    CHECK_HIP_ERROR(hx.memcheck());
    device_vector<T> dA(size_A);
    CHECK_DEVICE_ALLOCATION(dA.memcheck());
    device_vector<T> dx(size_x);
    CHECK_DEVICE_ALLOCATION(dx.memcheck());
    host_vector<T> hres(size_x);
    CHECK_HIP_ERROR(hres.memcheck());

    //
    // Initialize.
    //
    rocblas_init(hA, true);
    rocblas_init(hx, false);

    //
    // Transfer.
    //
    CHECK_HIP_ERROR(dA.transfer_from(hA));
    CHECK_HIP_ERROR(dx.transfer_from(hx));

    double gpu_time_used, cpu_time_used;
    double rocblas_gflops, cblas_gflops, rocblas_bandwidth;
    double rocblas_error;

    /* =====================================================================
     ROCBLAS
     =================================================================== */
    if(arg.unit_check || arg.norm_check)
    {

        //
        // ROCBLAS
        //
        CHECK_ROCBLAS_ERROR(rocblas_trmv_fn(handle, uplo, transA, diag, M, dA, lda, dx, incx));
        CHECK_HIP_ERROR(hres.transfer_from(dx));

        //
        // CPU BLAS
        //
        {
            cpu_time_used = get_time_us_no_sync();
            cblas_trmv<T>(uplo, transA, diag, M, hA, lda, hx, incx);
            cpu_time_used = get_time_us_no_sync() - cpu_time_used;
            cblas_gflops  = trmv_gflop_count<T>(M) / cpu_time_used * 1e6;
        }

        //
        // Unit check.
        //
        if(arg.unit_check)
        {
            unit_check_general<T>(1, dim_x, abs_incx, hx, hres);
        }

        //
        // Norm check.
        //
        if(arg.norm_check)
        {
            rocblas_error = norm_check_general<T>('F', 1, dim_x, abs_incx, hx, hres);
        }
    }

    if(arg.timing)
    {
        //
        // Warmup
        //
        {
            int number_cold_calls = arg.cold_iters;
            for(int iter = 0; iter < number_cold_calls; iter++)
            {
                rocblas_trmv_fn(handle, uplo, transA, diag, M, dA, lda, dx, incx);
            }
        }

        //
        // Go !
        //
        {
            hipStream_t stream;
            CHECK_ROCBLAS_ERROR(rocblas_get_stream(handle, &stream));
            gpu_time_used        = get_time_us_sync(stream); // in microseconds
            int number_hot_calls = arg.iters;
            for(int iter = 0; iter < number_hot_calls; iter++)
            {
                rocblas_trmv_fn(handle, uplo, transA, diag, M, dA, lda, dx, incx);
            }
            gpu_time_used = (get_time_us_sync(stream) - gpu_time_used) / number_hot_calls;
        }

        //
        // Evaluate performance.
        //
        rocblas_gflops    = trmv_gflop_count<T>(M) / gpu_time_used * 1e6;
        rocblas_bandwidth = (double((M * (M + 1)) / 2 + 2 * M) * sizeof(T)) / gpu_time_used * 1e-3;

        //
        // Display.
        //
        rocblas_cout << "M,lda,incx,uplo,transA,diag,rocblas-Gflops,rocblas-GB/s,";
        if(arg.norm_check)
        {
            rocblas_cout << "CPU-Gflops,norm_error";
        }
        rocblas_cout << std::endl;
        rocblas_cout << M << "," << lda << "," << incx << "," << incx << "," << incx << "," << incx
                     << "," << char_uplo << ',' << char_transA << ',' << char_diag << ','
                     << rocblas_gflops << "," << rocblas_bandwidth << ",";

        if(arg.norm_check)
        {
            rocblas_cout << cblas_gflops << ',';
            rocblas_cout << rocblas_error;
        }

        rocblas_cout << std::endl;
    }
}
