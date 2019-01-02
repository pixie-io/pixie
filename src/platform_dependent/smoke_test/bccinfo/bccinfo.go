package bccinfo

import (
	"io/ioutil"
	"os"
	"regexp"

	"github.com/golang/protobuf/proto"
	log "github.com/sirupsen/logrus"
	pb "pixielabs.ai/pixielabs/src/platform_dependent/smoke_test/proto"
)

// testCPUOutput checks the output string to make sure it contains cpu usage.
func testCPUOutput(bccOutput string, pbBccCheckInfo *pb.BCCCheckInfo) {
	r := regexp.MustCompile(`CPU\s+[0-9]`)
	pbBccCheckInfo.TestCPUDistribution = pb.FAIL
	if r.MatchString(bccOutput) {
		pbBccCheckInfo.TestCPUDistribution = pb.PASS
	}
}

// RunChecker runs the handler.
func RunChecker(outFH *os.File, bccFile string) {
	// read in the file.
	bccOutput, err := ioutil.ReadFile(bccFile)
	if err != nil {
		log.WithError(err).Fatalf("Couldn't read bcc output file %s", bccFile)
	}

	// pass string to contains CPUOutput.
	pbBccCheckInfo := new(pb.BCCCheckInfo)
	pbBccCheckInfo.Status = pb.PASS
	testCPUOutput(string(bccOutput), pbBccCheckInfo)

	// Write the test results to an output file.
	if _, err := outFH.WriteString(proto.MarshalTextString(pbBccCheckInfo)); err != nil {
		log.WithError(err).Fatal("Cannot write to output file")
	}

}

// DefaultChecker just marks the test as did not run and fails the BCC test.
func DefaultChecker(outFH *os.File) {
	// read in the file.
	// pass string to contains CPUOutput.
	pbBccCheckInfo := new(pb.BCCCheckInfo)
	pbBccCheckInfo.Status = pb.FAIL
	pbBccCheckInfo.TestCPUDistribution = pb.FAIL

	// Write the test results to an output file.
	if _, err := outFH.WriteString(proto.MarshalTextString(pbBccCheckInfo)); err != nil {
		log.WithError(err).Fatal("Cannot write to output file")
	}

}
