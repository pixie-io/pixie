package main

import (
	"bufio"
	"bytes"
	"fmt"
	"io/ioutil"
	"os"
	"os/exec"
	"text/template"
	"time"

	"github.com/golang/protobuf/proto"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	pb "pixielabs.ai/pixielabs/demos/load_generation/proto"
	"pixielabs.ai/pixielabs/src/utils"
)

type userClassesInfo struct {
	Users string
}

func init() {
	// Configure flags and defaults.
	pflag.String("config_file", "", "The load config file.")
	pflag.String("host", "", "The host that should be load tested.")
	pflag.String("run_time", "", "Duration in seconds to run locust load generation.")
}

// parseArgs parses the arguments into specified variables.
func parseArgs() {
	pflag.Parse()

	// Must call after all flags are setup.
	viper.AutomaticEnv()
	viper.SetEnvPrefix("PL")
	viper.BindPFlags(pflag.CommandLine)

	// Check the args.
	if viper.GetString("config_file") == "" {
		log.Fatal("Flag --config_file is required. Please specify config_file as an argument. Refer to the usage menu (-h) for more help.")
	}

	if viper.GetString("host") == "" {
		log.Fatal("Flag --host is required. Please specify hsot as an argument. Refer to the usage menu (-h) for more help.")
	}

	if viper.GetString("run_time") == "" {
		log.Fatal("Flag --run_time is required. Specify it as a positive integer followed by units. For example, 100s")
	}
}

// mustLoadConfig loads the load generation config.
func mustLoadConfig() *pb.LocustConfigFile {
	configFile := viper.GetString("config_file")

	// Read the config file and generate a protobuf config message.
	configBuf, err := ioutil.ReadFile(configFile)
	if err != nil {
		log.WithError(err).WithField("config_file", configFile).Fatal("Cannot read config file.")
	}

	pbLocustConfig := new(pb.LocustConfigFile)
	if err := proto.UnmarshalText(string(configBuf[:]), pbLocustConfig); err != nil {
		log.WithError(err).Fatal("Cannot Unmarshal protobuf.")
	}

	return pbLocustConfig
}

// mustMakeLocustFile creates the locustFile for a given phase. The user class definitions in the locustFile can differ
// between every phase.
func mustMakeLocustFile(phase pb.LocustPhaseConfig) string {
	// Location where the generated locustFile should be saved. This file is ephemeral.
	locustFilePath := "locustfile.py"

	// Location of the locust file template.
	locustFileTmpl := "locustfile.loadgen.tmpl"
	// Location of the locust user class definition template.
	userTmplPath := "locustuser.tmpl"

	// Create class definitions for each locust user in the phase.
	var userClasses bytes.Buffer
	w := bufio.NewWriter(&userClasses)
	for _, user := range phase.UserTypes {
		log.WithField("user_name", user.Name).Info("Creating class for user.")
		userTmpl := template.Must(template.ParseFiles(userTmplPath))
		if err := userTmpl.Execute(w, user); err != nil {
			log.WithError(err).WithField("user_name", user.Name).Fatal("Could not create user class for user.")
		}
	}
	w.Flush()

	// Append the class definitions to the locustFile.
	fileOut, err := os.Create(locustFilePath)
	if err != nil {
		log.WithError(err).WithField("locust_file_path", locustFilePath).Fatal("Could not create locust file.")
	}
	locustTmpl := template.Must(template.ParseFiles(locustFileTmpl))

	if err := locustTmpl.Execute(fileOut, userClassesInfo{userClasses.String()}); err != nil {
		log.WithError(err).Fatal("Could not create locustFile (template error)")
	}

	if err := fileOut.Close(); err != nil {
		log.WithError(err).WithField("locust_file_path", locustFilePath).Fatal("Cannot close output file.")
	}
	return locustFilePath
}

// createLocustCmd constructs the command to be run during a given phase.
func createLocustCmd(host, locustFile string, phase pb.LocustPhaseConfig) *exec.Cmd {
	locustCmd := fmt.Sprintf("locust --host=%s --no-web -c %v -r %v -t %vs -f %s", host, phase.NumUsers,
		phase.HatchRatePerS, phase.Duration, locustFile)
	log.WithField("locust_cmd", locustCmd).Info("Running locust command.")
	return utils.MakeCommand(locustCmd)
}

func main() {
	parseArgs()

	loadConfig := mustLoadConfig()
	host := viper.GetString("host")
	runTime := viper.GetString("run_time")
	testDuration, err := time.ParseDuration(runTime)
	if err != nil {
		log.WithError(err).WithField("run_time", runTime).Fatal("Cannot parse test duration.")
	}

	startTime := time.Now()
	stopTime := startTime.Add(testDuration)

	for time.Until(stopTime).Seconds() > 0 {
		for i, phase := range loadConfig.Phases {
			if time.Until(stopTime).Seconds() < 0 {
				break
			}
			log.WithField("phase", i).Info("Running locust phase.")

			// Create locust file for the phase.
			locustFile := mustMakeLocustFile(*phase)

			// Run locust command.
			cmd := createLocustCmd(host, locustFile, *phase)
			err := utils.RunCmd(cmd)
			if err != nil {
				log.WithError(err).WithField("phase", i).Fatal("Error on running locust phase.")
			}
		}
	}
}
