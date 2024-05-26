/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "src/stirling/source_connectors/socket_tracer/protocols/mqtt/stitcher.h"

#include <algorithm>
#include <deque>
#include <string>
#include <utility>

#include "src/common/base/base.h"
#include "src/common/json/json.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mqtt/types.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mqtt {

std::map<uint8_t, uint8_t> MapRequestToResponse = {
        // CONNECT to CONNACK
        {0x10, 0x20},
        // PUBLISH QOS 0 to Dummy response
        {0x30, 0xff},
        {0x31, 0xff},
        {0x38, 0xff},
        {0x39, 0xff},
        // PUBLISH QOS 1 to PUBACK
        {0x32, 0x40},
        {0x33, 0x40},
        {0x3a, 0x40},
        {0x3b, 0x40},
        // PUBLISH QOS 2 to PUBREC
        {0x34, 0x50},
        {0x35, 0x50},
        {0x3c, 0x50},
        {0x3d, 0x50},
        // PUBREL to PUBCOMP
        {0x60, 0x70},
        // SUBSCRIBE to SUBACK
        {0X80, 0X90},
        // UNSUBSCRIBE to UNSUBACK
        {0xa0, 0xb0},
        // PINGREQ to PINGRESP
        {0xc0, 0xd0},
        // DISCONNECT to Dummy response
        {0xe0, 0xff},
        // AUTH to Dummy response
        {0xf0, 0xff}
};

RecordsWithErrorCount<Record> StitchFrames(
        absl::flat_hash_map<packet_id_t, std::deque<Message>> *req_frames,
        absl::flat_hash_map<packet_id_t, std::deque<Message>> *resp_frames) {

    std::vector<Record> entries;
    int error_count = 0;

    // iterate through all deques of requests associated with a specific streamID and find the
    // matching response
    for (auto &[packet_id, req_deque]: *req_frames) {
        // goal is to match the request to the closest appropriate response to the specific control type based on
        // timestamp

        // get the response deque corresponding to the packet ID of the request deque
        auto pos = resp_frames->find(packet_id);
        // note that not finding a corresponding response deque is not indicative of error, as in
        // case of MQTT packets that do not have responses like Publish with QOS 0
        std::deque<mqtt::Message> empty_deque;
        std::deque<mqtt::Message>& resp_deque = (pos != resp_frames->end()) ? pos->second : empty_deque;

        // track the latest response timestamp to compare against request frame's timestamp later.
        uint64_t latest_resp_ts = resp_deque.empty() ? 0 : resp_deque.back().timestamp_ns;
        // finding the closest appropriate response from response deque in terms of timestamp and type
        // for each request in the request deque
        for(mqtt::Message& req_frame : req_deque) {
            // getting the appropriate response match value for the request match key
            uint8_t request_match_key = (req_frame.control_packet_type << 4) | static_cast<uint8_t>(req_frame.dup) << 3 | (req_frame.header_fields["qos"]) << 1 | static_cast<uint8_t>(req_frame.retain);
            auto iter = MapRequestToResponse.find(request_match_key);
            if (iter == MapRequestToResponse.end()) {
                VLOG(1) << absl::Substitute("Could not find any responses for frame type = $0", request_match_key);
                continue;
            }
            if (iter->second == 0xff) {
                // Request without responses found
                req_frame.consumed = true;
                latest_resp_ts = req_frame.timestamp_ns + 1;
                mqtt::Message dummy_resp;
                entries.push_back({std::move(req_frame), std::move(dummy_resp)});
                continue;
            }
            uint8_t response_match_value = iter->second;

            // finding the first response frame with timestamp greater than request frame
            auto first_timestamp_iter = std::lower_bound(
                    resp_deque.begin(), resp_deque.end(), req_frame.timestamp_ns,
                    [](const mqtt::Message& message, const uint64_t ts){return ts > message.timestamp_ns;});
            if (first_timestamp_iter == resp_deque.end()) {
                VLOG(1) << absl::Substitute("Could not find any responses after timestamp = $0", req_frame.timestamp_ns);
                continue;
            }

            // finding the first appropriate response frame with the desired control packet type and flags
            auto response_frame_iter = std::find_if(
                    first_timestamp_iter, resp_deque.end(),
                    [response_match_value](mqtt::Message& message) {
                        return  ((message.control_packet_type << 4) | (static_cast<uint8_t>(message.dup) << 3) | (message.header_fields["qos"] << 1) | static_cast<uint8_t>(message.retain)) == response_match_value;
                    });
            if (response_frame_iter == resp_deque.end()) {
                VLOG(1) << absl::Substitute("Could not find any responses with control packet type and flag = $0", response_match_value);
                continue;
            }
            mqtt::Message& resp_frame = *response_frame_iter;

            req_frame.consumed = true;
            resp_frame.consumed = true;
            entries.push_back({std::move(req_frame), std::move(resp_frame)});
        }

        // clearing the req_deque and resp_deque
        auto erase_until_iter = req_deque.begin();
        auto iter = req_deque.begin();
        while (iter != req_deque.end() && (iter->timestamp_ns < latest_resp_ts)) {
            if (iter->consumed) {
                ++erase_until_iter;
            }
            if (!iter->consumed && !(iter == req_deque.end()-1) && ((erase_until_iter+1)->consumed)) {
                ++error_count;
                ++erase_until_iter;
            }
            ++iter;
        }
        req_deque.erase(req_deque.begin(), erase_until_iter);
    }

    // iterate through all response dequeues to find out which ones haven't been consumed
    for (auto &[packet_id, resp_deque]: *resp_frames) {
        for (auto &resp: resp_deque) {
            if (!resp.consumed) {
                error_count++;
            }
        }
        resp_deque.clear();
    }

    return {entries, error_count};
}
} // namespace mqtt
} // namespace protocols
} // namespace stirling
} // namespace px