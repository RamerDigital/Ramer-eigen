#ifndef EIGEN_GENERAL_MATRIX_MATRIX_METAL_H
#define EIGEN_GENERAL_MATRIX_MATRIX_METAL_H

#include "../InternalHeaderCheck.h"

extern "C" {
void eigen_metal_sgemm(
    const char* transa, const char* transb, const int* m, const int* n, const int* k,
    const float* alpha, const float* a, const int* lda,
    const float* b, const int* ldb,
    const float* beta, float* c, const int* ldc);
}

namespace Eigen {
namespace internal {

template <typename Index, int LhsStorageOrder, bool ConjugateLhs, int RhsStorageOrder, bool ConjugateRhs>
struct general_matrix_matrix_product<Index, float, LhsStorageOrder, ConjugateLhs, float, RhsStorageOrder,
                                     ConjugateRhs, ColMajor, 1> {
  typedef gebp_traits<float, float> Traits;

  static void run(Index rows, Index cols, Index depth, const float* lhs_, Index lhsStride, const float* rhs_,
                  Index rhsStride, float* res, Index /*resIncr*/, Index resStride, float alpha,
                  level3_blocking<float, float>& /*blocking*/, GemmParallelInfo<Index>* /*info = 0*/) {
    if (rows == 0 || cols == 0 || depth == 0) return;
    eigen_assert(resIncr == 1);
    char transa = (LhsStorageOrder == RowMajor) ? ((ConjugateLhs) ? 'C' : 'T') : 'N';
    char transb = (RhsStorageOrder == RowMajor) ? ((ConjugateRhs) ? 'C' : 'T') : 'N';
    int m = static_cast<int>(rows);
    int n = static_cast<int>(cols);
    int k = static_cast<int>(depth);
    int lda = static_cast<int>(lhsStride);
    int ldb = static_cast<int>(rhsStride);
    int ldc = static_cast<int>(resStride);
    float beta(1);

    eigen_metal_sgemm(&transa, &transb, &m, &n, &k, &alpha, lhs_, &lda, rhs_, &ldb, &beta, res, &ldc);
  }
};

} // namespace internal
} // namespace Eigen

#endif // EIGEN_GENERAL_MATRIX_MATRIX_METAL_H
