package script

import (
	"fmt"

	"github.com/spf13/viper"
)

// ExecutableScript is the basic script entity that can be run.
type ExecutableScript struct {
	ScriptString string
	ScriptName   string
	ShortDoc     string
	LongDoc      string
	HasVis       bool
}

// LiveViewLink returns the fully qualified URL for the live view.
func (e ExecutableScript) LiveViewLink() string {
	if !e.HasVis {
		return ""
	}
	cloudAddr := viper.GetString("cloud_addr")
	if len(cloudAddr) == 0 {
		cloudAddr = "withpixie.ai"
	}

	return fmt.Sprintf("https://%s/live?script=%s", cloudAddr, e.ScriptName)
}
