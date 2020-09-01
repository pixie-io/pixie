package script

import (
	"flag"
	"fmt"
	"net/url"

	"github.com/gogo/protobuf/jsonpb"
	"github.com/spf13/viper"
	vispb "pixielabs.ai/pixielabs/src/shared/vispb"
)

// Arg is a single script argument.
type Arg struct {
	Name  string
	Value string
}

// ExecutableScript is the basic script entity that can be run.
type ExecutableScript struct {
	ScriptString string
	ScriptName   string
	ShortDoc     string
	LongDoc      string
	Vis          *vispb.Vis
	OrgName      string
	Hidden       bool
	// Args contains a map from name to argument info.
	Args map[string]Arg
}

// LiveViewLink returns the fully qualified URL for the live view.
func (e ExecutableScript) LiveViewLink(clusterID *string) string {
	if e.Vis == nil {
		return ""
	}
	cloudAddr := viper.GetString("cloud_addr")
	if len(cloudAddr) == 0 {
		cloudAddr = "withpixie.ai"
	}

	urlPath := "/live/script"
	if clusterID != nil {
		// url.URL automatically escapes the path.
		urlPath = fmt.Sprintf("/live/clusters/%s/script", *clusterID)
	}

	u := url.URL{
		Scheme: "https",
		Host:   cloudAddr,
		Path:   urlPath,
	}

	q := u.Query()
	q.Add("script", e.ScriptName)
	args := e.ComputedArgs()
	for _, arg := range args {
		q.Add(arg.Name, arg.Value)
	}
	u.RawQuery = q.Encode()

	return u.String()
}

// parses the spec return nil on failure.
func parseVisSpec(specJSON string) *vispb.Vis {
	var pb vispb.Vis
	if err := jsonpb.UnmarshalString(specJSON, &pb); err != nil {
		return nil
	}
	return &pb
}

// UpdateFlags updates the flags based on the passed in flag set.
func (e *ExecutableScript) UpdateFlags(fs *flag.FlagSet) {
	if e.Args == nil {
		e.Args = make(map[string]Arg, 0)
	}
	if e.Vis == nil {
		return
	}
	if len(e.Vis.Variables) == 0 {
		return
	}

	for _, v := range e.Vis.Variables {
		f := fs.Lookup(v.Name)
		if f == nil {
			e.Args[v.Name] = Arg{v.Name, v.DefaultValue}
		} else {
			e.Args[v.Name] = Arg{v.Name, f.Value.String()}
		}
	}
}

// ComputedArgs returns the args with defaults computed.
func (e *ExecutableScript) ComputedArgs() []Arg {
	args := make([]Arg, 0)
	if e.Vis == nil || len(e.Vis.Variables) == 0 {
		return args
	}
	for _, v := range e.Vis.Variables {
		arg, ok := e.Args[v.Name]
		if ok {
			args = append(args, arg)
		} else {
			args = append(args, Arg{v.Name, v.DefaultValue})
		}
	}
	return args
}

// GetFlagSet returns the flagset for this script based on the variables.
func (e *ExecutableScript) GetFlagSet() *flag.FlagSet {
	if e.Vis == nil || len(e.Vis.Variables) == 0 {
		return nil
	}
	fs := flag.NewFlagSet(e.ScriptName, flag.ContinueOnError)
	for _, v := range e.Vis.Variables {
		fs.String(v.Name, v.DefaultValue, fmt.Sprintf("Type: %s", v.Type))
	}
	return fs
}
