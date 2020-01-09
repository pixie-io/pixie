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
 * @param planner    - pointer to the planner object.
 * @param planner_state_str_c      - the planner state represented as a protobuf
 * @param planner_state_str_len  - length of the rel_map string
 * @param query       - The query string to comple.
 * @param query_len   - The length of the query string.
 * @return char*      - the serialized plan after compilation and planning.
 */
char* PlannerPlan(PlannerPtr planner_ptr, const char* planner_state_str_c,
                  int planner_state_str_len, const char* query, int query_len, int* resultLen);

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
