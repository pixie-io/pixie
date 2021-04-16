#include "src/carnot/exec/ml/model_pool.h"
#include <gflags/gflags.h>
#include <gtest/gtest.h>
#include "src/carnot/exec/ml/transformer_executor.h"

DEFINE_string(embedding_dir, "", "Path to embedding.proto");

namespace px {
namespace carnot {
namespace exec {
namespace ml {

TEST(ModelPool, basic) {
  auto p = ModelPool::Create();
  auto executor = p->GetModelExecutor<TransformerExecutor>(FLAGS_embedding_dir);
  EXPECT_EQ(kTransformer, executor->Type());
}

}  // namespace ml
}  // namespace exec
}  // namespace carnot
}  // namespace px
