package main

import (
	"io/ioutil"
	"os"

	"github.com/gogo/protobuf/jsonpb"
	"github.com/gogo/protobuf/proto"
	"github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"

	"pixielabs.ai/pixielabs/src/carnot/docspb"
	docstring "pixielabs.ai/pixielabs/src/carnot/docstring/pkg"
	"pixielabs.ai/pixielabs/src/shared/services"
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
