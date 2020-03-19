package controller

import (
	"context"
	"sort"

	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
	typespb "pixielabs.ai/pixielabs/src/shared/types/proto"
)

type docStringMapEntryResolver struct {
	FuncName  string
	DocString string
}

type vizSpecResolver struct {
	VegaSpec string
}

type vizSpecMapEntryResolver struct {
	FuncName string
	VizSpec  *vizSpecResolver
}

type fnArgsSpecArgResolver struct {
	Name         string
	DataType     string
	SemanticType string
	DefaultValue string
}

type fnArgsSpecResolver struct {
	Args []fnArgsSpecArgResolver
}

type fnArgsMapEntryResolver struct {
	FuncName  string
	FnArgSpec fnArgsSpecResolver
}

// VizFuncsInfoResolver is the resolver responsible for resolving the info about px.viz funcs
type VizFuncsInfoResolver struct {
	DocStringMap []docStringMapEntryResolver
	VizSpecMap   []vizSpecMapEntryResolver
	FnArgsMap    []fnArgsMapEntryResolver
}

type parseScriptForVizFuncsArgs struct {
	Script    string
	FuncNames *[]string
}

func dataTypePbToString(dataType cloudapipb.DataType) string {
	return typespb.DataType_name[int32(dataType)]
}

func semanticTypePbToString(semanticType cloudapipb.SemanticType) string {
	return typespb.SemanticType_name[int32(semanticType)]
}

func convertFnArgsMapToGQL(goMap map[string]*cloudapipb.FuncArgsSpec, fnArgsMap *[]fnArgsMapEntryResolver) {
	i := 0
	for name, fnArgsSpec := range goMap {
		entry := fnArgsMapEntryResolver{
			FuncName:  name,
			FnArgSpec: fnArgsSpecResolver{},
		}
		for _, arg := range fnArgsSpec.Args {
			argResolver := fnArgsSpecArgResolver{
				Name:         arg.Name,
				DataType:     dataTypePbToString(arg.DataType),
				SemanticType: semanticTypePbToString(arg.SemanticType),
				DefaultValue: arg.DefaultValue,
			}
			entry.FnArgSpec.Args = append(entry.FnArgSpec.Args, argResolver)
		}
		(*fnArgsMap)[i] = entry
		i++
	}
}

func convertVizSpecMapToGQL(goMap map[string]*cloudapipb.VizSpec, vizSpecMap *[]vizSpecMapEntryResolver) {
	i := 0
	for name, vizspec := range goMap {
		entry := vizSpecMapEntryResolver{
			FuncName: name,
			VizSpec: &vizSpecResolver{
				VegaSpec: vizspec.VegaSpec,
			},
		}
		(*vizSpecMap)[i] = entry
		i++
	}
}

func convertDocStringMapToGQL(goMap map[string]string, docStringMap *[]docStringMapEntryResolver) {
	i := 0
	for name, docstring := range goMap {
		entry := docStringMapEntryResolver{
			FuncName:  name,
			DocString: docstring,
		}
		(*docStringMap)[i] = entry
		i++
	}
}

// ExtractVizFuncsInfo resolves info about the px.viz decorated functions in the passed in script.
func (q *QueryResolver) ExtractVizFuncsInfo(ctx context.Context, args *parseScriptForVizFuncsArgs) (VizFuncsInfoResolver, error) {
	funcNames := []string{}
	if args.FuncNames != nil {
		funcNames = *args.FuncNames
	}
	req := &cloudapipb.ExtractVizFuncsInfoRequest{
		Script:    args.Script,
		FuncNames: funcNames,
	}

	vizFuncsInfo, err := q.Env.ScriptMgrServer.ExtractVizFuncsInfo(ctx, req)
	if err != nil {
		return VizFuncsInfoResolver{}, err
	}

	resolver := VizFuncsInfoResolver{
		DocStringMap: make([]docStringMapEntryResolver, len(vizFuncsInfo.DocStringMap)),
		VizSpecMap:   make([]vizSpecMapEntryResolver, len(vizFuncsInfo.VizSpecMap)),
		FnArgsMap:    make([]fnArgsMapEntryResolver, len(vizFuncsInfo.FnArgsMap)),
	}

	convertDocStringMapToGQL(vizFuncsInfo.DocStringMap, &resolver.DocStringMap)
	convertVizSpecMapToGQL(vizFuncsInfo.VizSpecMap, &resolver.VizSpecMap)
	convertFnArgsMapToGQL(vizFuncsInfo.FnArgsMap, &resolver.FnArgsMap)

	// Sort by function name to ensure consistent output.
	sort.Slice(resolver.DocStringMap, func(i, j int) bool {
		return resolver.DocStringMap[i].FuncName < resolver.DocStringMap[j].FuncName
	})
	sort.Slice(resolver.VizSpecMap, func(i, j int) bool {
		return resolver.VizSpecMap[i].FuncName < resolver.VizSpecMap[j].FuncName
	})
	sort.Slice(resolver.FnArgsMap, func(i, j int) bool {
		return resolver.FnArgsMap[i].FuncName < resolver.FnArgsMap[j].FuncName
	})

	return resolver, nil
}
