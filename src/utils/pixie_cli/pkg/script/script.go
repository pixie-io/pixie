package script

import (
	"fmt"

	"github.com/gogo/protobuf/jsonpb"
	"github.com/spf13/viper"
	vispb "pixielabs.ai/pixielabs/src/shared/vispb"
)

// ExecutableScript is the basic script entity that can be run.
type ExecutableScript struct {
	ScriptString string
	ScriptName   string
	ShortDoc     string
	LongDoc      string
	Vis          *vispb.Vis
}

// LiveViewLink returns the fully qualified URL for the live view.
func (e ExecutableScript) LiveViewLink() string {
	if e.Vis == nil {
		return ""
	}
	cloudAddr := viper.GetString("cloud_addr")
	if len(cloudAddr) == 0 {
		cloudAddr = "withpixie.ai"
	}

	return fmt.Sprintf("https://%s/live?script=%s", cloudAddr, e.ScriptName)
}

// parses the spec return nil on failure.
func parseVisSpec(specJSON string) *vispb.Vis {
	var pb vispb.Vis
	if err := jsonpb.UnmarshalString(specJSON, &pb); err != nil {
		return nil
	}
	return &pb
}
