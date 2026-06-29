#import <Metal/Metal.h>
#import <MetalPerformanceShaders/MetalPerformanceShaders.h>
#include <algorithm>

static id<MTLDevice> g_device = nil;
static id<MTLCommandQueue> g_queue = nil;

static void init_metal() {
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    g_device = MTLCreateSystemDefaultDevice();
    if (g_device) {
      g_queue = [g_device newCommandQueue];
    }
  });
}

extern "C" {
void eigen_metal_sgemm(
    const char* transa, const char* transb, const int* m, const int* n, const int* k,
    const float* alpha, const float* a, const int* lda,
    const float* b, const int* ldb,
    const float* beta, float* c, const int* ldc) 
{
  init_metal();
  if (!g_device || !g_queue) {
    // Fallback: if Metal is not supported, we do nothing or could assert.
    // In actual usage, fallback to CPU should be handled at C++ level,
    // but here we just return or print.
    return;
  }

  int M = *m;
  int N = *n;
  int K = *k;
  int lda_val = *lda;
  int ldb_val = *ldb;
  int ldc_val = *ldc;
  float alpha_val = *alpha;
  float beta_val = *beta;

  // Swapping A and B to handle ColMajor matrices using RowMajor MPS layout:
  // C^T = B^T * A^T
  BOOL transposeLeft = (*transb == 'T' || *transb == 'C');
  BOOL transposeRight = (*transa == 'T' || *transa == 'C');

  // Compute memory sizes required to cover the matrices in memory:
  // ColMajor memory represented as RowMajor:
  // Matrix A: size is M x K ColMajor => K x M RowMajor
  NSUInteger sizeA = std::max(M, K) * lda_val * sizeof(float);
  NSUInteger sizeB = std::max(K, N) * ldb_val * sizeof(float);
  NSUInteger sizeC = std::max(M, N) * ldc_val * sizeof(float);

  @autoreleasepool {
    // Wrap CPU pointers into UMA zero-copy MTLBuffers
    id<MTLBuffer> bufferA = [g_device newBufferWithBytesNoCopy:(void*)a
                                                        length:sizeA
                                                       options:MTLResourceStorageModeShared
                                                   deallocator:nil];

    id<MTLBuffer> bufferB = [g_device newBufferWithBytesNoCopy:(void*)b
                                                        length:sizeB
                                                       options:MTLResourceStorageModeShared
                                                   deallocator:nil];

    id<MTLBuffer> bufferC = [g_device newBufferWithBytesNoCopy:(void*)c
                                                        length:sizeC
                                                       options:MTLResourceStorageModeShared
                                                   deallocator:nil];

    if (!bufferA || !bufferB || !bufferC) {
      return;
    }

    // Set descriptors for the RowMajor swapped operation
    MPSMatrixDescriptor *descA = [MPSMatrixDescriptor
        matrixDescriptorWithRows:(transposeRight ? M : K)
                         columns:(transposeRight ? K : M)
                        rowBytes:lda_val * sizeof(float)
                        dataType:MPSDataTypeFloat32];

    MPSMatrixDescriptor *descB = [MPSMatrixDescriptor
        matrixDescriptorWithRows:(transposeLeft ? K : N)
                         columns:(transposeLeft ? N : K)
                        rowBytes:ldb_val * sizeof(float)
                        dataType:MPSDataTypeFloat32];

    MPSMatrixDescriptor *descC = [MPSMatrixDescriptor
        matrixDescriptorWithRows:N
                         columns:M
                        rowBytes:ldc_val * sizeof(float)
                        dataType:MPSDataTypeFloat32];

    MPSMatrix *matrixA = [[MPSMatrix alloc] initWithBuffer:bufferA descriptor:descA];
    MPSMatrix *matrixB = [[MPSMatrix alloc] initWithBuffer:bufferB descriptor:descB];
    MPSMatrix *matrixC = [[MPSMatrix alloc] initWithBuffer:bufferC descriptor:descC];

    // Create the MPSMatrixMultiplication kernel
    MPSMatrixMultiplication *mul = [[MPSMatrixMultiplication alloc]
        initWithDevice:g_device
         transposeLeft:transposeLeft
        transposeRight:transposeRight
            resultRows:N
         resultColumns:M
       interiorColumns:K
                 alpha:(double)alpha_val
                  beta:(double)beta_val];

    if (!mul) {
      return;
    }

    // Create command buffer, encode and submit
    id<MTLCommandBuffer> commandBuffer = [g_queue commandBuffer];
    [mul encodeToCommandBuffer:commandBuffer leftMatrix:matrixB rightMatrix:matrixA resultMatrix:matrixC];
    [commandBuffer commit];
    [commandBuffer waitUntilCompleted];
  }
}
}
