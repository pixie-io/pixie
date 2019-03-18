#include <gtest/gtest.h>
#include <ctime>
#include <functional>
#include <iomanip>

#include "absl/strings/str_format.h"
#include "src/common/common.h"
#include "src/common/status.h"
#include "src/stirling/bpftrace_connector.h"
#include "src/stirling/info_class_manager.h"
#include "src/stirling/seq_gen_connector.h"
#include "src/stirling/sequence_generator.h"
#include "src/stirling/source_registry.h"
#include "src/stirling/stirling.h"
#include "src/stirling/types.h"

#include "src/stirling/proto/collector_config.pb.h"

using PubProto = pl::stirling::stirlingpb::Publish;
using SubProto = pl::stirling::stirlingpb::Subscribe;

using pl::stirling::ColumnWrapperRecordBatch;
using pl::stirling::DataElements;
using pl::stirling::SeqGenConnector;
using pl::stirling::SourceRegistry;
using pl::stirling::Stirling;

using pl::types::Float64ValueColumnWrapper;
using pl::types::Int64ValueColumnWrapper;
using pl::types::SharedColumnWrapper;
using pl::types::StringValueColumnWrapper;
using pl::types::Time64NSValueColumnWrapper;

// Test arguments, from the command line
DEFINE_uint64(kRNGSeed, gflags::Uint64FromEnv("seed", 377), "Random Seed");
DEFINE_uint64(kNumTables, gflags::Uint64FromEnv("num_tables", 2), "Number of sources");
DEFINE_uint64(kNumIterMin, gflags::Uint64FromEnv("num_iter_min", 10), "Min number of iterations");
DEFINE_uint64(kNumIterMax, gflags::Uint64FromEnv("num_iter_max", 20), "Max number of iterations");
DEFINE_uint64(kNumProcessedRequirement, gflags::Uint64FromEnv("num_processed_required", 5000),
              "Number of records required to be processed before test is allowed to end");

// This is the duration for which a subscription will be valid.
constexpr std::chrono::milliseconds kDurationPerIter{500};

// Fraction of times a subscription will subscribe to a source.
const double kSubscribeProb = 0.7;

// If test typically takes too long, you may want to reduce kNumIterMin or kNumProcessedRequirement.
// Note that kNumIterMax * kDurationPerIter defines the maximum time the test can take.

class StirlingTest : public ::testing::Test {
 private:
  std::unique_ptr<Stirling> stirling_;
  std::vector<const DataElements*> schemas;
  PubProto publish_proto_;

  // Reference model (checkers).
  std::vector<pl::stirling::LinearSequence<int64_t>> lin_seq_checker_;
  std::vector<pl::stirling::ModuloSequence<int64_t>> mod10_seq_checker_;
  std::vector<pl::stirling::QuadraticSequence<int64_t>> square_seq_checker_;
  std::vector<pl::stirling::LinearSequence<double>> pi_seq_checker_;
  std::vector<pl::stirling::FibonacciSequence<int64_t>> fib_seq_checker_;

  std::vector<uint64_t> num_processed_per_table_;
  uint64_t num_processed_;

  // Random distributions for test parameters.
  std::default_random_engine rng;
  std::uniform_int_distribution<uint32_t> sampling_period_millis_dist_;
  std::uniform_int_distribution<uint32_t> push_period_millis_dist_;
  std::uniform_real_distribution<double> uniform_probability_dist_;

 public:
  inline static const uint64_t& kRNGSeed = FLAGS_kRNGSeed;
  inline static const uint64_t& kNumTables = FLAGS_kNumTables;
  inline static const uint64_t& kNumIterMin = FLAGS_kNumIterMin;
  inline static const uint64_t& kNumIterMax = FLAGS_kNumIterMax;
  inline static const uint64_t& kNumProcessedRequirement = FLAGS_kNumProcessedRequirement;

  StirlingTest()
      : rng(kRNGSeed),
        sampling_period_millis_dist_(0, 10),
        push_period_millis_dist_(0, 100),
        uniform_probability_dist_(0, 1.0) {
    for (uint32_t i = 0; i < kNumTables; ++i) {
      schemas.emplace_back(&SeqGenConnector::kElements);
    }
  }

  void SetUp() override {
    // Make registry with a number of SeqGenConnectors.
    std::unique_ptr<SourceRegistry> registry = std::make_unique<SourceRegistry>();
    for (uint32_t i = 0; i < kNumTables; ++i) {
      registry->RegisterOrDie<SeqGenConnector>(absl::StrFormat("sequences%u", i));
    }

    // Make Stirling.
    stirling_ = Stirling::Create(std::move(registry));

    // Initialize Stirling (brings-up all source connectors).
    PL_CHECK_OK(stirling_->Init());

    // Set a dummy callback function (normally this would be in the agent).
    stirling_->RegisterCallback(
        std::bind(&StirlingTest::AppendData, this, std::placeholders::_1, std::placeholders::_2));

    stirling_->GetPublishProto(&publish_proto_);

    for (uint32_t i = 0; i < kNumTables; ++i) {
      lin_seq_checker_.emplace_back(1, 1);
      mod10_seq_checker_.emplace_back(10);
      square_seq_checker_.emplace_back(1, 0, 0);
      pi_seq_checker_.emplace_back(3.14159, 0);
      fib_seq_checker_.emplace_back();

      num_processed_per_table_.push_back(0);
      num_processed_ = 0;
    }
  }

  void TearDown() override {
    for (uint32_t itable = 0; itable < kNumTables; ++itable) {
      LOG(INFO) << absl::StrFormat("Number of records processed: %u",
                                   num_processed_per_table_[itable]);
    }
  }

  Stirling* GetStirling() { return stirling_.get(); }

  SubProto GenerateRandomSubscription(const PubProto& publish_proto) {
    SubProto subscribe_proto;

    for (int i = 0; i < publish_proto.published_info_classes_size(); ++i) {
      auto sub_info_class = subscribe_proto.add_subscribed_info_classes();
      sub_info_class->MergeFrom(publish_proto.published_info_classes(i));

      sub_info_class->set_subscribed(uniform_probability_dist_(rng) < kSubscribeProb);
      sub_info_class->set_sampling_period_millis(sampling_period_millis_dist_(rng));
      sub_info_class->set_push_period_millis(push_period_millis_dist_(rng));
    }
    return subscribe_proto;
  }

  SubProto GenerateRandomSubscription() { return GenerateRandomSubscription(publish_proto_); }

  void AppendData(uint64_t table_id, std::unique_ptr<ColumnWrapperRecordBatch> record_batch) {
    // Note: Implicit assumption (not checked here) is that all columns have the same size
    uint64_t num_records = (*record_batch)[0]->Size();

    CheckRecordBatch(table_id, num_records, *record_batch);
  }

  void CheckRecordBatch(uint32_t table_id, uint64_t num_records,
                        const ColumnWrapperRecordBatch& record_batch) {
    auto table_schema = *(schemas[table_id]);

    for (uint32_t i = 0; i < num_records; ++i) {
      uint32_t j = 0;
      for (SharedColumnWrapper col : record_batch) {
        // TODO(oazizi): Switch is statically connected to the SeqGenConnector schema.
        // Find a less brittle way.
        switch (j) {
          case 0: {
            auto typedCol = std::static_pointer_cast<Time64NSValueColumnWrapper>(col);
            auto ns_count = (*typedCol)[i].val;
            PL_UNUSED(ns_count);
          } break;
          case 1: {
            auto typedCol = std::static_pointer_cast<Int64ValueColumnWrapper>(col);
            int64_t val = (*typedCol)[i].val;
            EXPECT_EQ(lin_seq_checker_[table_id](), val);
          } break;
          case 2: {
            auto typedCol = std::static_pointer_cast<Int64ValueColumnWrapper>(col);
            int64_t val = (*typedCol)[i].val;
            EXPECT_EQ(mod10_seq_checker_[table_id](), val);
          } break;
          case 3: {
            auto typedCol = std::static_pointer_cast<Int64ValueColumnWrapper>(col);
            int64_t val = (*typedCol)[i].val;
            EXPECT_EQ(square_seq_checker_[table_id](), val);
          } break;
          case 4: {
            auto typedCol = std::static_pointer_cast<Int64ValueColumnWrapper>(col);
            int64_t val = (*typedCol)[i].val;
            EXPECT_EQ(fib_seq_checker_[table_id](), val);
          } break;
          case 5: {
            auto typedCol = std::static_pointer_cast<Float64ValueColumnWrapper>(col);
            double val = (*typedCol)[i].val;
            EXPECT_EQ(pi_seq_checker_[table_id](), val);
          } break;
          default:
            CHECK(false) << absl::StrFormat("Unrecognized type: $%s",
                                            ToString(table_schema[j].type()));
        }

        j++;
      }
    }

    num_processed_per_table_[table_id] += num_records;
    num_processed_ += num_records;
  }

  uint64_t NumProcessed() { return num_processed_; }
};

// Stress/regression test that hammers Stirling with sequences.
// A reference model checks the sequences are correct on the callback.
TEST_F(StirlingTest, hammer_time_on_stirling) {
  Stirling* stirling = GetStirling();

  uint32_t i = 0;
  Status s;
  while (NumProcessed() < kNumProcessedRequirement || i < kNumIterMin) {
    // Process a subscription message.
    s = stirling->SetSubscription(GenerateRandomSubscription());
    ASSERT_TRUE(s.ok());

    // Run Stirling data collector.
    s = stirling->RunAsThread();
    ASSERT_TRUE(s.ok());

    // Stay in this config for the specified amount of time..
    std::this_thread::sleep_for(kDurationPerIter);

    stirling->Stop();
    stirling->WaitForThreadJoin();

    i++;

    // In case we have a slow environment, break out of the test after some time.
    if (i > kNumIterMax) {
      break;
    }
  }

  EXPECT_GT(NumProcessed(), 0);
}
