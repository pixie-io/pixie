#include "src/stirling/testing/test_output_generator/test_utils.h"

#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <cstring>
#include <fstream>
#include <memory>
#include "rapidjson/filewritestream.h"
#include "rapidjson/prettywriter.h"

namespace pl {
namespace stirling {
namespace test_generator_utils {

std::unique_ptr<rapidjson::Value> FlattenWireSharkJSONOutput(const std::string& json_path,
                                                             const std::string& protocol) {
  rapidjson::Document d;
  ReadJSON(json_path, &d);

  auto packets = std::make_unique<rapidjson::Value>(rapidjson::kArrayType);

  for (auto& raw_packet : d.GetArray()) {
    rapidjson::Value& layers = raw_packet["_source"]["layers"];

    for (rapidjson::Value::MemberIterator it = layers.MemberBegin(); it != layers.MemberEnd();
         ++it) {
      std::string_view name(it->name.GetString(), it->name.GetStringLength());
      if (name == protocol) {
        rapidjson::Value& v = it->value;
        packets->PushBack(v.Move(), d.GetAllocator());
      }
    }
  }
  return packets;
}

void ReadJSON(const std::string& json_path, rapidjson::Document* d) {
  std::ifstream ifs(json_path);
  rapidjson::IStreamWrapper isw(ifs);
  d->ParseStream(isw);
}

void WriteJSON(const std::string& output_path, rapidjson::Document* d) {
  std::ofstream ofs(output_path);
  rapidjson::OStreamWrapper osw(ofs);
  rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
  d->Accept(writer);
}

}  // namespace test_generator_utils
}  // namespace stirling
}  // namespace pl
