package expectations

import (
	pb "pixielabs.ai/pixielabs/src/platform_dependent/smoke_test/proto"
)

// GetHostExpectations sets the expectations for the os and other host information.
func GetHostExpectations(pbHostInfo *pb.HostInfo) {
	// Expected OS is linux.
	pbHostInfo.OS = pb.LINUX

	// Minimum expected major and minor versions of linux kernel.
	pbKernelInfo := new(pb.KernelInfo)
	pbKernelInfo.MajorVersion = uint64(4)
	pbKernelInfo.MinorVersion = uint64(15)
	pbHostInfo.KernelVersion = pbKernelInfo
}
