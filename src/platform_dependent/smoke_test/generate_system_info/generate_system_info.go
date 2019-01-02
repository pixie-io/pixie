package generatesysteminfo

import (
	"os"
	"runtime"

	"github.com/golang/protobuf/proto"
	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/src/platform_dependent/smoke_test/generate_system_info/linux_info"
	pb "pixielabs.ai/pixielabs/src/platform_dependent/smoke_test/proto"
)

// InfoForOS gets the system information for the identified operating system and writes protobuf messages to a file.
func InfoForOS(outputFileHandle *os.File) (*pb.SystemInfo, *pb.HostInfo) {
	switch runtime.GOOS {
	case "linux":
		pbSystemInfo, pbHostInfo := getLinuxInfo(outputFileHandle)
		return pbSystemInfo, pbHostInfo
	default:
		unsupportedOs := "Unsupported OS: " + runtime.GOOS
		if _, err := outputFileHandle.WriteString(unsupportedOs); err != nil {
			log.WithError(err).Fatalf("Cannot write to output file")
		}
		return nil, nil
	}
}

// Generate system information for a host that runs linux.
func getLinuxInfo(outFH *os.File) (*pb.SystemInfo, *pb.HostInfo) {
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

	return pbSystemInfoMessage, pbHostInfoMessage
}
