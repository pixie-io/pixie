package controller_test

import (
	"testing"

	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"

	"github.com/gogo/protobuf/proto"
	"github.com/golang/mock/gomock"
	"github.com/graph-gophers/graphql-go/gqltesting"

	"pixielabs.ai/pixielabs/src/cloud/api/controller/testutils"
)

const visFuncsQuery = `
import px
@px.vis.vega("vega spec for f")
def f(start_time: px.Time, end_time: px.Time, svc: str):
  """Doc string for f"""
  return 1

@px.vis.vega("vega spec for g")
def g(a: int, b: float):
  """Doc string for g"""
  return 1
`

const expectedVisFuncsInfoPBStr = `
doc_string_map {
  key: "f"
  value: "Doc string for f"
}
doc_string_map {
  key: "g"
  value: "Doc string for g"
}
vis_spec_map {
  key: "f"
  value {
    vega_spec: "vega spec for f"
  }
}
vis_spec_map {
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

func TestExtractVisFuncsInfo(t *testing.T) {
	gqlEnv, _, _, mockScriptMgrSvr, cleanup := testutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	expectedRespPb := &cloudapipb.ExtractVisFuncsInfoResponse{}
	if err := proto.UnmarshalText(expectedVisFuncsInfoPBStr, expectedRespPb); err != nil {
		t.Fatal("Failed to unmarshal expected vis funcs info")
	}

	mockScriptMgrSvr.EXPECT().ExtractVisFuncsInfo(gomock.Any(),
		&cloudapipb.ExtractVisFuncsInfoRequest{
			Script:    visFuncsQuery,
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
					extractVisFuncsInfo(script: $script) {
						docStringMap {
							funcName,
							docString	
						},
						visSpecMap {
							funcName,
							visSpec { vegaSpec }
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
				"script": visFuncsQuery,
			},
			ExpectedResult: `
			{
				"extractVisFuncsInfo": {
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
					"visSpecMap": [
						{
							"funcName": "f",
							"visSpec": {
								"vegaSpec": "vega spec for f"
							}
						},
						{
							"funcName": "g",
							"visSpec": {
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

const expectedVisFuncsInfoNoGPBStr = `
doc_string_map {
  key: "f"
  value: "Doc string for f"
}
vis_spec_map {
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

func TestExtractVisFuncsInfo_FilterFuncs(t *testing.T) {
	gqlEnv, _, _, mockScriptMgrSvr, cleanup := testutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	expectedRespPb := &cloudapipb.ExtractVisFuncsInfoResponse{}
	if err := proto.UnmarshalText(expectedVisFuncsInfoNoGPBStr, expectedRespPb); err != nil {
		t.Fatal("Failed to unmarshal expected vis funcs info")
	}

	mockScriptMgrSvr.EXPECT().ExtractVisFuncsInfo(gomock.Any(),
		&cloudapipb.ExtractVisFuncsInfoRequest{
			Script:    visFuncsQuery,
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
					extractVisFuncsInfo(script: $script, funcNames: ["f"]) {
						docStringMap {
							funcName,
							docString	
						},
						visSpecMap {
							funcName,
							visSpec { vegaSpec }
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
				"script": visFuncsQuery,
			},
			ExpectedResult: `
			{
				"extractVisFuncsInfo": {
					"docStringMap": [
						{
							"docString": "Doc string for f",
							"funcName": "f"
						}
					],
					"visSpecMap": [
						{
							"funcName": "f",
							"visSpec": {
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
