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

#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

typedef void* PlannerPtr;

/**
 * @brief Makes a new planner object.
 * @return PlannePtr - pointer to the planner object.
 */
PlannerPtr PlannerNew(const char* udf_info_str_data, int udf_info_str_len);

/**
 * @brief Takes in the planner object and the schema, uses this to setup the planner and
 * compile the query into a serialized protobuf.
 *
 * @param planner                 Pointer to the Planner.
 * @param planner_state_str_c     The planner state proto, seralized as a string.
 * @param planner_state_str_len   Length of the planner state proto serialized string.
 * @param query_request_str_c     The query request proto to plan, seralized as a string.
 * @param query_request_str_len   The length of the query request serialized string.
 * @return char*                  The distributed plan proto, serialized as a string.
 */
char* PlannerPlan(PlannerPtr planner_ptr, const char* query, int query_len, int* resultLen);

/**
 * @brief Compiles mutations into their executable form. Takes in a serialized
 * CompileMutationsRequest and returns a serialiaed CompileMutationsResponse.
 *
 * @param planner Pointer to the Planner object
 * @param planner_state_str_c     The serialized distributedpb.LogicalPlannerState.
 * @param planner_state_str_len   Length of the serialized LogicalPlannerState.
 * @param mutation_request_str_c The serialized px.carnot.planner.plannerpb.CompileMutationsRequest
 * message.
 * @param mutation_request_str_len  The length of the serialized CompileMutationsRequest.
 * @param resultLen Variable to store the length of the return value
 * @return char* The serialized px.carnot.planner.plannerpb.CompileMutationsResponse, where the
 * length of the message is `resultLen`. If the request contains an erroneous format, the response
 * stores the error information in the status field.
 */
char* PlannerCompileMutations(PlannerPtr planner_ptr, const char* mutation_request_str_c,
                              int mutation_request_str_len, int* resultLen);

/**
 * @brief Generates an OTel export script based on the script passed in.
 *
 * The planner determines which data to export by reading all the schemas of DataFrames
 * sent through px.display.
 * @param planner Pointer to the Planner object
 * @param generate_otel_script_grequest_str_c The serialized
 * px.carnot.planner.plannerpb.GenerateOtelScriptRequest message.
 * @param generate_otel_script_grequest_str_len  The length of the serialized
 * GenerateOtelScriptRequest.
 * @param resultLen Variable to store the length of the return value
 * @return char* The serialized px.carnot.planner.plannerpb.GenerateOtelScriptResponse, where the
 * length of the message is `resultLen`. If the request contains an erroneous format, the response
 * stores the error information in the status field.
 */
char* PlannerGenerateOTelScript(PlannerPtr planner_ptr,
                                const char* generate_otel_script_request_str_c,
                                int generate_otel_script_request_str_len, int* resultLen);
/**
 * @brief Frees up the memory handled by the planner.
 *
 * @param planner_ptr - pointer to the planner ptr.
 */
void PlannerFree(PlannerPtr planner_ptr);

/**
 * @brief Frees the memory of the string.
 *
 * @param str
 */
void StrFree(char* str);

#ifdef __cplusplus
}
#endif
