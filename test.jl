using Test, Libdl, Random
const libRmath = "src/libRmath-julia.$(Libdl.dlext)"

unsafe_store!(cglobal((:unif_rand_ptr, libRmath), Ptr{Cvoid}),
              @cfunction(rand, Float64, ()))
unsafe_store!(cglobal((:norm_rand_ptr, libRmath), Ptr{Cvoid}),
              @cfunction(randn, Float64, ()))
unsafe_store!(cglobal((:exp_rand_ptr, libRmath), Ptr{Cvoid}),
              @cfunction(randexp, Float64, ()))

@testset "ccall" begin
    @test ccall((:dbeta, libRmath), Float64, (Float64, Float64, Float64, Int32), 0.5, 0.1, 5.0, 0) ≈ 0.014267678091051986
    @test 0 <= ccall((:rbeta, libRmath), Float64, (Float64, Float64), 0.1, 5.0) <= 1.0
end

@testset "rhyper" begin
    # double rhyper(double nn1in, double nn2in, double kkin)
    Nred = 30.0
    Nblue = 40.0
    Npulled = 5.0

    hyper_samples = [
        ccall((:rhyper, libRmath), Float64, (Float64, Float64, Float64), Nred, Nblue, Npulled)
        for _ in 1:1_000_000
    ]
    expected_mean = Npulled * Nred / (Nred + Nblue)
    sample_mean = sum(hyper_samples) / length(hyper_samples)
    @test sample_mean ≈ expected_mean rtol = 0.01

    N = (Nred + Nblue)
    expected_variance = Npulled * Nred * (N - Nred) * (N - Npulled) / (N * N * (N - 1))
    sample_variance = 1 / (length(hyper_samples)) * sum((hyper_samples .- sample_mean) .^ 2)
    @test sample_variance ≈ expected_variance rtol = 0.01
end

function sample_KkC(n; N, Q)
    K = rand([1,2,3,4,5])
    k = ccall(
        (:rhyper, libRmath), Float64, (Float64, Float64, Float64),
        K, N-K, n
    )
    return k
end

@testset "fulll" begin
    function f(Q)
        objective(n) = [sample_KkC(n; N = 819_200, Q) for _ = 1:100]
        vals = [10, 100]
        objective.(vals)
    end

    Qs = [0.05, 0.055, 0.1, 0.2, 0.3]

    Threads.@threads for i in eachindex(Qs)
        f(Qs[i])
    end
end
