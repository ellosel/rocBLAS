/* ************************************************************************
 * Copyright 2020-2021 Advanced Micro Devices, Inc.
 * ************************************************************************ */
#include "../../library/src/include/check_numerics_matrix.hpp"
#include "../../library/src/include/check_numerics_vector.hpp"
#include "rocblas_data.hpp"
#include "rocblas_test.hpp"
#include "rocblas_vector.hpp"
#include "type_dispatch.hpp"

namespace
{
    template <typename T>
    void expect_decimals_eq(T a, T b, int decimals)
    {
        EXPECT_EQ(std::round(a * pow(10, decimals)), std::round(b * pow(10, decimals)));
    }

    // half floats

    template <typename T>
    void testing_half_operators(const Arguments& arg)
    {

        T c(0.5);
        T s(2.0);

        T result = -(c + c) * s;
        result /= s;
        EXPECT_EQ(result, T(-1.0));

        c      = T(0.5);
        s      = T(2.0);
        result = c * s + s / c;
        EXPECT_EQ((float)result, 5.0f);

        // unique harmonic convergence
        // search half-precision-arithmetic-fp16-versus-bfloat16 harmonic
        result = T(0);
        if(std::is_same<T, rocblas_half>{})
        {
            for(int i = 1; i <= 513; i++)
                result += T(1.0) / T(i);
            expect_decimals_eq((float)result, 7.08594f, 5);
        }
        else if(std::is_same<T, rocblas_bfloat16>{})
        {
            for(int i = 1; i <= 65; i++)
                result += T(1.0) / T(i);
            expect_decimals_eq((float)result, 5.0625f, 4);
        }
    };

    // By default, arbitrary type combinations are invalid.
    // The unnamed second parameter is used for enable_if_t below.
    template <typename, typename = void>
    struct half_operators_testing : rocblas_test_invalid
    {
    };

    template <typename T>
    struct half_operators_testing<
        T,
        std::enable_if_t<std::is_same<T, rocblas_half>{} || std::is_same<T, rocblas_bfloat16>{}>>
        : rocblas_test_valid
    {
        void operator()(const Arguments& arg)
        {
            if(!strcmp(arg.function, "half_operators"))
                testing_half_operators<T>(arg);
            else
                FAIL() << "Internal error: Test called with unknown function: " << arg.function;
        }
    };

    struct half_operators : RocBLAS_Test<half_operators, half_operators_testing>
    {
        // Filter for which types apply to this suite
        static bool type_filter(const Arguments& arg)
        {
            return true;
        }

        // Filter for which functions apply to this suite
        static bool function_filter(const Arguments& arg)
        {
            return !strcmp(arg.function, "half_operators");
        }

        // Google Test name suffix based on parameters
        static std::string name_suffix(const Arguments& arg)
        {
            RocBLAS_TestName<half_operators> name(arg.name);
            name << rocblas_datatype2string(arg.a_type);
            return std::move(name);
        }
    };

    TEST_P(half_operators, auxiliary)
    {
        CATCH_SIGNALS_AND_EXCEPTIONS_AS_FAILURES(
            rocblas_simple_dispatch<half_operators_testing>(GetParam()));
    }
    INSTANTIATE_TEST_CATEGORIES(half_operators);

    //
    // complex

    template <typename T>
    void testing_complex_operators(const Arguments& arg)
    {
        using R = real_t<T>;

        T c(0.5, 0.25);
        R s(2.0);

        T result = c * s;
        EXPECT_EQ(result, T(1.0, 0.5));

        result /= s;
        EXPECT_EQ(result, c);

        T val(1.0, -2.0);
        result = (s - val) / s;
        EXPECT_EQ(result, T(0.5, 1.0));

        result = T(20.0, -4.0) / T(3.0, 2.0);
        EXPECT_EQ(result, T(4.0, -4.0));

        result = 1.0 / T(1.0, 0.0);
        EXPECT_EQ(result, T(1.0, 0.0));
    };

    // By default, arbitrary type combinations are invalid.
    // The unnamed second parameter is used for enable_if_t below.
    template <typename, typename = void>
    struct complex_operators_testing : rocblas_test_invalid
    {
    };

    template <typename T>
    struct complex_operators_testing<T,
                                     std::enable_if_t<std::is_same<T, rocblas_float_complex>{}
                                                      || std::is_same<T, rocblas_double_complex>{}>>
        : rocblas_test_valid
    {
        void operator()(const Arguments& arg)
        {
            if(!strcmp(arg.function, "complex_operators"))
                testing_complex_operators<T>(arg);
            else
                FAIL() << "Internal error: Test called with unknown function: " << arg.function;
        }
    };

    struct complex_operators : RocBLAS_Test<complex_operators, complex_operators_testing>
    {
        // Filter for which types apply to this suite
        static bool type_filter(const Arguments& arg)
        {
            return true;
        }

        // Filter for which functions apply to this suite
        static bool function_filter(const Arguments& arg)
        {
            return !strcmp(arg.function, "complex_operators");
        }

        // Google Test name suffix based on parameters
        static std::string name_suffix(const Arguments& arg)
        {
            RocBLAS_TestName<complex_operators> name(arg.name);
            name << rocblas_datatype2string(arg.a_type);
            return std::move(name);
        }
    };

    TEST_P(complex_operators, auxiliary)
    {
        CATCH_SIGNALS_AND_EXCEPTIONS_AS_FAILURES(
            rocblas_simple_dispatch<complex_operators_testing>(GetParam()));
    }
    INSTANTIATE_TEST_CATEGORIES(complex_operators);

    //Testing a vector for NaN/zero/Inf
    template <typename T>
    void testing_check_numerics_vector(const Arguments& arg)
    {
        rocblas_int    N           = arg.N;
        rocblas_int    inc_x       = arg.incx;
        rocblas_int    offset_x    = 0;
        rocblas_stride stride_x    = arg.stride_x;
        rocblas_int    batch_count = arg.batch_count;

        //Creating a rocBLAS handle
        rocblas_handle handle;
        CHECK_ROCBLAS_ERROR(rocblas_create_handle(&handle));

        //Hard-code the enum `check_numerics` to `rocblas_check_numerics_mode_fail` which will return `rocblas_status_check_numerics_fail` if the vector contains a NaN/Inf
        rocblas_check_numerics_mode check_numerics = rocblas_check_numerics_mode_fail;

        //Argument sanity check before allocating invalid memory
        if(N <= 0 || inc_x <= 0)
        {
            return;
        }

        size_t size_x = N * size_t(inc_x);

        //Allocating memory for the host vector
        host_vector<T> h_x(size_x);
        //==============================================================================================
        // Initializing random values in the vector
        //==============================================================================================
        rocblas_seedrand();
        rocblas_init<T>(h_x, 1, N, inc_x);

        // allocate memory on device
        device_vector<T> d_x(size_x);

        // copy data from CPU to device
        CHECK_HIP_ERROR(hipMemcpy(d_x, h_x, sizeof(T) * size_x, hipMemcpyHostToDevice));

        rocblas_status status          = rocblas_status_success;
        const char     function_name[] = "testing_check_numerics_vector";
        bool           is_input        = true;
        status = rocblas_internal_check_numerics_vector_template(function_name,
                                                                 handle,
                                                                 N,
                                                                 (T*)d_x,
                                                                 offset_x,
                                                                 inc_x,
                                                                 stride_x,
                                                                 1,
                                                                 check_numerics,
                                                                 is_input);
        EXPECT_EQ(status, rocblas_status_success);
        //==============================================================================================
        // Initializing and testing for zero in the vector
        //==============================================================================================
        rocblas_init_zero<T>((T*)h_x, N - 1, N);

        // copy data from CPU to device
        CHECK_HIP_ERROR(hipMemcpy(d_x, h_x, sizeof(T) * size_x, hipMemcpyHostToDevice));

        status = rocblas_internal_check_numerics_vector_template(function_name,
                                                                 handle,
                                                                 N,
                                                                 (T*)d_x,
                                                                 offset_x,
                                                                 inc_x,
                                                                 stride_x,
                                                                 1,
                                                                 check_numerics,
                                                                 is_input);
        EXPECT_EQ(status, rocblas_status_success);
        //==============================================================================================
        // Initializing and testing for Inf in the vector
        //==============================================================================================
        rocblas_seedrand();
        rocblas_init<T>(h_x, 1, N, inc_x);
        rocblas_init_inf<T>((T*)h_x, N - 3, N - 1);

        // copy data from CPU to device
        CHECK_HIP_ERROR(hipMemcpy(d_x, h_x, sizeof(T) * size_x, hipMemcpyHostToDevice));

        status = rocblas_internal_check_numerics_vector_template(function_name,
                                                                 handle,
                                                                 N,
                                                                 (T*)d_x,
                                                                 offset_x,
                                                                 inc_x,
                                                                 stride_x,
                                                                 1,
                                                                 check_numerics,
                                                                 is_input);

        EXPECT_EQ(status, rocblas_status_check_numerics_fail);
        //==============================================================================================
        // Initializing and testing for NaN in the vector
        //==============================================================================================
        rocblas_seedrand();
        rocblas_init<T>(h_x, 1, N, inc_x);
        rocblas_init_nan<T>((T*)h_x, 0, N - 3);

        // copy data from CPU to device
        CHECK_HIP_ERROR(hipMemcpy(d_x, h_x, sizeof(T) * size_x, hipMemcpyHostToDevice));
        status = rocblas_internal_check_numerics_vector_template(function_name,
                                                                 handle,
                                                                 N,
                                                                 (T*)d_x,
                                                                 offset_x,
                                                                 inc_x,
                                                                 stride_x,
                                                                 1,
                                                                 check_numerics,
                                                                 is_input);

        EXPECT_EQ(status, rocblas_status_check_numerics_fail);

        //==============================================================================================
        // Initializing random values in batched vectors
        //==============================================================================================
        //Allocate device and host batched vectors
        device_batch_vector<T> d_x_batch(N, inc_x, batch_count);
        host_batch_vector<T>   h_x_batch(N, inc_x, batch_count);

        //Initialize Data on CPU
        rocblas_seedrand();
        rocblas_init(h_x_batch, true);

        //Transferring data from host to device
        CHECK_HIP_ERROR(d_x_batch.transfer_from(h_x_batch));

        status = rocblas_internal_check_numerics_vector_template(function_name,
                                                                 handle,
                                                                 N,
                                                                 d_x_batch.const_batch_ptr(),
                                                                 offset_x,
                                                                 inc_x,
                                                                 stride_x,
                                                                 batch_count,
                                                                 check_numerics,
                                                                 is_input);

        EXPECT_EQ(status, rocblas_status_success);

        //==============================================================================================
        // Initializing and testing for zero in batched vectors
        //==============================================================================================
        //Initialize Data on CPU
        rocblas_seedrand();
        rocblas_init(h_x_batch, true);
        for(int i = 0; i < batch_count; i++)
            for(size_t j = 0; j < N; j++)
                h_x_batch[i][j * inc_x] = T(rocblas_zero_rng());

        //Transferring data from host to device
        CHECK_HIP_ERROR(d_x_batch.transfer_from(h_x_batch));

        status = rocblas_internal_check_numerics_vector_template(function_name,
                                                                 handle,
                                                                 N,
                                                                 d_x_batch.const_batch_ptr(),
                                                                 offset_x,
                                                                 inc_x,
                                                                 stride_x,
                                                                 batch_count,
                                                                 check_numerics,
                                                                 is_input);

        EXPECT_EQ(status, rocblas_status_success);

        //==============================================================================================
        // Initializing and testing for Inf in batched vectors
        //==============================================================================================
        //Initialize Data on CPU
        rocblas_seedrand();
        rocblas_init(h_x_batch, true);
        for(int i = 3; i < batch_count; i++)
            for(size_t j = 0; j < N; j++)
                h_x_batch[i][j * inc_x] = T(rocblas_inf_rng());

        //Transferring data from host to device
        CHECK_HIP_ERROR(d_x_batch.transfer_from(h_x_batch));

        status = rocblas_internal_check_numerics_vector_template(function_name,
                                                                 handle,
                                                                 N,
                                                                 d_x_batch.const_batch_ptr(),
                                                                 offset_x,
                                                                 inc_x,
                                                                 stride_x,
                                                                 batch_count,
                                                                 check_numerics,
                                                                 is_input);

        EXPECT_EQ(status, rocblas_status_check_numerics_fail);

        //==============================================================================================
        // Initializing and testing for NaN in batched vectors
        //==============================================================================================
        //Initialize Data on CPU
        rocblas_seedrand();
        rocblas_init(h_x_batch, true);
        for(int i = 4; i < batch_count; i++)
            for(size_t j = 0; j < N; j++)
                h_x_batch[i][j * inc_x] = T(rocblas_nan_rng());

        //Transferring data from host to device
        CHECK_HIP_ERROR(d_x_batch.transfer_from(h_x_batch));

        status = rocblas_internal_check_numerics_vector_template(function_name,
                                                                 handle,
                                                                 N,
                                                                 d_x_batch.const_batch_ptr(),
                                                                 offset_x,
                                                                 inc_x,
                                                                 stride_x,
                                                                 batch_count,
                                                                 check_numerics,
                                                                 is_input);

        EXPECT_EQ(status, rocblas_status_check_numerics_fail);

        CHECK_ROCBLAS_ERROR(rocblas_destroy_handle(handle));
    };

    // By default, arbitrary type combinations are invalid.
    // The unnamed second parameter is used for enable_if_t below.
    template <typename, typename = void>
    struct check_numerics_vector_testing : rocblas_test_invalid
    {
    };

    template <typename T>
    struct check_numerics_vector_testing<
        T,
        std::enable_if_t<std::is_same<T, rocblas_half>{} || std::is_same<T, rocblas_bfloat16>{}
                         || std::is_same<T, rocblas_float_complex>{}
                         || std::is_same<T, rocblas_double_complex>{} || std::is_same<T, float>{}
                         || std::is_same<T, double>{}>> : rocblas_test_valid
    {
        void operator()(const Arguments& arg)
        {
            if(!strcmp(arg.function, "check_numerics_vector"))
                testing_check_numerics_vector<T>(arg);
            else
                FAIL() << "Internal error: Test called with unknown function: " << arg.function;
        }
    };

    struct check_numerics_vector
        : RocBLAS_Test<check_numerics_vector, check_numerics_vector_testing>
    {
        // Filter for which types apply to this suite
        static bool type_filter(const Arguments& arg)
        {
            return true;
        }

        // Filter for which functions apply to this suite
        static bool function_filter(const Arguments& arg)
        {
            return !strcmp(arg.function, "check_numerics_vector");
        }

        // Google Test name suffix based on parameters
        static std::string name_suffix(const Arguments& arg)
        {
            RocBLAS_TestName<check_numerics_vector> name(arg.name);
            name << rocblas_datatype2string(arg.a_type);
            return std::move(name);
        }
    };

    TEST_P(check_numerics_vector, auxiliary)
    {
        CATCH_SIGNALS_AND_EXCEPTIONS_AS_FAILURES(
            rocblas_simple_dispatch<check_numerics_vector_testing>(GetParam()));
    }
    INSTANTIATE_TEST_CATEGORIES(check_numerics_vector);

    //Testing a matrix for NaN/zero/Inf
    template <typename T>
    void testing_check_numerics_matrix(const Arguments& arg)
    {
        rocblas_int    M           = arg.M;
        rocblas_int    N           = arg.N;
        rocblas_int    lda         = M;
        rocblas_int    offset_a    = 0;
        rocblas_stride stride_a    = arg.stride_a;
        rocblas_int    batch_count = arg.batch_count;

        //Creating a rocBLAS handle
        rocblas_handle handle;
        CHECK_ROCBLAS_ERROR(rocblas_create_handle(&handle));

        //Hard-code the enum `check_numerics` to `rocblas_check_numerics_mode_fail` which will return `rocblas_status_check_numerics_fail` if the matrix contains a NaN/Inf
        rocblas_check_numerics_mode check_numerics = rocblas_check_numerics_mode_fail;

        //Argument sanity check before allocating invalid memory
        if(!M || !N || !batch_count)
            return;

        size_t size_a = N * size_t(lda);

        //Allocating memory for the host matrix
        host_vector<T> h_A(size_a);
        //==============================================================================================
        // Initializing random values in the matrix
        //==============================================================================================
        rocblas_seedrand();
        rocblas_init<T>(h_A, M, N, lda);

        // allocate memory on device
        device_vector<T> d_A(size_a);

        // copy data from CPU to device
        CHECK_HIP_ERROR(hipMemcpy(d_A, h_A, sizeof(T) * size_a, hipMemcpyHostToDevice));

        rocblas_status status          = rocblas_status_success;
        const char     function_name[] = "testing_check_numerics_matrix";
        bool           is_input        = true;
        status = rocblas_internal_check_numerics_ge_matrix_template(function_name,
                                                                    handle,
                                                                    rocblas_operation_transpose,
                                                                    M,
                                                                    N,
                                                                    (T*)d_A,
                                                                    offset_a,
                                                                    lda,
                                                                    stride_a,
                                                                    1,
                                                                    check_numerics,
                                                                    is_input);
        EXPECT_EQ(status, rocblas_status_success);
        //==============================================================================================
        // Initializing and testing for zero in the matrix
        //==============================================================================================
        rocblas_init_zero<T>((T*)h_A, M, N, lda);

        // copy data from CPU to device
        CHECK_HIP_ERROR(hipMemcpy(d_A, h_A, sizeof(T) * size_a, hipMemcpyHostToDevice));

        status = rocblas_internal_check_numerics_ge_matrix_template(function_name,
                                                                    handle,
                                                                    rocblas_operation_transpose,
                                                                    M,
                                                                    N,
                                                                    (T*)d_A,
                                                                    offset_a,
                                                                    lda,
                                                                    stride_a,
                                                                    1,
                                                                    check_numerics,
                                                                    is_input);
        EXPECT_EQ(status, rocblas_status_success);
        //==============================================================================================
        // Initializing and testing for Inf in the matrix
        //==============================================================================================
        rocblas_seedrand();
        rocblas_init<T>((T*)h_A, M, N, lda);
        rocblas_init_inf<T>((T*)h_A, M - 1, N - 1, lda);

        // copy data from CPU to device
        CHECK_HIP_ERROR(hipMemcpy(d_A, h_A, sizeof(T) * size_a, hipMemcpyHostToDevice));

        status = rocblas_internal_check_numerics_ge_matrix_template(function_name,
                                                                    handle,
                                                                    rocblas_operation_none,
                                                                    M,
                                                                    N,
                                                                    (T*)d_A,
                                                                    offset_a,
                                                                    lda,
                                                                    stride_a,
                                                                    1,
                                                                    check_numerics,
                                                                    is_input);

        EXPECT_EQ(status, rocblas_status_check_numerics_fail);
        //==============================================================================================
        // Initializing and testing for NaN in the matrix
        //==============================================================================================
        rocblas_seedrand();
        rocblas_init<T>((T*)h_A, M, N, lda);
        rocblas_init_nan<T>((T*)h_A, M, N, lda);

        // copy data from CPU to device
        CHECK_HIP_ERROR(hipMemcpy(d_A, h_A, sizeof(T) * size_a, hipMemcpyHostToDevice));

        status = rocblas_internal_check_numerics_ge_matrix_template(function_name,
                                                                    handle,
                                                                    rocblas_operation_none,
                                                                    M,
                                                                    N,
                                                                    (T*)d_A,
                                                                    offset_a,
                                                                    lda,
                                                                    stride_a,
                                                                    1,
                                                                    check_numerics,
                                                                    is_input);

        EXPECT_EQ(status, rocblas_status_check_numerics_fail);

        //==============================================================================================
        // Initializing random values in batched matrices
        //==============================================================================================
        //Allocate device and host batched matrices
        device_batch_vector<T> d_A_batch(size_a, 1, batch_count);
        host_batch_vector<T>   h_A_batch(size_a, 1, batch_count);

        //Initialize Data on CPU
        rocblas_seedrand();
        rocblas_init(h_A_batch, true);

        //Transferring data from host to device
        CHECK_HIP_ERROR(d_A_batch.transfer_from(h_A_batch));

        status = rocblas_internal_check_numerics_ge_matrix_template(function_name,
                                                                    handle,
                                                                    rocblas_operation_none,
                                                                    M,
                                                                    N,
                                                                    d_A_batch.const_batch_ptr(),
                                                                    offset_a,
                                                                    lda,
                                                                    stride_a,
                                                                    batch_count,
                                                                    check_numerics,
                                                                    is_input);

        EXPECT_EQ(status, rocblas_status_success);

        //==============================================================================================
        // Initializing and testing for zero in batched matrices
        //==============================================================================================
        //Initialize Data on CPU
        rocblas_seedrand();
        rocblas_init(h_A_batch, true);
        for(size_t i_batch = 0; i_batch < batch_count; i_batch++)
            for(size_t i = 0; i < M; ++i)
                for(size_t j = 0; j < N; ++j)
                    h_A_batch[i_batch][i + j * lda] = T(rocblas_zero_rng());

        //Transferring data from host to device
        CHECK_HIP_ERROR(d_A_batch.transfer_from(h_A_batch));

        status = rocblas_internal_check_numerics_ge_matrix_template(function_name,
                                                                    handle,
                                                                    rocblas_operation_transpose,
                                                                    M,
                                                                    N,
                                                                    d_A_batch.const_batch_ptr(),
                                                                    offset_a,
                                                                    lda,
                                                                    stride_a,
                                                                    batch_count,
                                                                    check_numerics,
                                                                    is_input);

        EXPECT_EQ(status, rocblas_status_success);

        //==============================================================================================
        // Initializing and testing for Inf in batched matrices
        //==============================================================================================
        //Initialize Data on CPU
        rocblas_seedrand();
        rocblas_init(h_A_batch, true);
        for(size_t i_batch = 4; i_batch < batch_count; i_batch++)
            for(size_t i = 0; i < M; ++i)
                for(size_t j = 0; j < N; ++j)
                    h_A_batch[i_batch][i + j * lda] = T(rocblas_inf_rng());

        //Transferring data from host to device
        CHECK_HIP_ERROR(d_A_batch.transfer_from(h_A_batch));

        status = rocblas_internal_check_numerics_ge_matrix_template(function_name,
                                                                    handle,
                                                                    rocblas_operation_transpose,
                                                                    M,
                                                                    N,
                                                                    d_A_batch.const_batch_ptr(),
                                                                    offset_a,
                                                                    lda,
                                                                    stride_a,
                                                                    batch_count,
                                                                    check_numerics,
                                                                    is_input);

        EXPECT_EQ(status, rocblas_status_check_numerics_fail);

        //==============================================================================================
        // Initializing and testing for NaN in batched matrices
        //==============================================================================================
        //Initialize Data on CPU
        rocblas_seedrand();
        rocblas_init(h_A_batch, true);
        for(size_t i_batch = 1; i_batch < batch_count; i_batch++)
            for(size_t i = 0; i < M; ++i)
                for(size_t j = 0; j < N; ++j)
                    h_A_batch[i_batch][i + j * lda] = T(rocblas_nan_rng());

        //Transferring data from host to device
        CHECK_HIP_ERROR(d_A_batch.transfer_from(h_A_batch));

        status = rocblas_internal_check_numerics_ge_matrix_template(function_name,
                                                                    handle,
                                                                    rocblas_operation_transpose,
                                                                    M,
                                                                    N,
                                                                    d_A_batch.const_batch_ptr(),
                                                                    offset_a,
                                                                    lda,
                                                                    stride_a,
                                                                    batch_count,
                                                                    check_numerics,
                                                                    is_input);

        EXPECT_EQ(status, rocblas_status_check_numerics_fail);

        CHECK_ROCBLAS_ERROR(rocblas_destroy_handle(handle));
    };

    // By default, arbitrary type combinations are invalid.
    // The unnamed second parameter is used for enable_if_t below.
    template <typename, typename = void>
    struct check_numerics_matrix_testing : rocblas_test_invalid
    {
    };

    template <typename T>
    struct check_numerics_matrix_testing<
        T,
        std::enable_if_t<std::is_same<T, rocblas_half>{} || std::is_same<T, rocblas_bfloat16>{}
                         || std::is_same<T, rocblas_float_complex>{}
                         || std::is_same<T, rocblas_double_complex>{} || std::is_same<T, float>{}
                         || std::is_same<T, double>{}>> : rocblas_test_valid
    {
        void operator()(const Arguments& arg)
        {
            if(!strcmp(arg.function, "check_numerics_matrix"))
                testing_check_numerics_matrix<T>(arg);
            else
                FAIL() << "Internal error: Test called with unknown function: " << arg.function;
        }
    };

    struct check_numerics_matrix
        : RocBLAS_Test<check_numerics_matrix, check_numerics_matrix_testing>
    {
        // Filter for which types apply to this suite
        static bool type_filter(const Arguments& arg)
        {
            return true;
        }

        // Filter for which functions apply to this suite
        static bool function_filter(const Arguments& arg)
        {
            return !strcmp(arg.function, "check_numerics_matrix");
        }

        // Google Test name suffix based on parameters
        static std::string name_suffix(const Arguments& arg)
        {
            RocBLAS_TestName<check_numerics_matrix> name(arg.name);
            name << rocblas_datatype2string(arg.a_type);
            return std::move(name);
        }
    };

    TEST_P(check_numerics_matrix, auxiliary)
    {
        CATCH_SIGNALS_AND_EXCEPTIONS_AS_FAILURES(
            rocblas_simple_dispatch<check_numerics_matrix_testing>(GetParam()));
    }
    INSTANTIATE_TEST_CATEGORIES(check_numerics_matrix);

} // namespace
