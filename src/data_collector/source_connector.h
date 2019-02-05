#pragma once

#include <chrono>
#include <memory>
#include <string>

#include "src/common/status.h"
#include "src/data_collector/info_class_schema.h"

namespace pl {
namespace datacollector {

class InfoClassSchema;

/**
 * Abstract Base class defining a Data Source Connector.
 * Currently assumes the Data Source Connector will poll the source.
 */
class SourceConnector {
 public:
  SourceConnector() = delete;
  explicit SourceConnector(const std::string& name) : name_(name) {}
  virtual ~SourceConnector() = default;

  const std::string& name() const { return name_; }

  /**
   * Given a pointer to an InfoClassSchema, add the InfoClassElements supported by this
   * SourceConnector. The InfoClassSchema should be empty.
   */
  virtual Status PopulateSchema(InfoClassSchema* schema) = 0;

  /**
   * Main function that returns data.
   * Data is void*, because the schema is only known at run-time, and can change.
   * This data will be reorganized into Arrow tables.
   * Note: this function should be final, because it contains some time-keeping stats.
   * But can't declare it final, without also declaring it virtual, which then causes lint errors.
   * Derived classes should override GetDataCore().
   */
  void* GetData();

  /**
   * Any initialization code that may be required.
   */
  // virtual Status Init();

 private:
  const std::string name_;

  /**
   * Main function that returns a pointer to the raw data collected by the Source Connector.
   */
  virtual void* GetDataImpl() = 0;
};

/**
 * Placeholder for an EBPF Data Source Connector
 */
class EBPFConnector : public SourceConnector {
 public:
  EBPFConnector() = delete;

  /**
   * Constructor needs the BPF source code, the kernel event to attach to, and the function in the
   * BPF source that the kernel should call.
   */
  explicit EBPFConnector(const std::string& name, const std::string& bpf_program,
                         const std::string& kernel_event, const std::string& fn_name);
  virtual ~EBPFConnector();

  /**
   * Main function that returns data for this Source/InfoClass.
   */
  void* GetDataImpl() override;

  /**
   * Populate the InfoClassSchema with this this SourceConnector's available data.
   */
  Status PopulateSchema(InfoClassSchema* schema) override;

 private:
  void* data_buf_;
};

/**
 * Placeholder for an OpenTracing Data Source Connector.
 */
class OpenTracingConnector : public SourceConnector {
 public:
  explicit OpenTracingConnector(const std::string& name);
  virtual ~OpenTracingConnector();

  /**
   * Main function that returns data for this Source/InfoClass.
   */
  void* GetDataImpl() override;

  /**
   * Populate the InfoClassSchema with this this SourceConnector's available data.
   */
  Status PopulateSchema(InfoClassSchema* schema) override;

 private:
  void* data_buf_;
};

}  // namespace datacollector
}  // namespace pl
