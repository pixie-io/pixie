package linuxinfo

import (
	linuxproc "github.com/c9s/goprocinfo/linux"
	"github.com/stretchr/testify/assert"
	pb "pixielabs.ai/pixielabs/src/platform_dependent/smoke_test/proto"
	"testing"
)

func TestConvertCPUInfoToProto(t *testing.T) {
	assert := assert.New(t)

	// Setup CPUInfo struct.
	cpuInfo := new(linuxproc.CPUInfo)
	processor := linuxproc.Processor{}
	processor.Id = 1
	processor.Model = 158
	processor.VendorId = "GenuineIntel"
	processor.Flags = []string{"fpu", "vme"}
	processor.Cores = 4
	processor.MHz = 2903.998
	processor.CacheSize = 12288
	cpuInfo.Processors = append(cpuInfo.Processors, processor)

	// Test.
	pbCPUInfo := new(pb.CPUInfo)
	pbCPUInfo = convertCPUInfoToProto(cpuInfo)

	assert.Equal(pbCPUInfo.Processors[0].GetID(), processor.Id)
	assert.Equal(pbCPUInfo.Processors[0].GetModel(), processor.Model)
	assert.Equal(pbCPUInfo.Processors[0].GetVendorID(), processor.VendorId)
	assert.Equal(pbCPUInfo.Processors[0].GetFlags(), processor.Flags)
	assert.Equal(pbCPUInfo.Processors[0].GetCores(), processor.Cores)
	assert.Equal(pbCPUInfo.Processors[0].GetMHz(), processor.MHz)
	assert.Equal(pbCPUInfo.Processors[0].GetCacheSize(), processor.CacheSize)
}

func TestConvertMemInfoToProto(t *testing.T) {
	assert := assert.New(t)

	// Setup CPUInfo struct.
	memInfo := new(linuxproc.MemInfo)
	memInfo.MemTotal = 1234
	memInfo.MemFree = 456
	memInfo.MemAvailable = 789
	memInfo.Buffers = 123
	memInfo.Cached = 444
	memInfo.SwapCached = 555
	memInfo.Active = 666

	// Test.
	pbMemInfo := new(pb.MemInfo)
	pbMemInfo = convertMemInfoToProto(memInfo)
	assert.Equal(pbMemInfo.GetMemTotal(), memInfo.MemTotal)
	assert.Equal(pbMemInfo.GetMemFree(), memInfo.MemFree)
	assert.Equal(pbMemInfo.GetMemAvailable(), memInfo.MemAvailable)
	assert.Equal(pbMemInfo.GetBuffers(), memInfo.Buffers)
	assert.Equal(pbMemInfo.GetCached(), memInfo.Cached)
	assert.Equal(pbMemInfo.GetSwapCached(), memInfo.SwapCached)
	assert.Equal(pbMemInfo.GetActive(), memInfo.Active)
}

func TestGetLinuxKernelInfo(t *testing.T) {
	assert := assert.New(t)
	kernelString := "4.15.0-33-generic"

	// Test.
	pbKernelInfo := new(pb.KernelInfo)
	pbKernelInfo, err := getLinuxKernelInfo(kernelString)

	assert.Equal(err, "")
	assert.Equal(pbKernelInfo.GetKernelVersion(), kernelString)
	assert.Equal(pbKernelInfo.GetMajorVersion(), uint64(4))
	assert.Equal(pbKernelInfo.GetMinorVersion(), uint64(15))
	assert.Equal(pbKernelInfo.GetPatchNumber(), uint64(0))
	assert.Equal(pbKernelInfo.GetPreRelease(), "[33-generic]")
	assert.Equal(pbKernelInfo.GetBuild(), "")

}

func TestGetLinuxOSInfo(t *testing.T) {
	assert := assert.New(t)
	osString := "name=Ubuntu\n" +
		"pretty_name=Ubuntu 18.04.1 LTS\n" +
		"version=18.04.1 LTS (Bionic Beaver)\n" +
		"id=ubuntu\n" +
		"version_id=18.04\n" +
		"id_like=debian\n" +
		"home_url=https://www.ubuntu.com/\n" +
		"support_url=https://help.ubuntu.com/\n" +
		"bug_report_url=https://bugs.launchpad.net/ubuntu/\n"
	// Test.
	pbOSInfo := new(pb.HostInfo_LinuxOSInfo)
	pbOSInfo, err := getLinuxOSInfo(osString)

	assert.Equal(err, "")
	assert.Equal(pbOSInfo.LinuxOSInfo.GetName(), "Ubuntu")
	assert.Equal(pbOSInfo.LinuxOSInfo.GetPrettyName(), "Ubuntu 18.04.1 LTS")
	assert.Equal(pbOSInfo.LinuxOSInfo.GetVersion(), "18.04.1 LTS (Bionic Beaver)")
	assert.Equal(pbOSInfo.LinuxOSInfo.GetID(), "ubuntu")
	assert.Equal(pbOSInfo.LinuxOSInfo.GetIDLike(), []string{"debian"})
	assert.Equal(pbOSInfo.LinuxOSInfo.GetHomeUrl(), "https://www.ubuntu.com/")

}
