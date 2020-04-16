package script

import (
	"fmt"

	"github.com/spf13/viper"
)

// Metadata has metadata information about a specific script.
type Metadata struct {
	ScriptName string
	ShortDoc   string
	LongDoc    string
	HasVis     bool
}

// LiveViewLink returns the fully qualified URL for the live view.
func (m Metadata) LiveViewLink() string {
	if !m.HasVis {
		return ""
	}
	cloudAddr := viper.GetString("cloud_addr")
	if len(cloudAddr) == 0 {
		cloudAddr = "withpixie.ai"
	}

	return fmt.Sprintf("https://%s/live?script=%s", cloudAddr, m.ScriptName)
}

// ExecutableScript is the basic script entity that can be run.
type ExecutableScript struct {
	metadata     Metadata
	scriptString string
}

// NewExecutableScript creates a script based on metadata data and the script string.
func NewExecutableScript(md Metadata, s string) *ExecutableScript {
	return &ExecutableScript{
		metadata:     md,
		scriptString: s,
	}
}

// Metadata returns the script metadata.
func (e *ExecutableScript) Metadata() Metadata {
	return e.metadata
}

// ScriptString returns the raw script string.
func (e *ExecutableScript) ScriptString() string {
	return e.scriptString
}
