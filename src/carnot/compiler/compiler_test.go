package compiler_test

import (
	"fmt"
	"log"
	"os"
	"strings"
	"testing"

	"github.com/gogo/protobuf/proto"
	"github.com/stretchr/testify/assert"
	"pixielabs.ai/pixielabs/src/carnot/compiler"
	pb "pixielabs.ai/pixielabs/src/carnot/compiler/compilerpb"
	"pixielabs.ai/pixielabs/src/carnot/planpb"
	statuspb "pixielabs.ai/pixielabs/src/common/base/proto"
)

// TestCompiler_Simple makes sure that we can actually pass in all the info needed
// to create a CompilerState and can successfully compile to an expected result.
func TestCompiler_Simple(t *testing.T) {
	expectedPlan := `dag: {
		nodes: {
			id: 1
		}
	}
	nodes: {
		id: 1
		dag: {
			nodes: {
				sorted_deps: 7
			}
			nodes: {
				id: 7
				sorted_deps: 13
			}
			nodes: {
				id: 13
			}
		}
		nodes: {
			op: {
				op_type: MEMORY_SOURCE_OPERATOR
				mem_source_op: {
					name: "perf_and_http"
					column_idxs: 0
					column_idxs: 1
					column_idxs: 2
					column_idxs: 3
					column_names: "_time"
					column_names: "cpu_cycles"
					column_names: "tlb_misses"
					column_names: "http"
					column_types: TIME64NS
					column_types: INT64
					column_types: INT64
					column_types: INT64
				}
			}
		}
		nodes: {
			id: 7
			op: {
				op_type: MAP_OPERATOR
				map_op: {
					expressions: {
						column: {
							index: 3
						}
					}
					expressions: {
						func: {
							name: "pl.divide"
							args: {
								column: {
									index: 1
								}
							}
							args: {
								column: {
									index: 2
								}
							}
						}
					}
					column_names: "http_code"
					column_names: "cpu_tlb_ratio"
				}
			}
		}
		nodes: {
			id: 13
			op: {
				op_type: MEMORY_SINK_OPERATOR
				mem_sink_op: {
					name: "out"
					column_types: INT64
					column_types: INT64
					column_names: "http_code"
					column_names: "cpu_tlb_ratio"
				}
			}
		}
	}`

	expectedPlanPB := &planpb.Plan{}
	if err := proto.UnmarshalText(expectedPlan, expectedPlanPB); err != nil {
		fmt.Println(err)
	}

	// Setup the schema from a proto.
	relProto := `columns {
		column_name: "_time"
		column_type: TIME64NS
	}
	columns {
		column_name: "cpu_cycles"
		column_type: INT64 
	}
	columns {
		column_name: "tlb_misses"
		column_type: INT64 
	}
	columns {
		column_name: "http"
		column_type: INT64
	}`
	tableName := "perf_and_http"

	// Create the compiler.
	c := compiler.New()
	defer c.Free()
	// Pass the relation proto, table and query to the compilation.
	queryLines := []string{
		"queryDF = From(table='perf_and_http', select=['_time', 'cpu_cycles', 'tlb_misses', 'http'])",
		"mapDF = queryDF.Map(fn=lambda r : {'http_code' : r.http, 'cpu_tlb_ratio' : r.cpu_cycles/r.tlb_misses})",
		"mapDF.Result(name='out')",
	}
	query := strings.Join(queryLines, "\n")
	compilerResultPB, err := c.Compile(relProto, tableName, query)
	if err != nil {
		log.Fatalln("Failed to compile:", err)
		os.Exit(1)
	}
	status := compilerResultPB.Status
	assert.Equal(t, status.ErrCode, statuspb.OK)

	compilerPlanPB := compilerResultPB.LogicalPlan
	assert.True(t, proto.Equal(expectedPlanPB, compilerPlanPB))
}

func TestCompiler_MissingTable(t *testing.T) {
	// Setup the schema from a proto.
	relProto := `columns {
		column_name: "_time"
		column_type: TIME64NS
	}
	columns {
		column_name: "cpu_cycles"
		column_type: INT64 
	}
	columns {
		column_name: "tlb_misses"
		column_type: INT64 
	}
	columns {
		column_name: "http"
		column_type: INT64
	}`
	tableName := "perf_and_http"

	// Create the compiler.
	c := compiler.New()
	defer c.Free()
	// Pass the relation proto, table and query to the compilation.
	queryLines := []string{
		"queryDF = From(table='not_perf_and_http', select=['_time', 'cpu_cycles', 'tlb_misses', 'http'])",
		"mapDF = queryDF.Map(fn=lambda r : {'http_code' : r.http, 'cpu_tlb_ratio' : r.cpu_cycles/r.tlb_misses})",
		"mapDF.Result(name='out')",
	}
	query := strings.Join(queryLines, "\n")
	compilerResultPB, err := c.Compile(relProto, tableName, query)
	if err != nil {
		t.Fatal("Failed to compiler:", err)
	}
	status := compilerResultPB.Status

	assert.NotEqual(t, status.ErrCode, statuspb.OK)
	// TODO(PL-518) test the error context.
	var errorPB pb.CompilerErrorGroup
	err = compiler.GetCompilerErrorContext(status, &errorPB)
	if !assert.NoError(t, err) {
		t.FailNow()
	}

	if !assert.Equal(t, 1, len(errorPB.Errors)) {
		t.FailNow()
	}
	compilerError := errorPB.Errors[0]
	lineColError := compilerError.GetLineColError()
	if !assert.NotEqual(t, lineColError, nil) {
		t.FailNow()
	}
	assert.Equal(t, lineColError.Line, uint64(1))
	assert.Equal(t, lineColError.Column, uint64(15))
	assert.Equal(t, lineColError.Message, "Table not_perf_and_http not found in the relation map")
}
