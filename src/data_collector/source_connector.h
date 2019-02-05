#pragma once

#include <string>

namespace pl {
namespace datacollector {

class SourceConnector {
 public:
  explicit SourceConnector(const std::string& name) : name_(name) {}
  virtual ~SourceConnector() = default;

  const std::string& name() const { return name_; }

 private:
  const std::string name_;
};

}  // namespace datacollector
}  // namespace pl
