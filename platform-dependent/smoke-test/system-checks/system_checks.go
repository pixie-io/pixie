package systemchecks

import (
	"os"
	"runtime"

	"github.com/golang/protobuf/proto"
	log "github.com/sirupsen/logrus"
	pb "pixielabs.ai/pixielabs/platform-dependent/smoke-test/proto"
	"pixielabs.ai/pixielabs/platform-dependent/smoke-test/system-checks/linux-checker"
)

// RunChecker runs all the checks for the generated system and host information.
// Currently we only support linux.
func RunChecker(outputFileHandle *os.File, pbSystemInfo *pb.SystemInfo, pbHostInfo *pb.HostInfo) {
	switch runtime.GOOS {
	case "linux":
		linuxchecker.HostChecker(outputFileHandle, pbHostInfo)
	default:
		pbCheckInfo := new(pb.CheckInfo)
		pbCheckInfo.TestKernelVersion = pb.FAIL
		pbCheckInfo.TestOS = pb.FAIL
		// Write the test results to an output file.
		if _, err := outputFileHandle.WriteString(proto.MarshalTextString(pbCheckInfo)); err != nil {
			log.WithError(err).Fatalf("Cannot write to output file")
		}
	}
}
