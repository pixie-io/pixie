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

package main

import (
	"encoding/json"
	"io/ioutil"
	"os"

	"github.com/gogo/protobuf/jsonpb"
	"github.com/gogo/protobuf/proto"
	"github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"

	"px.dev/pixie/src/carnot/docspb"
	docstring "px.dev/pixie/src/carnot/docstring/pkg"
	"px.dev/pixie/src/shared/services"
)

func init() {
	pflag.String("input_doc_pb", "", "The file that holds Pxl docs serialized as protobuf")
	pflag.String("py_api_docs", "", "The file to write output JSON to")
	pflag.String("output_json", "output.json", "The file to write output JSON to")
}

func main() {
	services.PostFlagSetupAndParse()

	// Read the raw pxlDocs File.
	pxlDocs := &docspb.InternalPXLDocs{}
	pxlB, err := ioutil.ReadFile(viper.GetString("input_doc_pb"))
	if err != nil {
		logrus.Fatal(err)
	}
	err = proto.UnmarshalText(string(pxlB), pxlDocs)
	if err != nil {
		logrus.Fatal(err)
	}

	// Parse and format the docstrings.
	formattedPxlDocs, err := docstring.ParseAllDocStrings(pxlDocs)
	if err != nil {
		logrus.Fatal(err)
	}

	// Read in the python api doc file.
	var pyAPI json.RawMessage
	pyB, err := ioutil.ReadFile(viper.GetString("py_api_docs"))
	if err != nil {
		logrus.Fatal(err)
	}
	err = json.Unmarshal(pyB, &pyAPI)
	if err != nil {
		logrus.Fatal(err)
	}

	// First marshal the protobuf to a json format. The
	// protobuf marshaller handles enums and nesting nicely.
	m := jsonpb.Marshaler{}
	pxlDocsString, err := m.MarshalToString(formattedPxlDocs)
	if err != nil {
		logrus.Fatal(err)
	}

	// Next, unmarshal the protobuf into a raw message.
	var pxlJSON map[string]*json.RawMessage
	if err := json.Unmarshal([]byte(pxlDocsString), &pxlJSON); err != nil {
		logrus.Fatal(err)
	}

	// Add on the pyApiDocs.
	pxlJSON["pyApiDocs"] = &pyAPI

	// Finally, Marshal out the full structure.
	outb, err := json.MarshalIndent(&pxlJSON, "", "  ")
	if err != nil {
		logrus.Fatal(err)
	}

	// Write out the file.
	outputF, err := os.Create(viper.GetString("output_json"))
	if err != nil {
		logrus.Fatal(err)
	}
	defer outputF.Close()

	_, err = outputF.Write(outb)
	if err != nil {
		logrus.Fatal(err)
	}
}
