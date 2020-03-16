package controller

import (
	"context"
	"errors"
	"fmt"

	"pixielabs.ai/pixielabs/src/cloud/scriptmgr/scriptmgrpb"
	statuspb "pixielabs.ai/pixielabs/src/common/base/proto"
	"pixielabs.ai/pixielabs/src/shared/scriptspb"
)

// Planner is the interface to the cc planner via cgo.
type Planner interface {
	ParseScriptForVizFuncsInfo(script string) (*scriptspb.VizFuncsInfoResult, error)
	Free()
}

// Server implements the GRPC Server for the scriptmgr service.
type Server struct {
	planner Planner
}

// NewServer creates a new GRPC scriptmgr server.
func NewServer(planner Planner) *Server {
	return &Server{planner}
}

// ExtractVizFuncsInfo parses a non-persisted script and returns info such as docstrings, vega spec, and func args.
func (s *Server) ExtractVizFuncsInfo(ctx context.Context, req *scriptmgrpb.ExtractVizFuncsInfoRequest) (*scriptspb.VizFuncsInfo, error) {
	plannerResultPB, err := s.planner.ParseScriptForVizFuncsInfo(req.Script)
	if err != nil {
		return nil, err
	}

	// When the status is not OK, this means it's a compilation error on the query passed in.
	if plannerResultPB.Status.ErrCode != statuspb.OK {
		return nil, errors.New(plannerResultPB.Status.Msg)
	}

	if len(req.FuncNames) > 0 {
		return filterVizFuncInfoByFuncNames(plannerResultPB.Info, req.FuncNames)
	}
	return plannerResultPB.Info, nil
}

func filterVizFuncInfoByFuncNames(info *scriptspb.VizFuncsInfo, funcNames []string) (*scriptspb.VizFuncsInfo, error) {
	newInfo := &scriptspb.VizFuncsInfo{
		DocStringMap: make(map[string]string, len(funcNames)),
		VizSpecMap:   make(map[string]*scriptspb.VizSpec, len(funcNames)),
		FnArgsMap:    make(map[string]*scriptspb.FuncArgsSpec, len(funcNames)),
	}

	errmsg := func(f string) error { return fmt.Errorf("function '%s' was not found in script", f) }

	for _, f := range funcNames {
		docstring, ok := info.DocStringMap[f]
		if !ok {
			return nil, errmsg(f)
		}
		vizspec, ok := info.VizSpecMap[f]
		if !ok {
			return nil, errmsg(f)
		}
		fnargs, ok := info.FnArgsMap[f]
		if !ok {
			return nil, errmsg(f)
		}
		newInfo.DocStringMap[f] = docstring
		newInfo.VizSpecMap[f] = vizspec
		newInfo.FnArgsMap[f] = fnargs
	}
	return newInfo, nil
}
