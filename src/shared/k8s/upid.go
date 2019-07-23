package k8s

import (
	"fmt"

	"pixielabs.ai/pixielabs/src/shared/types"
)

// ASIDFromUPID gets the ASID from the UPID.
func ASIDFromUPID(u *types.UInt128) uint32 {
	return uint32(u.High >> 32)
}

// PIDFromUPID gets the PID from the UPID.
func PIDFromUPID(u *types.UInt128) uint32 {
	return uint32(u.High)
}

// StartTSFromUPID gets the start timestamp from the UPID.
func StartTSFromUPID(u *types.UInt128) uint64 {
	return uint64(u.Low)
}

// StringFromUPID gets a string from the UPID.
func StringFromUPID(u *types.UInt128) string {
	return fmt.Sprintf("%d:%d:%d", ASIDFromUPID(u), PIDFromUPID(u), StartTSFromUPID(u))
}
