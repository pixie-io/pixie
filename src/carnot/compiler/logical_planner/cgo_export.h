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
