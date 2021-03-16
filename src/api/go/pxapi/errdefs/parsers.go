package errdefs

import (
	"fmt"

	"google.golang.org/grpc/codes"

	publicvizierapipb "pixielabs.ai/pixielabs/src/api/public/vizierapipb"
)

// ParseStatus parses the status field. Returns an error if exists.
func ParseStatus(s *publicvizierapipb.Status) error {
	if s == nil || s.Code == int32(codes.OK) {
		return nil
	}
	if len(s.ErrorDetails) > 0 {
		details := s.ErrorDetails
		var errs []error
		hasCompilerErrors := false
		for _, d := range details {
			switch e := d.Error.(type) {
			case *publicvizierapipb.ErrorDetails_CompilerError:
				errs = append(errs, newCompilerErrorWithDetails(e.CompilerError))
				hasCompilerErrors = true
			default:
				errs = append(errs, ErrInternal)
			}
		}
		if hasCompilerErrors {
			return newCompilerMultiError(errs...)
		}
		return newErrorGroup(errs...)
	}
	if s.Code == int32(codes.InvalidArgument) {
		return fmt.Errorf("%w: %s", ErrInvalidArgument, s.Message)
	}
	return ErrInternal

}
