#pragma once

#include <memory>
#include <string>
#include "rapidjson/document.h"
#include "src/common/base/base.h"

namespace pl {
namespace stirling {
namespace test_generator_utils {

/**
 * Flatten the JSON to extract all the packets of that protocol (e.g. MySQL) into a rapidjson array.
 * Iterate through all layers because keys can be duplicate.
 * @param json_path: path to the input raw traffic capture by Tshark.
 * @param protocol: The protocol to search for and extract.
 */
std::unique_ptr<rapidjson::Value> FlattenWireSharkJSONOutput(const std::string& json_path,
                                                             const std::string& protocol);

/**
 * Read in path to JSON file and parse it to a rapidjson Document.
 * @param json_path: path of json file.
 * @param d: rapidjson document to parse the json to.
 */
void ReadJSON(const std::string& json_path, rapidjson::Document* d);

/**
 * Write a rapidjson Value to a JSON file specified by path.
 * @param d: rapidjson document whose content is written to output_path.
 */
void WriteJSON(const std::string& output_path, rapidjson::Document* d);

}  // namespace test_generator_utils
}  // namespace stirling
}  // namespace pl
