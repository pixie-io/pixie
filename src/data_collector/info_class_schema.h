#pragma once

#include <arrow/api.h>
#include <gflags/gflags.h>
#include <glog/logging.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "src/common/status.h"
#include "src/common/types/types.pb.h"
#include "src/data_collector/proto/collector_config.pb.h"

namespace pl {
namespace datacollector {

class SourceConnector;
class DataTable;
struct RawDataBuf;

using datacollectorpb::Element_State;
using types::DataType;

/**
 * InfoClassElement is a basic structure that holds a single available data element from a source,
 * its type and a state.
 *
 * The state defines whether the data element is:
 *  (0) not collected.
 *  (1) collected but not subscribed.
 *  (2) collected and subscribed.
 */
class InfoClassElement {
 public:
  InfoClassElement() = delete;
  virtual ~InfoClassElement() = default;
  explicit InfoClassElement(const std::string& name, const DataType& type,
                            const Element_State& state)
      : name_(name), type_(type), state_(state) {}

  void SetState(const Element_State& state) { state_ = state; }

  const std::string& name() const { return name_; }
  const DataType& type() const { return type_; }
  const Element_State& state() const { return state_; }
  size_t WidthBytes() const;

  /**
   * @brief Generate a proto message based on the InfoClassElement.
   *
   * @return datacollectorpb::Element
   */
  datacollectorpb::Element ToProto() const;

 private:
  std::string name_;
  DataType type_;
  Element_State state_;
};

/**
 * InfoClassSchema consists af a collection of related InfoClassElements, that are sampled together.
 * By definition, the elements should be collected together (with a common timestamp).
 *
 * The InfoClassSchema also serves as the State Manager for the entire data collector.
 *  - The Config unit uses the Schemas to publish available data to the Agent.
 *  - The Config unit changes the state of elements based on the Publish call from the Agent.
 *  - There is a 1:1 relationship with the Data Tables.
 *  - Each InfoClassSchema points back to its SourceConnector.
 */
class InfoClassSchema {
 public:
  InfoClassSchema() = delete;
  /**
   * @brief Construct a new Info Class Schema object
   * SourceConnector constructs InfoClassSchema objects with and adds Elements to it
   *
   * @param name Name of the InfoClass
   * @param source Pointer to the SourceConnector that created the InfoClassSchema object.
   * This is required to identify an InfoClassSchema parent source and also to generate
   * the publish proto.
   */
  explicit InfoClassSchema(const std::string& name) : name_(name) {
    id_ = global_id_++;
    last_sampled_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());
    last_pushed_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());
  }
  virtual ~InfoClassSchema() = default;

  /**
   * @brief Source connector connected to this Info Class.
   *
   * @param source Pointer to source connector instance.
   */
  void SetSourceConnector(SourceConnector* source) { source_ = source; }

  /**
   * @brief Data table connected to this Info Class.
   *
   * @param Pointer to data table instance.
   */
  void SetDataTable(DataTable* data_table) { data_table_ = data_table; }

  /**
   * @brief Add an Element to the InfoClassSchema
   *
   * @param element InfoClassElement object
   */
  void AddElement(const InfoClassElement& element) { elements_.push_back(element); }

  /**
   * @brief Add an element to the InfoClassSchema.
   *
   * @param name Name of the Element
   * @param type Data type of the element
   * @param state Subscription state for the Element
   */
  Status AddElement(const std::string& name, DataType type, Element_State state);

  /**
   * @brief Get number of elements in the InfoClassSchema
   *
   * @return size_t Number of elements
   */
  uint32_t NumElements() const { return elements_.size(); }

  /**
   * @brief Set the subscription state of an Element
   *
   * @param index Element to update in the InfoClassSchema
   * @param state Subscription state
   * @return Status
   */
  void UpdateElementSubscription(size_t index, const Element_State& state) {
    DCHECK(index < elements_.size());
    elements_[index].SetState(state);
  }

  /**
   * @brief Source Connector accessor.
   *
   * @return Pointer to source connector for this InfoClassSchema.
   */
  SourceConnector* GetSourceConnector() { return source_; }

  /**
   * @brief Data Table accessor.
   *
   * @return DataTable* Pointer to the data table for this InfoClassSchema.
   */
  DataTable* GetDataTable() { return data_table_; }

  /**
   * @brief Get an Element object
   *
   * @param index
   * @return InfoClassElement
   */
  const InfoClassElement& GetElement(size_t index) const {
    DCHECK(index < elements_.size());
    return elements_[index];
  }

  /**
   * @brief Generate a proto message based on the InfoClassSchema.
   *
   * @return datacollectorpb::InfoClass
   */
  datacollectorpb::InfoClass ToProto() const;

  /**
   * @brief Configure sampling period.
   *
   * @param period Sampling period in ms.
   */
  void SetSamplingPeriod(std::chrono::milliseconds period) { sampling_period_ = period; }

  /**
   * @brief Returns true if sampling is required, for whatever reason (elapsed time, etc.).
   *
   * @return bool
   */
  bool SamplingRequired() const;

  /**
   * @brief Returns true if a data push is required, for whatever reason (elapsed time, occupancy,
   * etc.).
   *
   * @return bool
   */
  bool PushRequired() const;

  /**
   * @brief Get a pointer to collected data from the collector
   *
   * @return RawDataRecords Number of records, and raw pointer to data.
   */
  RawDataBuf GetData();

  /**
   * @brief Notify function to update state after making changes to the schema.
   * This will make sure changes are pushed to the Source Connector and Data Tables accordingly.
   */
  void Notify() {}

  const std::string& name() const { return name_; }
  const SourceConnector* source() const { return source_; }
  uint64_t id() { return id_; }

 private:
  static std::atomic<uint64_t> global_id_;

  /**
   * Unique ID of the InfoClassSchema instance. ID must never repeat, even after destruction.
   */
  uint64_t id_;

  /**
   * Name of the Info Class.
   */
  std::string name_;

  /**
   * Vector of all the elements provided by this Info Class.
   */
  std::vector<InfoClassElement> elements_;

  /**
   * Pointer back to the source connector providing the data.
   */
  SourceConnector* source_;

  /**
   * Pointer to the data table where the data is stored.
   */
  DataTable* data_table_;

  /**
   * Sampling period.
   */
  std::chrono::milliseconds sampling_period_;

  /**
   * Keep track of when the source was last sampled.
   */
  std::chrono::milliseconds last_sampled_;

  /**
   * Statistics: count number of samples.
   */
  uint32_t sampling_count_ = 0;

  /**
   * Sampling period.
   */
  std::chrono::milliseconds push_period_;

  /**
   * Keep track of when the source was last sampled.
   */
  std::chrono::milliseconds last_pushed_;

  /**
   * Data push threshold, based number of records after which a push.
   */
  uint32_t occupancy_threshold_;

  /**
   * Data push threshold, based on percentage of buffer that is filled.
   */
  uint32_t occupancy_pct_threshold_;

  /**
   * Statistics: count number of pushes.
   */
  // TODO(oazizi): Bring this back, and use it.
  // uint32_t push_count_;

  /**
   * @brief Returns the next time the source needs to be sampled, according to the sampling period.
   *
   * @return std::chrono::milliseconds
   */
  std::chrono::milliseconds NextSamplingTime() const { return last_sampled_ + sampling_period_; }

  /**
   * @brief Returns the next time the data table needs to be pushed upstream, according to the push
   * period.
   *
   * @return std::chrono::milliseconds
   */
  std::chrono::milliseconds NextPushTime() const { return last_pushed_ + push_period_; }

  // Convenience function
  static std::chrono::milliseconds CurrentTime() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());
  }
};

}  // namespace datacollector
}  // namespace pl
