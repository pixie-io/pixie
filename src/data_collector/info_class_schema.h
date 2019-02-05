#pragma once

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <atomic>
#include <string>
#include <vector>

#include "src/common/status.h"
#include "src/common/types/types.pb.h"
#include "src/data_collector/proto/collector_config.pb.h"
#include "src/data_collector/source_connector.h"

namespace pl {
namespace datacollector {

using datacollectorpb::Element_State;
using types::DataType;

class InfoClassElement {
 public:
  InfoClassElement(const std::string& name, const DataType& type, const Element_State& state)
      : name_(name), data_type_(type), state_(state) {}
  virtual ~InfoClassElement() = default;

  /**
   * @brief Generate a proto message based on the InfoClassElement.
   *
   * @return datacollectorpb::Element
   */
  datacollectorpb::Element ToProto() const;
  void SetState(const Element_State& state) { state_ = state; }
  const std::string& name() const { return name_; }
  const DataType& data_type() const { return data_type_; }
  const Element_State& state() const { return state_; }

 private:
  std::string name_;
  DataType data_type_;
  Element_State state_;
};

class InfoClassSchema {
 public:
  /**
   * @brief Construct a new Info Class Schema object
   * SourceConnector constructs InfoClassSchema objects with and adds Elements to it
   *
   * @param name Name of the InfoClass
   * @param source Pointer to the SourceConnector that created the InfoClassSchema object.
   * This is required to identify an InfoClassSchema parent source and also to generate
   * the publish proto.
   */
  InfoClassSchema(const std::string& name, const SourceConnector& source)
      : name_(name), source_(source) {
    id_ = global_id_++;
  }
  virtual ~InfoClassSchema() = default;

  /**
   * @brief Add an Element to the InfoClassSchema
   *
   * @param name Name of the Element
   * @param type Data type of the element
   * @param state Subscription state for the Element
   */
  void AddElement(const InfoClassElement& element) { elements_.push_back(element); }

  /**
   * @brief Get number of elements in the InfoClassSchema
   *
   * @return size_t Number of elements
   */
  size_t NumElements() const { return elements_.size(); }

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

  const std::string& name() const { return name_; }
  const SourceConnector& source() const { return source_; }

  uint64_t id() { return id_; }

 private:
  static std::atomic<uint64_t> global_id_;
  std::vector<InfoClassElement> elements_;
  std::string name_;
  const SourceConnector& source_;
  uint64_t id_;
};

}  // namespace datacollector
}  // namespace pl
