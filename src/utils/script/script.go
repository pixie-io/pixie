/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package script

import (
	"fmt"
	"io"
	"net/url"
	"strings"

	"github.com/gogo/protobuf/jsonpb"
	"github.com/spf13/viper"

	"px.dev/pixie/src/api/proto/vispb"
)

var jsonUnmashaler = &jsonpb.Unmarshaler{
	AllowUnknownFields: true,
}

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
	OrgID        string
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

	urlPath := "/live"
	if clusterID != nil {
		urlPath = fmt.Sprintf("/live/clusters/%s", url.PathEscape(*clusterID))
	}

	u := url.URL{
		Scheme: "https",
		Host:   fmt.Sprintf("work.%s", cloudAddr),
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
	err := jsonUnmashaler.Unmarshal(strings.NewReader(specJSON), &pb)
	if err != nil && err != io.EOF {
		return nil, err
	}
	return &pb, nil
}

// UpdateFlags updates the flags based on the passed in flag set.
func (e *ExecutableScript) UpdateFlags(fs *FlagSet) error {
	if e.Args == nil {
		e.Args = make(map[string]Arg)
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
