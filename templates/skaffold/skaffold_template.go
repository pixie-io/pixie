package main

import (
	"fmt"
	"io/ioutil"
	"os"
	"path"
	"path/filepath"
	"text/template"

	"github.com/golang/protobuf/proto"
	log "github.com/sirupsen/logrus"
	pb "pixielabs.ai/pixielabs/templates/skaffold/proto"
)

func main() {
	var templateFiles = []string{}
	totPath := path.Join(os.Getenv("GOPATH"), "src/pixielabs.ai/pixielabs")

	// Find all the skaffold template files to generate service configs.
	err := filepath.Walk(totPath,
		func(path string, info os.FileInfo, err error) error {
			if err != nil {
				return err
			}
			if info.IsDir() && info.Name() == "vendor" {
				return filepath.SkipDir
			}
			if filepath.Ext(path) == ".tmpl" {
				templateFiles = append(templateFiles, path)

			}
			return nil
		})
	if err != nil {
		log.Println(err)
	}

	// Read the dev and prod config protobuf text files.
	devConfigFile := path.Join(totPath, "templates/skaffold/skaffold_service_config_dev.pbtxt")
	prodConfigFile := path.Join(totPath, "templates/skaffold/skaffold_service_config_prod.pbtxt")
	configFiles := [2]string{devConfigFile, prodConfigFile}

	for _, config := range configFiles {
		// Read the config file and generate a protobuf config message.
		configBuf, err := ioutil.ReadFile(config)
		if err != nil {
			log.WithError(err).Fatalf("Cannot read config file: " + config)
		}
		pbServiceConfig := new(pb.ServiceConfig)
		if err := proto.UnmarshalText(string(configBuf[:]), pbServiceConfig); err != nil {
			log.WithError(err).Fatalf("Cannot Unmarshal protobuf.")
		}
		// Generate all the service configs.
		for _, templateFile := range templateFiles {
			ext := path.Ext(templateFile)
			yamlOutputFile := fmt.Sprintf("%s_%s.yaml", templateFile[0:len(templateFile)-len(ext)], pbServiceConfig.Deployment)
			yamlOut, err := os.Create(yamlOutputFile)
			if err != nil {
				log.WithError(err).Fatalf("Could not create " + yamlOutputFile)
			}
			Skaffoldtmpl := template.Must(template.ParseFiles(templateFile))
			if err := Skaffoldtmpl.Execute(yamlOut, pbServiceConfig); err != nil {
				log.WithError(err).Fatalf("Could not create service yaml for skaffold (template error)")
			}
			if err := yamlOut.Close(); err != nil {
				log.WithError(err).Fatalf("Cannot close output file: " + yamlOutputFile)
			}
		}
	}
}
