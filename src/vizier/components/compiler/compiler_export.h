#pragma once
#ifdef __cplusplus
extern "C" {
#endif

// TODO(reviewer) - got any ideas on better naming suggestions here?
// lots of overloading with the pl::carnot::compiler ns.
typedef void* CompilerPtr;
/**
 * @brief Makes a new compiler object.
 * @return CompilerState     - pointer to the compiler object.
 */
CompilerPtr CompilerNew();

/**
 * @brief Takes in the compiler object and the schema, uses this to setup the compiler and
 * compile the query into a serialized protobuf.
 *
 * @param compiler    - pointer to the compiler object.
 * @param schema      - schema represented as a protobuf.
 * @param schema_len  - length of the schema string.
 * @param query       - The query string to comple.
 * @param query_len   - The length of the query string.
 * @return char*      - the serialized logical plan after compilation.
 */
char* CompilerCompile(CompilerPtr compiler_ptr, const char* schema, int schema_len,
                      const char* query, int query_len, const char* udf_proto_c_str,
                      int udf_proto_str_len, int* resultLen);

/**
 * @brief Frees up the memory handled by the compiler.
 *
 * @param compiler_state - pointer to the compiler state
 */
void CompilerFree(CompilerPtr compiler_ptr);

/**
 * @brief Frees the memory of the string.
 *
 * @param str
 */
void CompilerStrFree(char* str);

#ifdef __cplusplus
}
#endif
