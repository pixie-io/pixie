package controller_test

import (
	"testing"

	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"

	"github.com/gogo/protobuf/proto"
	"github.com/golang/mock/gomock"
	"github.com/graph-gophers/graphql-go/gqltesting"

	"pixielabs.ai/pixielabs/src/cloud/api/controller/testutils"
)

const vizFuncsQuery = `
import px
@px.viz.vega("vega spec for f")
def f(start_time: px.Time, end_time: px.Time, svc: str):
  """Doc string for f"""
  return 1

@px.viz.vega("vega spec for g")
def g(a: int, b: float):
  """Doc string for g"""
  return 1
`

const expectedVizFuncsInfoPBStr = `
doc_string_map {
  key: "f"
  value: "Doc string for f"
}
doc_string_map {
  key: "g"
  value: "Doc string for g"
}
viz_spec_map {
  key: "f"
  value {
    vega_spec: "vega spec for f"
  }
}
viz_spec_map {
  key: "g"
  value {
    vega_spec: "vega spec for g"
  }
}
fn_args_map {
  key: "f"
  value {
    args {
      data_type: TIME64NS
      name: "start_time"
    }
    args {
      data_type: TIME64NS
      name: "end_time"
    }
    args {
      data_type: STRING
      name: "svc"
    }
  }
}
fn_args_map {
  key: "g"
  value {
    args {
      data_type: INT64
      name: "a"
    }
    args {
      data_type: FLOAT64
      name: "b"
    }
  }
}
`

func TestExtractVizFuncsInfo(t *testing.T) {
	gqlEnv, _, _, mockScriptMgrSvr, cleanup := testutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	expectedRespPb := &cloudapipb.ExtractVizFuncsInfoResponse{}
	if err := proto.UnmarshalText(expectedVizFuncsInfoPBStr, expectedRespPb); err != nil {
		t.Fatal("Failed to unmarshal expected viz funcs info")
	}

	mockScriptMgrSvr.EXPECT().ExtractVizFuncsInfo(gomock.Any(),
		&cloudapipb.ExtractVizFuncsInfoRequest{
			Script:    vizFuncsQuery,
			FuncNames: []string{},
		}).
		Return(expectedRespPb, nil)

	gqlSchema := LoadSchema(gqlEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				query ($script: String!) {
					extractVizFuncsInfo(script: $script) {
						docStringMap {
							funcName,
							docString	
						},
						vizSpecMap {
							funcName,
							vizSpec { vegaSpec }
						},
						fnArgsMap {
							funcName,
							fnArgSpec { args {
								name,
								dataType,
								semanticType,
								defaultValue
							}}
						}
					}
				}
			`,
			Variables: map[string]interface{}{
				"script": vizFuncsQuery,
			},
			ExpectedResult: `
			{
				"extractVizFuncsInfo": {
					"docStringMap": [
						{
							"docString": "Doc string for f",
							"funcName": "f"
						},
						{
							"docString": "Doc string for g",
							"funcName": "g"
						}
					],
					"vizSpecMap": [
						{
							"funcName": "f",
							"vizSpec": {
								"vegaSpec": "vega spec for f"
							}
						},
						{
							"funcName": "g",
							"vizSpec": {
								"vegaSpec": "vega spec for g"
							}
						}
					],
					"fnArgsMap": [
						{
							"funcName": "f",
							"fnArgSpec": {
								"args": [
									{
										"name": "start_time",
										"dataType": "TIME64NS",
										"defaultValue": "",
										"semanticType": "ST_UNSPECIFIED"
									},
									{
										"name": "end_time",
										"dataType": "TIME64NS",
										"defaultValue": "",
										"semanticType": "ST_UNSPECIFIED"
									},
									{
										"name": "svc",
										"dataType": "STRING",
										"defaultValue": "",
										"semanticType": "ST_UNSPECIFIED"
									}
								]
							}
						},
						{
							"funcName": "g",
							"fnArgSpec": {
								"args": [
									{
										"name": "a",
										"dataType": "INT64",
										"defaultValue": "",
										"semanticType": "ST_UNSPECIFIED"
									},
									{
										"name": "b",
										"dataType": "FLOAT64",
										"defaultValue": "",
										"semanticType": "ST_UNSPECIFIED"
									}
								]
							}
						}
					]
				}
			}
			`,
		},
	})
}

const expectedVizFuncsInfoNoGPBStr = `
doc_string_map {
  key: "f"
  value: "Doc string for f"
}
viz_spec_map {
  key: "f"
  value {
    vega_spec: "vega spec for f"
  }
}
fn_args_map {
  key: "f"
  value {
    args {
      data_type: TIME64NS
      name: "start_time"
    }
    args {
      data_type: TIME64NS
      name: "end_time"
    }
    args {
      data_type: STRING
      name: "svc"
    }
  }
}
`

func TestExtractVizFuncsInfo_FilterFuncs(t *testing.T) {
	gqlEnv, _, _, mockScriptMgrSvr, cleanup := testutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	expectedRespPb := &cloudapipb.ExtractVizFuncsInfoResponse{}
	if err := proto.UnmarshalText(expectedVizFuncsInfoNoGPBStr, expectedRespPb); err != nil {
		t.Fatal("Failed to unmarshal expected viz funcs info")
	}

	mockScriptMgrSvr.EXPECT().ExtractVizFuncsInfo(gomock.Any(),
		&cloudapipb.ExtractVizFuncsInfoRequest{
			Script:    vizFuncsQuery,
			FuncNames: []string{"f"},
		}).
		Return(expectedRespPb, nil)

	gqlSchema := LoadSchema(gqlEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				query ($script: String!) {
					extractVizFuncsInfo(script: $script, funcNames: ["f"]) {
						docStringMap {
							funcName,
							docString	
						},
						vizSpecMap {
							funcName,
							vizSpec { vegaSpec }
						},
						fnArgsMap {
							funcName,
							fnArgSpec { args {
								name,
								dataType,
								semanticType,
								defaultValue
							}}
						}
					}
				}
			`,
			Variables: map[string]interface{}{
				"script": vizFuncsQuery,
			},
			ExpectedResult: `
			{
				"extractVizFuncsInfo": {
					"docStringMap": [
						{
							"docString": "Doc string for f",
							"funcName": "f"
						}
					],
					"vizSpecMap": [
						{
							"funcName": "f",
							"vizSpec": {
								"vegaSpec": "vega spec for f"
							}
						}
					],
					"fnArgsMap": [
						{
							"funcName": "f",
							"fnArgSpec": {
								"args": [
									{
										"name": "start_time",
										"dataType": "TIME64NS",
										"defaultValue": "",
										"semanticType": "ST_UNSPECIFIED"
									},
									{
										"name": "end_time",
										"dataType": "TIME64NS",
										"defaultValue": "",
										"semanticType": "ST_UNSPECIFIED"
									},
									{
										"name": "svc",
										"dataType": "STRING",
										"defaultValue": "",
										"semanticType": "ST_UNSPECIFIED"
									}
								]
							}
						}
					]
				}
			}
			`,
		},
	})
}
