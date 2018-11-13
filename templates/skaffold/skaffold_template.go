package main

import (
	"fmt"
	"io/ioutil"
	"os"
	"path"
	"path/filepath"
	"strings"
	"text/template"

	"github.com/golang/protobuf/proto"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	pb "pixielabs.ai/pixielabs/templates/skaffold/proto"
	"pixielabs.ai/pixielabs/utils"
)

// init defines the args.
func init() {
	pflag.String("build_dir", "", "The build_dir in which to build the project")
	pflag.Bool("staging", false, "Flag to turn on staging. Do not use with --prod.")
	pflag.Bool("prod", false, "Flag to turn on prod. Do not use with --staging.")
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

	if viper.GetBool("prod") && viper.GetBool("staging") {
		log.Fatal("Flags --prod and --staging can only be used exclusively. Please remove one to specify the environment")

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
	var templateFiles = []string{}
	for _, curPath := range pathsToSearch {
		log.Infof("Searching %s", curPath)
		err := filepath.Walk(curPath,
			func(path string, info os.FileInfo, err error) error {
				if err != nil {
					return err
				}
				// Check whether path has the extension, which
				// might contain several "." delineated pieces.
				if strings.HasSuffix(path, ext) {
					log.Infof("Found template: %s\n", path)
					templateFiles = append(templateFiles, path)
				}
				return nil
			})
		if err != nil {
			log.WithError(err).Errorf("Error on findTemplateFiles while searching %s", curPath)
		}
	}
	return templateFiles
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

// startSkaffold starts the skaffold command.
func startSkaffold(buildDir, workspaceDir string) {
	skaffoldSubCmd := "dev"
	if viper.GetBool("prod") || viper.GetBool("staging") {
		skaffoldSubCmd = "run"
	}
	environ := getEnviron()
	skaffoldFile := path.Join(buildDir, fmt.Sprintf("skaffold/skaffold_%s.yaml", environ))
	if utils.FileExists(skaffoldFile) {
		skaffoldCmd := fmt.Sprintf("skaffold %s -f %s", skaffoldSubCmd, skaffoldFile)
		log.WithField("cmd", skaffoldCmd).Info("Starting Skaffold")
		cmd := utils.MakeCommand(skaffoldCmd)
		os.Chdir(workspaceDir)
		err := utils.RunCmd(cmd)
		if err != nil {
			log.WithError(err).Fatalf("Error starting skaffold with cmd \"%s\".", skaffoldCmd)
		}
	} else {
		log.Fatalf("Can't find skaffold file %s. Skaffold cannot run", skaffoldFile)
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
	servicesDir := path.Join(totPath, "services")
	skaffoldDir := path.Join(totPath, "skaffold")
	dirsToTemplate := []string{skaffoldDir, servicesDir}

	// Extension
	ext := ".skfld.tmpl"
	templateFiles := findTemplateFiles(dirsToTemplate, ext)

	// Get the config file.
	configEnviron := getEnviron()
	configFile := path.Join(totPath, fmt.Sprintf("templates/skaffold/skaffold_service_config_%s.pbtxt", configEnviron))
	config := loadConfig(configFile, buildDir)

	// Generate all the service config from templateFiles.
	generateServiceConfigs(templateFiles, buildDir, totPath, ext, config)

	// Get the skaffold YAML template.
	startSkaffold(buildDir, totPath)
}
