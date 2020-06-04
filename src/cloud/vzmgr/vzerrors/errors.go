package vzerrors

import (
	"errors"

	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
)

var (
	// ErrDeploymentKeyNotFound is used when specified key cannot be located.
	ErrDeploymentKeyNotFound = errors.New("invalid deployment key")
	// ErrProvisionFailedVizierIsActive errors when the specified vizier is active and not disconnected.
	ErrProvisionFailedVizierIsActive = errors.New("provisioning failed because vizier with specified UID is already active")
)

// ToGRPCError converts vzmgr errors to grpc errors if possible.
func ToGRPCError(err error) error {
	switch err {
	case ErrProvisionFailedVizierIsActive:
		return status.Error(codes.ResourceExhausted, err.Error())
	case ErrDeploymentKeyNotFound:
		return status.Error(codes.NotFound, err.Error())
	}
	return status.Error(codes.Internal, err.Error())
}
