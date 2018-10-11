package linuxchecker

import (
	"testing"

	"github.com/stretchr/testify/assert"
	pb "pixielabs.ai/pixielabs/platform-dependent/smoke-test/proto"
)

func testCheckerLinuxOS(t *testing.T) {
	pbHostInfo := new(pb.HostInfo)
	pbHostInfo.OS = pb.LINUX

	pbExpectedHostInfo := new(pb.HostInfo)
	pbExpectedHostInfo.OS = pb.LINUX

	pbCheckInfo := new(pb.CheckInfo)
	assert := assert.New(t)

	testLinuxOS(pbHostInfo, pbExpectedHostInfo, pbCheckInfo)
	assert.Equal(pbCheckInfo.GetTestOS(), pb.PASS)
}

func testCheckerKernelVersion(t *testing.T) {
	pbHostInfo := new(pb.HostInfo)

	pbKernelInfo := new(pb.KernelInfo)
	pbKernelInfo.MajorVersion = 4
	pbKernelInfo.MinorVersion = 15

	pbHostInfo.KernelVersion = pbKernelInfo

	pbExpectedHostInfo := new(pb.HostInfo)
	pbExpectedHostInfo.KernelVersion = pbKernelInfo

	pbCheckInfo := new(pb.CheckInfo)
	assert := assert.New(t)

	testKernelVersion(pbHostInfo, pbExpectedHostInfo, pbCheckInfo)
	assert.Equal(pbCheckInfo.GetTestKernelVersion(), pb.PASS)
}
