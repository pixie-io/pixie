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
	pflag.String("input_doc_pb", "", "The file that holds a serialized protobuf")
	pflag.String("output_json", "output.json", "The file to write output JSON to")
}

func main() {
	services.PostFlagSetupAndParse()

	// Read input doc file.
	allDoc := &docspb.InternalPXLDocs{}

	// ReadFile fails if the input doc doesn't exist.
	b, err := ioutil.ReadFile(viper.GetString("input_doc_pb"))
	if err != nil {
		logrus.Fatal(err)
	}

	err = proto.UnmarshalText(string(b), allDoc)
	if err != nil {
		logrus.Fatal(err)
	}

	newDocs, err := docstring.ParseAllDocStrings(allDoc)
	if err != nil {
		logrus.Fatal(err)
	}

	outputF, err := os.Create(viper.GetString("output_json"))
	if err != nil {
		logrus.Fatal(err)
	}
	defer outputF.Close()

	// Write output JSON to file.
	m := jsonpb.Marshaler{}
	err = m.Marshal(outputF, newDocs)
	if err != nil {
		logrus.Fatal(err)
	}
}
