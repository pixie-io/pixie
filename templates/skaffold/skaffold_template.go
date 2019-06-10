package main

import (
	"fmt"
	"io/ioutil"
	"os"
	"path"
	"strings"
	"text/template"

	"github.com/bmatcuk/doublestar"
	"github.com/golang/protobuf/proto"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"pixielabs.ai/pixielabs/src/utils"
	pb "pixielabs.ai/pixielabs/templates/skaffold/proto"
)

// init defines the args.
func init() {
	pflag.String("build_dir", "", "The build_dir in which to build the project")
	pflag.String("build_type", "dev", "The type of the buid: dev, staging, prod")
}

// parseArgs parses the arguments into specified variables.
func parseArgs() {
	pflag.Parse()
	// Must call after all flags are setup.
	viper.AutomaticEnv()
	viper.SetEnvPrefix("PL")
	viper.BindPFlags(pflag.CommandLine)

	// Check the args.
	if viper.GetString("build_dir") == "" {
		log.Fatal("Flag --build_dir is required. Please specify build_dir as an argument. Refer to the usage menu (-h) for more help.")
	}
}

// getEnviron returns the environment string based on the current flags.
func getEnviron() string {
	environ := "dev"
	if viper.GetBool("prod") {
		environ = "prod"
		// TODO(philkuz) PL-64 make a script that emails everyone upon push to prod.
	} else if viper.GetBool("staging") {
		environ = "staging"
	}
	return environ
}

// findTemplateFiles searches for template files.
func findTemplateFiles(pathsToSearch []string, ext string) []string {
	var allTemplates []string
	for _, curPath := range pathsToSearch {
		log.Infof("Searching %s", curPath)
		globTemplate := curPath + "/**/*" + ext
		templateFiles, err := doublestar.Glob(globTemplate)
		if err != nil {
			log.WithError(err).Errorf("Error on findTemplateFiles while searching %s", curPath)
		}
		allTemplates = append(allTemplates, templateFiles...)
	}
	return allTemplates
}

// loadConfig loads the config from protobuf and adds another field, the BuildDir.
func loadConfig(configFile, buildDir string) serviceConfigExt {
	totPath, err := utils.FindBazelWorkspaceRoot()
	if err != nil {
		log.WithError(err).Fatalf("WORKSPACE directory could not be found")
	}
	localBuildDir := strings.Replace(buildDir, totPath, "", -1)

	// Read the config file and generate a protobuf config message.
	configBuf, err := ioutil.ReadFile(configFile)
	if err != nil {
		log.WithError(err).Fatalf("Cannot read config file: " + configFile)
	}

	pbServiceConfig := new(pb.ServiceConfig)
	if err := proto.UnmarshalText(string(configBuf[:]), pbServiceConfig); err != nil {
		log.WithError(err).Fatalf("Cannot Unmarshal protobuf.")
	}
	config := serviceConfigExt{
		ServiceConfig: *pbServiceConfig,
		BuildDir:      localBuildDir,
	}
	return config
}

// generateServiceConfigs takes the template files and substitutes the config details into the fields.
func generateServiceConfigs(templateFiles []string, buildDir, totPath, ext string, config serviceConfigExt) {
	for _, templateFile := range templateFiles {
		fileNoExt := strings.Replace(templateFile, ext, "", -1)
		yamlOutputFile := fmt.Sprintf("%s_%s.yaml", fileNoExt, config.Deployment)
		yamlOutputFile = path.Join(buildDir, strings.Replace(yamlOutputFile, totPath, "", -1))
		outputPath := path.Dir(yamlOutputFile)

		// create the subdirectory in the buildDir if it doesn't exist yet.
		os.MkdirAll(outputPath, os.ModePerm)

		yamlOut, err := os.Create(yamlOutputFile)
		if err != nil {
			log.WithError(err).Fatalf("Could not create " + yamlOutputFile)
		}
		Skaffoldtmpl := template.Must(template.ParseFiles(templateFile))
		// set the local build dir as the buildDir.
		if err := Skaffoldtmpl.Execute(yamlOut, config); err != nil {
			log.WithError(err).Fatalf("Could not create service yaml for skaffold (template error)")
		}
		if err := yamlOut.Close(); err != nil {
			log.WithError(err).Fatalf("Cannot close output file: " + yamlOutputFile)
		}
	}
}

type serviceConfigExt struct {
	pb.ServiceConfig
	BuildDir string
}

func main() {
	// handle script arguments.
	parseArgs()

	buildDir := viper.GetString("build_dir")

	totPath, err := utils.FindBazelWorkspaceRoot()
	if err != nil {
		log.WithError(err).Fatalf("WORKSPACE directory could not be found")
	}

	// make the buildDir.
	os.MkdirAll(buildDir, os.ModePerm)

	// Find all the skaffold template files to generate service configs.
	skaffoldDir := path.Join(totPath, "skaffold")
	dirsToTemplate := []string{skaffoldDir}

	// Remove the UI directory from the services search path, since
	// node_modules contains a lot of files making the template search slow.
	servicesDir := path.Join(totPath, "src")
	fileInfos, err := ioutil.ReadDir(servicesDir)
	if err != nil {
		log.WithError(err).Fatal("Failed to read directory info for services.")
	}

	for _, fileInfo := range fileInfos {
		if fileInfo.IsDir() && fileInfo.Name() != "ui" {
			dirsToTemplate = append(dirsToTemplate, path.Join(servicesDir, fileInfo.Name()))
		}
	}

	// Extension
	ext := ".skfld.tmpl"
	templateFiles := findTemplateFiles(dirsToTemplate, ext)

	// Get the config file.
	configEnviron := viper.GetString("build_type")
	configFile := path.Join(totPath, fmt.Sprintf("templates/skaffold/skaffold_service_config_%s.pbtxt", configEnviron))
	config := loadConfig(configFile, buildDir)

	// Generate all the service config from templateFiles.
	generateServiceConfigs(templateFiles, buildDir, totPath, ext, config)
}
