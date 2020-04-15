package script

// Metadata has metadata information about a specific script.
type Metadata struct {
	ScriptName string
	ShortDoc   string
	LongDoc    string
	HasVis     bool
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
