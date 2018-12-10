package linuxchecker

import (
	"os"

	"github.com/golang/protobuf/proto"
	log "github.com/sirupsen/logrus"
	pb "pixielabs.ai/pixielabs/src/platform_dependent/smoke_test/proto"
	"pixielabs.ai/pixielabs/src/platform_dependent/smoke_test/system_checks/linux_checker/expectations"
)

// HostChecker run tests on the host operating system and kernel version information.
func HostChecker(outFH *os.File, pbHostInfo *pb.HostInfo) {
	pbExpectedHostInfo := new(pb.HostInfo)
	expectations.GetHostExpectations(pbExpectedHostInfo)
	pbCheckInfo := new(pb.CheckInfo)
	// Check Host Information.
	testLinuxOS(pbHostInfo, pbExpectedHostInfo, pbCheckInfo)
	testKernelVersion(pbHostInfo, pbExpectedHostInfo, pbCheckInfo)

	// Write the test results to an output file.
	if _, err := outFH.WriteString(proto.MarshalTextString(pbCheckInfo)); err != nil {
		log.WithError(err).Fatalf("Cannot write to output file")
	}
}

// testLinuxOS verifies whether the OS is linux.
func testLinuxOS(pbOS *pb.HostInfo, pbExpected *pb.HostInfo, pbCheckInfo *pb.CheckInfo) {
	pbCheckInfo.TestOS = pb.FAIL
	if pbOS.GetOS() == pbExpected.GetOS() {
		pbCheckInfo.TestOS = pb.PASS
	}
}

// testKernerlVersion verifies whether the kernel can support eBPF.
func testKernelVersion(pbKernelInfo *pb.HostInfo, pbExpected *pb.HostInfo, pbCheckInfo *pb.CheckInfo) {
	pbCheckInfo.TestKernelVersion = pb.FAIL
	// Check Kernel major and minor versions.
	if (pbKernelInfo.GetKernelVersion().GetMajorVersion() >=
		pbExpected.GetKernelVersion().GetMajorVersion()) &&
		(pbKernelInfo.GetKernelVersion().GetMinorVersion() >=
			pbExpected.GetKernelVersion().GetMinorVersion()) {
		pbCheckInfo.TestKernelVersion = pb.PASS
	}
}
