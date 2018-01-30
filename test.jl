using Base.Test
if VERSION >= v"0.7.0-DEV.3382"
    import Libdl
end
const libRmath = "src/libRmath-julia.$(Libdl.dlext)"

unsafe_store!(cglobal((:unif_rand_ptr,libRmath),Ptr{Void}),
              cfunction(rand,Float64,()))
unsafe_store!(cglobal((:norm_rand_ptr,libRmath),Ptr{Void}),
              cfunction(randn,Float64,()))
unsafe_store!(cglobal((:exp_rand_ptr,libRmath),Ptr{Void}),
              cfunction(randexp,Float64,()))

@testset "ccall" begin
    @test ccall((:dbeta,libRmath), Float64, (Float64,Float64,Float64,Int32), 0.5, 0.1, 5.0, 0) ≈ 0.014267678091051986
    @test 0 <= ccall((:rbeta,libRmath), Float64, (Float64,Float64), 0.1, 5.0) <= 1.0
end
