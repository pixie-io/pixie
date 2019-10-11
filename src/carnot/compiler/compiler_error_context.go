package compiler

import (
	"errors"
	"fmt"

	"github.com/gogo/protobuf/types"
	pb "pixielabs.ai/pixielabs/src/carnot/compiler/compilerpb"
	statuspb "pixielabs.ai/pixielabs/src/common/base/proto"
)

// GetCompilerErrorContext inspects the context field (pb.Any msg) and
// converts into the CompilerErrorGroup message for proper use.
func GetCompilerErrorContext(status *statuspb.Status, errorPB *pb.CompilerErrorGroup) error {
	context := status.GetContext()
	if context == nil {
		return errors.New("No context in status")
	}

	if !types.Is(context, errorPB) {
		return fmt.Errorf("Didn't expect type %s", context.TypeUrl)
	}
	return types.UnmarshalAny(context, errorPB)
}

// HasContext returns true whether status has context or not.
func HasContext(status *statuspb.Status) bool {
	context := status.GetContext()
	return context != nil
}
