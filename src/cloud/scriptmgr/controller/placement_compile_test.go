package controller_test

import (
	"regexp"
	"strings"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/cloud/scriptmgr/controller"
)

const placementJSONTxt = `{
  "name": "service-latencies",
  "args": {
    "service": "px.Service",
    "namespace": "px.Namespace",
    "start_time": "px.Time",
    "end_time": "px.Time"
  },
  "tables": {
    "error_chart": {
      "func": "get_error_chart",
      "args": {
        "svc": "@service",
        "start_time": "@start_time",
        "end_time": "@end_time"
      }
    },
    "latency_between_svcs_chart": {
      "func": "get_latency_between_svcs_chart",
      "args": {
        "svc1": "@service",
        "svc2": "carts",
        "start_time": "@start_time",
        "end_time": "@end_time"
      }
    }
  }
}`

type argMap struct {
	argNameToValue *map[string]string
}

func parseArgsFromCall(argsValueStr string) *argMap {
	as := strings.Split(argsValueStr, ",")

	argNameToValue := make(map[string]string)
	for _, a := range as {
		argsSplit := strings.Split(a, "=")
		name, value := strings.Trim(argsSplit[0], " "), strings.Trim(argsSplit[1], " ")
		argNameToValue[name] = value
	}
	return &argMap{
		&argNameToValue,
	}
}

const dfLineRegex = `[A-z_0-9]* = ([A-z_0-9]*)\(([A-z_0-9=.'\s_,]*)\)`

func getDFArguments(callLine, spacing string) (*map[string]*argMap, error) {
	f := make(map[string]*argMap)
	re, err := regexp.Compile(dfLineRegex)
	if err != nil {
		return nil, err
	}
	matches := re.FindAllStringSubmatch(callLine, -1)
	for _, m := range matches {
		funcIdx := 1
		argsIdx := 2
		funcName := m[funcIdx]

		f[funcName] = parseArgsFromCall(m[argsIdx])
	}

	return &f, nil
}

func getDisplayArguments(callLine, spacing string) (*map[string]string, error) {
	re, err := regexp.Compile(dfLineRegex + `\n\s*px.display\(([A-z_0-9_]*), ('.*')\)`)
	if err != nil {
		return nil, err
	}
	f := make(map[string]string)
	matches := re.FindAllStringSubmatch(callLine, -1)
	for _, m := range matches {
		funcIdx := 1
		outTableNameIdx := 4
		funcName := m[funcIdx]
		outTableName := m[outTableNameIdx]

		f[outTableName] = funcName
	}

	return &f, nil
}

const compiledMainForPlacement = `
def main(service: px.Service, namespace: px.Namespace, start_time: px.Time, end_time: px.Time):
    df = get_error_chart(svc=service, start_time=start_time, end_time=end_time)
    px.display(df, 'error_chart')
    df = get_latency_between_svcs_chart(svc1=service, svc2='carts', start_time=start_time, end_time=end_time)
    px.display(df, 'latency_between_svcs_chart')
`

func TestPlacementToPxl(t *testing.T) {
	spacing := "    "
	compiler := controller.NewPlacementCompiler()
	res, err := compiler.PlacementToPxl(placementJSONTxt)
	require.NoError(t, err)

	actual, err1 := getDFArguments(res, spacing)
	expected, err2 := getDFArguments(compiledMainForPlacement, spacing)
	require.NoError(t, err1)
	require.NoError(t, err2)

	assert.Equal(t, 2, len(*actual))
	assert.Equal(t, *expected, *actual)

	actualDisplay, err1 := getDisplayArguments(res, spacing)
	expectedDisplay, err2 := getDisplayArguments(compiledMainForPlacement, spacing)
	require.NoError(t, err1)
	require.NoError(t, err2)
	require.Equal(t, 2, len(*actualDisplay))
	assert.Equal(t, expectedDisplay, actualDisplay)
}

const jsonPercentilePlacement = `{
  "name": "service-latencies",
  "args": {
    "service": "px.Service"
  },
  "tables": {
    "error_chart": {
      "func": "latency_percentiles",
      "args": {
        "svc": "@service",
        "percentile": 51.5,
        "limit": 10
      }
    }
  }
}`

const compiledPercentilePxl = `
def main(service: px.Service):
    df = latency_percentiles(svc=service, percentile=51.5, limit=10)
    px.display(df, 'error_chart')
`

// Test integers and
func TestPlacementPercentilPxl(t *testing.T) {
	spacing := "    "
	compiler := controller.NewPlacementCompiler()
	res, err := compiler.PlacementToPxl(jsonPercentilePlacement)
	require.NoError(t, err)

	actual, err1 := getDFArguments(res, spacing)
	expected, err2 := getDFArguments(compiledPercentilePxl, spacing)

	require.NoError(t, err1)
	require.NoError(t, err2)

	assert.Equal(t, 1, len(*actual))
	assert.Equal(t, *expected, *actual)

	actualDisplay, err1 := getDisplayArguments(res, spacing)
	expectedDisplay, err2 := getDisplayArguments(compiledPercentilePxl, spacing)

	require.NoError(t, err1)
	require.NoError(t, err2)

	require.Equal(t, 1, len(*actualDisplay))
	assert.Equal(t, expectedDisplay, actualDisplay)
}

func TestBadJSONFails(t *testing.T) {
	data := `{"missing": "closing bracket"`
	compiler := controller.NewPlacementCompiler()
	_, err := compiler.PlacementToPxl(data)
	assert.EqualError(t, err, "unexpected end of JSON input")
}

const badServiceNameArgInFnPlacement = `{
  "name": "service-latencies",
  "args": {
    "service": "px.Service"
  },
  "tables": {
    "error_chart": {
      "func": "get_latency_p50",
      "args": {
        "service name": "@service"
      }
    }
  }
}`
const badGlobalArgPlacement = `{
  "name": "service-latencies",
  "args": {
    "service name": "px.Service"
  },
  "tables": {
    "error_chart": {
      "func": "get_latency_p50",
      "args": {
        "service": "carts"
      }
    }
  }
}`

// Only some arg names can work as strings
func TestInvalidArgNamesFails(t *testing.T) {
	compiler := controller.NewPlacementCompiler()
	_, err := compiler.PlacementToPxl(badServiceNameArgInFnPlacement)
	assert.EqualError(t, err, "'service name' is an invalid argname. must match regex ^[A-z_][A-z0-9_]*$")
	_, err = compiler.PlacementToPxl(badGlobalArgPlacement)
	assert.EqualError(t, err, "'service name' is an invalid argname. must match regex ^[A-z_][A-z0-9_]*$")
}

// Our structure doesnt have a table here, should error out somehow if that happens
const badStructurePlacement = `{
  "name": "service-latencies",
  "args": {
    "service_name": "px.Service"
  },
  "error_chart": {
    "func": "get_latency_p50",
    "args": {
      "service": "carts"
    }
  }
}`

// Verify that bad structure, yet still Valid can be passed in and detected.
func TestInvalidJSONStructureFails(t *testing.T) {
	compiler := controller.NewPlacementCompiler()
	_, err := compiler.PlacementToPxl(badStructurePlacement)
	assert.EqualError(t, err, "you must specify tables in the placement spec")
}

// Pod name is not a global arg.
const nonExistantGlobalArgPlacement = `{
  "name": "service-latencies",
  "args": {
    "service_name": "px.Service"
  },
  "tables": {
    "error_chart": {
      "func": "get_latency_p50",
      "args": {
        "service": "@pod_name"
      }
    }
  }
}`

func TestNonexistantGlobalArg(t *testing.T) {
	compiler := controller.NewPlacementCompiler()
	_, err := compiler.PlacementToPxl(nonExistantGlobalArgPlacement)
	assert.EqualError(t, err, "'pod_name' is not a valid global arg")
}

// foobar is not a valid type name.
const badGlobalArgSpec = `{
  "name": "service-latencies",
  "args": {
    "service_name": "px.foobar"
  },
  "tables": {
    "error_chart": {
      "func": "get_latency_p50",
      "args": {
        "service": "@service_name"
      }
    }
  }
}`

func TestBadGlobalArgType(t *testing.T) {
	t.Skip()
	compiler := controller.NewPlacementCompiler()
	_, err := compiler.PlacementToPxl(badGlobalArgSpec)
	assert.EqualError(t, err, "'foobar' is not a valid attribute of px")
}
