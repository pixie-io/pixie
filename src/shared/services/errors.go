package services

import (
	"fmt"
	"net/http"

	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"pixielabs.ai/pixielabs/src/shared/services/handler"
)

// HTTPStatusFromCode converts a gRPC error code into the corresponding HTTP response status.
// Copied from: https://github.com/grpc-ecosystem/grpc-gateway/blob/master/runtime/errors.go.
func HTTPStatusFromCode(code codes.Code) int {
	switch code {
	case codes.OK:
		return http.StatusOK
	case codes.Canceled:
		return http.StatusRequestTimeout
	case codes.Unknown:
		return http.StatusInternalServerError
	case codes.InvalidArgument:
		return http.StatusBadRequest
	case codes.DeadlineExceeded:
		return http.StatusGatewayTimeout
	case codes.NotFound:
		return http.StatusNotFound
	case codes.AlreadyExists:
		return http.StatusConflict
	case codes.PermissionDenied:
		return http.StatusForbidden
	case codes.Unauthenticated:
		return http.StatusUnauthorized
	case codes.ResourceExhausted:
		return http.StatusTooManyRequests
	case codes.FailedPrecondition:
		// Note, this deliberately doesn't translate to the similarly named '412 Precondition Failed' HTTP response status.
		return http.StatusBadRequest
	case codes.Aborted:
		return http.StatusConflict
	case codes.OutOfRange:
		return http.StatusBadRequest
	case codes.Unimplemented:
		return http.StatusNotImplemented
	case codes.Internal:
		return http.StatusInternalServerError
	case codes.Unavailable:
		return http.StatusServiceUnavailable
	case codes.DataLoss:
		return http.StatusInternalServerError
	}

	return http.StatusInternalServerError
}

// HTTPStatusFromError returns a HTTP Status from the given GRPC error.
func HTTPStatusFromError(err error, message string) *handler.StatusError {
	grpcStatus, ok := status.FromError(err)
	if !ok {
		return handler.NewStatusError(http.StatusInternalServerError, message)
	}
	return handler.NewStatusError(HTTPStatusFromCode(grpcStatus.Code()), fmt.Sprintf("%s: %s", message, grpcStatus.Message()))
}
