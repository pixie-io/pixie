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
 * @brief Returns the Main Function argument's Specification. Fails if the main function doesn't
 * exist in the query argument.
 *
 * @param planner Pointer to the Planner
 * @param query_request_str_c The query request proto serialized to a string
 * @param query_request_str_len  The length of the serialized query request.
 * @param requestLen A pointer to an int that stores the length of the result.
 * @return char*
 */
char* PlannerGetMainFuncArgsSpec(PlannerPtr planner_ptr, const char* query_request_str_c,
                                 int query_request_str_len, int* resultLen);

/**
 * @brief Takes in the planner object and the script string, uses this to determine information
 * about the Vis Funcs.
 *
 * @param planner Pointer to the Planner
 * @param script_str_c The string of the script to be parsed.
 * @param script_str_len  The length of the script string.
 * @param requestLen A pointer to an int that stores the length of the result.
 * @return char* The VisFuncsInfo, serialized as a string.
 */
char* PlannerVisFuncsInfo(PlannerPtr planner_ptr, const char* script_str_c, int script_str_len,
                          int* resultLen);

/**
 * @brief Compiles mutations into their executable form. Takes in a serialized
 * CompileMutationsRequest and returns a serialiaed CompileMutationsResponse.
 *
 * @param planner Pointer to the Planner object
 * @param planner_state_str_c     The serialized distributedpb.LogicalPlannerState.
 * @param planner_state_str_len   Length of the serialized LogicalPlannerState.
 * @param mutation_request_str_c The serialized pl.carnot.planner.plannerpb.CompileMutationsRequest
 * message.
 * @param mutation_request_str_len  The length of the serialized CompileMutationsRequest.
 * @param resultLen Variable to store the length of the return value
 * @return char* The serialized pl.carnot.planner.plannerpb.CompileMutationsResponse, where the
 * length of the message is `resultLen`. If the request contains an erroneous format, the response
 * stores the error information in the Message status field.
 */
char* PlannerCompileMutations(PlannerPtr planner_ptr, const char* planner_state_str_c,
                              int planner_state_str_len, const char* mutation_request_str_c,
                              int mutation_request_str_len, int* resultLen);
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
