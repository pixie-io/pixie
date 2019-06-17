package main

import (
	"bufio"
	"io"
	"io/ioutil"
	"os"
	"strings"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
)

// Preprocess replaces any #include "..." with actual header code.
// It checks that the header included in given by Bazel in the sandbox, and that the bpf header doesn't include any
// more references to other user defined headers.
// There will be a line number mismatch between the stack produced at run time and the actual source code after
// the header expansion.
func Preprocess(file io.Reader, headers []string) string {
	fileContent := ""
	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		text := scanner.Text()
		if strings.HasPrefix(text, "#include \"") {
			headerPath := strings.Split(text, "\"")[1]
			validInclude := false
			for _, includedPath := range headers {
				if includedPath == headerPath {
					validInclude = true
				}
			}
			if !validInclude {
				log.Fatalf("File %s is not included in bazel", headerPath)
			}
			headerByte, err := ioutil.ReadFile(headerPath)
			if err != nil {
				log.Fatal(err)
			}
			headerString := string(headerByte)
			if strings.Contains(headerString, "#include \"") {
				log.Fatal("Bpf header includes other user include headers")
			}
			fileContent += "//-----------------------------------\n"
			fileContent += headerString
			fileContent += "\n//-----------------------------------"
		} else {
			fileContent += text
		}
		fileContent += "\n"
	}
	return fileContent
}

func init() {
	pflag.String("input_file", "", "path to the input file")
	pflag.StringSlice("header_files", []string{}, "path to the corresponding headers")
	pflag.String("output_file", "", "path to the output file")
	pflag.Parse()
	viper.AutomaticEnv()
	viper.SetEnvPrefix("PL")
	err := viper.BindPFlags(pflag.CommandLine)
	if err != nil {
		log.WithError(err).Fatal("Viper failed to bind to pflags")
	}
}

func main() {
	inputFile := viper.GetString("input_file")
	if len(inputFile) == 0 {
		log.Error("Input file not specified")
	}

	headerFiles := viper.GetStringSlice("header_files")

	file, err := os.Open(inputFile)
	if err != nil {
		log.WithError(err).Fatal("Failed to read input file")
	}
	defer func() {
		err := file.Close()
		if err != nil {
			log.WithError(err).Fatal("Failed to close input file")
		}
	}()

	fileContent := Preprocess(file, headerFiles)
	err = ioutil.WriteFile(viper.GetString("output_file"), []byte(fileContent), os.FileMode(0755))
	if err != nil {
		log.WithError(err).Fatal("Failed to write to output file")
	}
}
