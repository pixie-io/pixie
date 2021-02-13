#pragma once
#include <gmock/gmock.h>
#include <string>

#include <absl/container/flat_hash_set.h>
#include "src/common/system/system.h"
#include "src/shared/metadata/cgroup_metadata_reader.h"
#include "src/shared/upid/upid.h"

namespace pl {
namespace md {

class MockCGroupMetadataReader : public CGroupMetadataReader {
 public:
  MockCGroupMetadataReader() : CGroupMetadataReader(system::Config::GetInstance()) {}
  ~MockCGroupMetadataReader() override = default;

  MOCK_CONST_METHOD4(ReadPIDs,
                     Status(PodQOSClass qos_class, std::string_view pod_id,
                            std::string_view container_id, absl::flat_hash_set<uint32_t>* pid_set));
  MOCK_CONST_METHOD1(ReadPIDStartTime, int64_t(uint32_t pid));
  MOCK_CONST_METHOD1(ReadPIDCmdline, std::string(uint32_t pid));
  MOCK_CONST_METHOD1(PodDirExists, bool(const PodInfo& pod_info));
};

}  // namespace md
}  // namespace pl
