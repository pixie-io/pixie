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

type visSpecResolver struct {
	VegaSpec string
}

type visSpecMapEntryResolver struct {
	FuncName string
	VisSpec  *visSpecResolver
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

// VisFuncsInfoResolver is the resolver responsible for resolving the info about px.vis funcs
type VisFuncsInfoResolver struct {
	DocStringMap []docStringMapEntryResolver
	VisSpecMap   []visSpecMapEntryResolver
	FnArgsMap    []fnArgsMapEntryResolver
}

type parseScriptForVisFuncsArgs struct {
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

func convertVisSpecMapToGQL(goMap map[string]*cloudapipb.VisSpec, visSpecMap *[]visSpecMapEntryResolver) {
	i := 0
	for name, vizspec := range goMap {
		entry := visSpecMapEntryResolver{
			FuncName: name,
			VisSpec: &visSpecResolver{
				VegaSpec: vizspec.VegaSpec,
			},
		}
		(*visSpecMap)[i] = entry
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

// ExtractVisFuncsInfo resolves info about the px.vis decorated functions in the passed in script.
func (q *QueryResolver) ExtractVisFuncsInfo(ctx context.Context, args *parseScriptForVisFuncsArgs) (VisFuncsInfoResolver, error) {
	funcNames := []string{}
	if args.FuncNames != nil {
		funcNames = *args.FuncNames
	}
	req := &cloudapipb.ExtractVisFuncsInfoRequest{
		Script:    args.Script,
		FuncNames: funcNames,
	}

	visFuncsInfo, err := q.Env.ScriptMgrServer.ExtractVisFuncsInfo(ctx, req)
	if err != nil {
		return VisFuncsInfoResolver{}, err
	}

	resolver := VisFuncsInfoResolver{
		DocStringMap: make([]docStringMapEntryResolver, len(visFuncsInfo.DocStringMap)),
		VisSpecMap:   make([]visSpecMapEntryResolver, len(visFuncsInfo.VisSpecMap)),
		FnArgsMap:    make([]fnArgsMapEntryResolver, len(visFuncsInfo.FnArgsMap)),
	}

	convertDocStringMapToGQL(visFuncsInfo.DocStringMap, &resolver.DocStringMap)
	convertVisSpecMapToGQL(visFuncsInfo.VisSpecMap, &resolver.VisSpecMap)
	convertFnArgsMapToGQL(visFuncsInfo.FnArgsMap, &resolver.FnArgsMap)

	// Sort by function name to ensure consistent output.
	sort.Slice(resolver.DocStringMap, func(i, j int) bool {
		return resolver.DocStringMap[i].FuncName < resolver.DocStringMap[j].FuncName
	})
	sort.Slice(resolver.VisSpecMap, func(i, j int) bool {
		return resolver.VisSpecMap[i].FuncName < resolver.VisSpecMap[j].FuncName
	})
	sort.Slice(resolver.FnArgsMap, func(i, j int) bool {
		return resolver.FnArgsMap[i].FuncName < resolver.FnArgsMap[j].FuncName
	})

	return resolver, nil
}
