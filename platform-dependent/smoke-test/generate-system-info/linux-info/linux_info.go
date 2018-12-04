package linuxinfo

import (
	"fmt"
	"os"
	"os/exec"
	"strings"

	"github.com/blang/semver"

	linuxproc "github.com/c9s/goprocinfo/linux"
	pb "pixielabs.ai/pixielabs/platform-dependent/smoke-test/proto"
)

// GetLinuxCPUInfo gets the CPU information for the host system and generates a protobuf message.
func GetLinuxCPUInfo() *pb.CPUInfo {
	errString := ""
	cpuInfoPath := "/proc/cpuinfo"
	pbCPUInfo := new(pb.CPUInfo)
	// Get CPU information of the host system.
	if cpuInfo, err := linuxproc.ReadCPUInfo(cpuInfoPath); err != nil {
		errString += "Could not read: " + cpuInfoPath + "\n" + err.Error() + "\n"
	} else {
		pbCPUInfo = convertCPUInfoToProto(cpuInfo)
	}
	pbCPUInfo.Error = errString
	return pbCPUInfo
}

func convertCPUInfoToProto(cpuInfo *linuxproc.CPUInfo) *pb.CPUInfo {
	pbCPUInfo := new(pb.CPUInfo)
	for _, processor := range cpuInfo.Processors {
		pbProcessor := new(pb.Processor)
		// Assign all the CPU info from the processor struct
		pbProcessor.ID = processor.Id
		pbProcessor.VendorID = string(processor.VendorId)
		pbProcessor.Model = int64(processor.Model)
		pbProcessor.ModelName = string(processor.ModelName)
		pbProcessor.Cores = int64(processor.Cores)
		pbProcessor.MHz = float64(processor.MHz)
		pbProcessor.CacheSize = int64(processor.CacheSize)
		pbProcessor.PhysicalID = int64(processor.PhysicalId)
		pbProcessor.CoreID = int64(processor.CoreId)
		for _, flag := range processor.Flags {
			pbProcessor.Flags = append(pbProcessor.Flags, flag)
		}
		pbCPUInfo.Processors = append(pbCPUInfo.Processors, pbProcessor)
	}
	return pbCPUInfo
}

// GetLinuxMemInfo gets the Memory information for the host system and generate a protobuf message.
func GetLinuxMemInfo() *pb.MemInfo {
	errString := ""
	memInfoPath := "/proc/meminfo"
	pbMemInfo := new(pb.MemInfo)
	// Get MemInfo of the host system.
	if memInfo, err := linuxproc.ReadMemInfo(memInfoPath); err != nil {
		errString += "Could not read: " + memInfoPath + "\n" + err.Error() + "\n"
	} else {
		pbMemInfo = convertMemInfoToProto(memInfo)
	}
	pbMemInfo.Error = errString
	return pbMemInfo
}

func convertMemInfoToProto(memInfo *linuxproc.MemInfo) *pb.MemInfo {
	pbMemInfo := new(pb.MemInfo)

	pbMemInfo.MemTotal = memInfo.MemTotal
	pbMemInfo.MemFree = memInfo.MemFree
	pbMemInfo.MemAvailable = memInfo.MemAvailable
	pbMemInfo.Buffers = memInfo.Buffers
	pbMemInfo.Cached = memInfo.Cached
	pbMemInfo.SwapCached = memInfo.SwapCached
	pbMemInfo.Active = memInfo.Active

	return pbMemInfo
}

// GetLinuxHostInfo gets information about the host operating system, name, environment vartiables, kernel, etc.
func GetLinuxHostInfo() *pb.HostInfo {
	errArray := []string{}
	pbHostInfo := new(pb.HostInfo)

	// Set Linux as the OS.
	pbHostInfo.OS = pb.LINUX

	// Get the kernel version and parse it.
	kernelCmd := exec.Command("uname", "-r")
	if kernelByteArray, err := kernelCmd.CombinedOutput(); err != nil {
		errArray = append(errArray, "Could not get kernel information: "+err.Error())
	} else {
		pbKernelInfo := new(pb.KernelInfo)
		pbKernelInfo, kernelErrorStr := getLinuxKernelInfo(string(kernelByteArray[:]))
		pbHostInfo.KernelVersion = pbKernelInfo
		errArray = append(errArray, kernelErrorStr)
	}

	// Get the OS details for the host.
	osInfoPath := "/etc/os-release"
	cmd := exec.Command("cat", osInfoPath)
	if osByteArray, err := cmd.CombinedOutput(); err != nil {
		errArray = append(errArray, "Could not read: "+osInfoPath+"\n"+err.Error())
	} else {
		pbLinuxOSInfo := new(pb.HostInfo_LinuxOSInfo)
		pbLinuxOSInfo, osErrorString := getLinuxOSInfo(string(osByteArray[:]))
		pbHostInfo.OSDetails = pbLinuxOSInfo
		errArray = append(errArray, osErrorString)
	}

	// Get the host name
	if hostName, err := os.Hostname(); err != nil {
		errArray = append(errArray, "Could not get the hostname: "+err.Error())
	} else {
		pbHostInfo.HostName = hostName
	}

	// Get the host env variables
	pbHostInfo.HostEnv = strings.Join(os.Environ(), "")

	// Add the error String
	pbHostInfo.Error = strings.Join(errArray, "\n")
	return pbHostInfo
}

// Helper function to get the kernel version and parse it.
func getLinuxKernelInfo(kernelString string) (*pb.KernelInfo, string) {
	errString := ""
	kernelString = strings.TrimSpace(kernelString)
	// Add kernel information to the protobuf.
	pbKernelInfo := new(pb.KernelInfo)
	pbKernelInfo.KernelVersion = kernelString
	if ver, err := semver.New(kernelString); err != nil {
		errString += "Could not parse kernel version: " + err.Error()
	} else {
		pbKernelInfo.MajorVersion = ver.Major
		pbKernelInfo.MinorVersion = ver.Minor
		pbKernelInfo.PatchNumber = ver.Patch
		pbKernelInfo.PreRelease = fmt.Sprintf("%s", ver.Pre)
		pbKernelInfo.Build = strings.Join(ver.Build, "")
	}
	return pbKernelInfo, errString
}

// Helper function to get operating system information.
func getLinuxOSInfo(osString string) (*pb.HostInfo_LinuxOSInfo, string) {
	osInfoErrorString := ""
	pbLinuxOSInfo := new(pb.LinuxOSInfo)
	pbHostInfoLinuxOsInfo := new(pb.HostInfo_LinuxOSInfo)

	// Parse out the information from /etc/os-release.
	splitFunc := func(c rune) bool {
		return c == '\n'
	}
	osInfoStringArray := strings.FieldsFunc(osString, splitFunc)
	for _, item := range osInfoStringArray {
		osInfoItem := strings.Split(item, "=")
		switch strings.ToLower(osInfoItem[0]) {
		case "name":
			pbLinuxOSInfo.Name = osInfoItem[1]
		case "pretty_name":
			pbLinuxOSInfo.PrettyName = osInfoItem[1]
		case "version":
			pbLinuxOSInfo.Version = osInfoItem[1]
		case "id":
			pbLinuxOSInfo.ID = osInfoItem[1]
		case "version_id":
			pbLinuxOSInfo.VersionID = osInfoItem[1]
		case "build_id":
			pbLinuxOSInfo.BuildID = osInfoItem[1]
		case "id_like":
			pbLinuxOSInfo.IDLike = append(pbLinuxOSInfo.IDLike, osInfoItem[1])
		case "cpe_name":
			pbLinuxOSInfo.CpeName = osInfoItem[1]
		case "home_url":
			pbLinuxOSInfo.HomeUrl = osInfoItem[1]
		case "support_url":
			pbLinuxOSInfo.SupportUrl = osInfoItem[1]
		case "bug_report_url":
			pbLinuxOSInfo.BugReportUrl = osInfoItem[1]
		default:
			osInfoErrorString += "Unknown param for linux OS info.\n"
		}
	}
	pbHostInfoLinuxOsInfo.LinuxOSInfo = pbLinuxOSInfo
	return pbHostInfoLinuxOsInfo, osInfoErrorString
}
