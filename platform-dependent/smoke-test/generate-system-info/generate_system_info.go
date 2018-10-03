package generatesysteminfo

import (
	"github.com/golang/protobuf/proto"
	log "github.com/sirupsen/logrus"
	"os"
	"pixielabs.ai/pixielabs/platform-dependent/smoke-test/generate-system-info/linux-info"
	pb "pixielabs.ai/pixielabs/platform-dependent/smoke-test/proto"
	"runtime"
)

// InfoForOS gets the system information for the identified operating system and writes protobuf messages to a file.
func InfoForOS(outputFileHandle *os.File) {
	switch runtime.GOOS {
	case "linux":
		getLinuxInfo(outputFileHandle)
	default:
		unsupportedOs := "Unsupported OS: " + runtime.GOOS
		if _, err := outputFileHandle.WriteString(unsupportedOs); err != nil {
			log.WithError(err).Fatalf("Cannot write to output file")
		}
		if err := outputFileHandle.Close(); err != nil {
			log.WithError(err).Fatalf("Cannot close output file")
		}
	}
}

// Generate system information for a host that runs linux.
func getLinuxInfo(outFH *os.File) {
	pbCPUInfoMessage := linuxinfo.GetLinuxCPUInfo()
	pbMemInfoMessage := linuxinfo.GetLinuxMemInfo()
	pbHostInfoMessage := linuxinfo.GetLinuxHostInfo()

	pbSystemInfoMessage := new(pb.SystemInfo)
	pbSystemInfoMessage.CpuInfo = pbCPUInfoMessage
	pbSystemInfoMessage.MemInfo = pbMemInfoMessage

	if _, err := outFH.WriteString(proto.MarshalTextString(pbSystemInfoMessage)); err != nil {
		log.WithError(err).Fatalf("Cannot write to output file")
	}
	if _, err := outFH.WriteString(proto.MarshalTextString(pbHostInfoMessage)); err != nil {
		log.WithError(err).Fatalf("Cannot write to output file")
	}

	if err := outFH.Close(); err != nil {
		log.WithError(err).Fatalf("Cannot close output file")
	}
}
