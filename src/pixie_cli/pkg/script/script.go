package script

import (
	"fmt"
	"io"
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
	// Marks if this script is local rather than hosted.
	IsLocal bool
	// Args contains a map from name to argument info.
	Args map[string]Arg
}

// LiveViewLink returns the fully qualified URL for the live view.
func (e ExecutableScript) LiveViewLink(clusterID *string) string {
	if e.Vis == nil || e.IsLocal {
		return ""
	}
	cloudAddr := viper.GetString("cloud_addr")
	if len(cloudAddr) == 0 {
		cloudAddr = "withpixie.ai"
	}

	urlPath := "/live/script"
	if clusterID != nil {
		urlPath = fmt.Sprintf("/live/clusters/%s/script", url.PathEscape(*clusterID))
	}

	u := url.URL{
		Scheme: "https",
		Host:   cloudAddr,
		Path:   urlPath,
	}

	q := u.Query()
	q.Add("script", e.ScriptName)
	for _, arg := range e.Args {
		q.Add(arg.Name, arg.Value)
	}
	u.RawQuery = q.Encode()

	return u.String()
}

// ParseVisSpec parses the spec return nil on failure.
func ParseVisSpec(specJSON string) (*vispb.Vis, error) {
	var pb vispb.Vis
	if err := jsonpb.UnmarshalString(specJSON, &pb); err != nil && err != io.EOF {
		return nil, err
	}
	return &pb, nil
}

// UpdateFlags updates the flags based on the passed in flag set.
func (e *ExecutableScript) UpdateFlags(fs *FlagSet) error {
	if e.Args == nil {
		e.Args = make(map[string]Arg, 0)
	}
	if e.Vis == nil {
		return nil
	}
	if len(e.Vis.Variables) == 0 {
		return nil
	}

	for _, v := range e.Vis.Variables {
		val, err := fs.Lookup(v.Name)
		if err != nil {
			return err
		}
		e.Args[v.Name] = Arg{v.Name, val}
	}
	return nil
}

// ComputedArgs returns the args as an array.
func (e *ExecutableScript) ComputedArgs() ([]Arg, error) {
	args := make([]Arg, 0)
	if e.Vis == nil || len(e.Vis.Variables) == 0 {
		return args, nil
	}
	for _, v := range e.Vis.Variables {
		arg, ok := e.Args[v.Name]
		if !ok {
			return nil, fmt.Errorf("Argument %s not found", v.Name)
		}
		args = append(args, arg)
	}
	return args, nil
}

// GetFlagSet returns the flagset for this script based on the variables.
func (e *ExecutableScript) GetFlagSet() *FlagSet {
	if e.Vis == nil || len(e.Vis.Variables) == 0 {
		return nil
	}
	fs := NewFlagSet(e.ScriptName)
	for _, v := range e.Vis.Variables {
		var defaultValue *string
		if v.DefaultValue != nil {
			defaultValue = &v.DefaultValue.Value
		}
		fs.String(v.Name, defaultValue, fmt.Sprintf("Type: %s", v.Type))
	}
	return fs
}
