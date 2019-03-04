#pragma once

#include <arrow/api.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "src/common/common.h"
#include "src/common/type_utils.h"
#include "src/common/types/types.pb.h"
#include "src/stirling/proto/collector_config.pb.h"
#include "src/stirling/types.h"

namespace pl {
namespace stirling {

class SourceConnector;
class DataTable;
struct RawDataBuf;

using stirlingpb::Element_State;
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
class InfoClassElement : public DataElement {
 public:
  InfoClassElement() = delete;
  virtual ~InfoClassElement() = default;
  explicit InfoClassElement(const DataElement& element)
      : DataElement(element), state_(Element_State::Element_State_NOT_SUBSCRIBED) {}
  explicit InfoClassElement(const std::string& name, const DataType& type,
                            const Element_State& state)
      : DataElement(name, type), state_(state) {}

  void SetState(const Element_State& state) { state_ = state; }
  const Element_State& state() const { return state_; }

  /**
   * @brief Generate a proto message based on the InfoClassElement.
   *
   * @return stirlingpb::Element
   */
  stirlingpb::Element ToProto() const;

 private:
  Element_State state_;
};

/**
 * @brief InfoClassSchema is simply a vector of InfoClassElements.
 *
 * Each element in the vector represents a column in the schema.
 */
using InfoClassSchema = std::vector<InfoClassElement>;

/**
 * InfoClassManager consists af a collection of related InfoClassElements, that are sampled
 * together. By definition, the elements should be collected together (with a common timestamp).
 *
 * The InfoClassManager also serves as the State Manager for the entire data collector.
 *  - The Config unit uses the Schemas to publish available data to the Agent.
 *  - The Config unit changes the state of elements based on the Publish call from the Agent.
 *  - There is a 1:1 relationship with the Data Tables.
 *  - Each InfoClassManager points back to its SourceConnector.
 */
class InfoClassManager {
 public:
  InfoClassManager() = delete;
  /**
   * @brief Construct a new Info Class Manager object
   * SourceConnector constructs InfoClassManager objects with and adds Elements to it
   *
   * @param name Name of the InfoClass
   * @param source Pointer to the SourceConnector that created the InfoClassManager object.
   * This is required to identify an InfoClassManager parent source and also to generate
   * the publish proto.
   */
  explicit InfoClassManager(const std::string& name) : name_(name) {
    last_sampled_ = std::chrono::milliseconds::zero();
    last_pushed_ = std::chrono::milliseconds::zero();
    id_ = global_id_++;
  }
  virtual ~InfoClassManager() = default;

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
   * @brief Get the schema of the InfoClass.
   *
   * @return InfoClassSchema schema
   */
  InfoClassSchema& Schema() { return elements_; }

  /**
   * @brief Set the subscription state of an Element
   *
   * @param index Element to update in the InfoClassManager
   * @param state Subscription state
   * @return Status
   */
  void UpdateElementSubscription(size_t index, const Element_State& state) {
    DCHECK(index < elements_.size());
    elements_[index].SetState(state);
  }

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
   * @brief Generate a proto message based on the InfoClassManager.
   *
   * @return stirlingpb::InfoClass
   */
  stirlingpb::InfoClass ToProto() const;

  /**
   * @brief Configure sampling period.
   *
   * @param period Sampling period in ms.
   */
  void SetSamplingPeriod(std::chrono::milliseconds period) { sampling_period_ = period; }

  /**
   * @brief Configure sampling period.
   *
   * @param period Sampling period in ms.
   */
  void SetPushPeriod(std::chrono::milliseconds period) { push_period_ = period; }

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
   * @brief Samples the data from the Source and copies into local buffers.
   *
   * @return Status
   */
  Status SampleData();

  /**
   * @brief Push data by using the callback.
   *
   * @return Status.
   */
  Status PushData(PushDataCallback agent_callback);

  /**
   * @brief Notify function to update state after making changes to the schema.
   * This will make sure changes are pushed to the Source Connector and Data Tables accordingly.
   */
  void Notify() {}

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

  /**
   * @brief Convenience function to return current time in Milliseconds.
   *
   * @return milliseconds
   */
  static std::chrono::milliseconds CurrentTime() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch());
  }

  const std::string& name() const { return name_; }
  const SourceConnector* source() const { return source_; }
  uint64_t id() { return id_; }

 private:
  static std::atomic<uint64_t> global_id_;

  /**
   * Unique ID of the InfoClassManager instance. ID must never repeat, even after destruction.
   */
  uint64_t id_;

  /**
   * Name of the Info Class.
   */
  std::string name_;

  /**
   * Vector of all the elements provided by this Info Class.
   */
  InfoClassSchema elements_;

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
  uint32_t occupancy_threshold_ = kDefaultOccupancyThreshold;

  /**
   * Data push threshold, based on percentage of buffer that is filled.
   */
  uint32_t occupancy_pct_threshold_ = kDefaultOccupancyPctThreshold;

  /**
   * Statistics: count number of pushes.
   */
  uint32_t push_count_ = 0;

  static constexpr uint32_t kDefaultOccupancyThreshold = 1024;
  static constexpr uint32_t kDefaultOccupancyPctThreshold = 100;
};

}  // namespace stirling
}  // namespace pl
