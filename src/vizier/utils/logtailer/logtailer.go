package main

import (
	"io"
	"io/ioutil"
	"os"
	"path"
	"strings"
	"time"

	"github.com/hpcloud/tail"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"

	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/vizier/utils/logtailer/logwriter"
)

const retryAttempts = 5
const retryDelay = 10 * time.Second

func init() {
	pflag.String("container_logs", "", "The path to the container logs directory")
	pflag.String("pod_name", "", "The name of the current pod")
	pflag.String("cloud_connector_addr", "vizier-cloud-connector.pl.svc:50800", "The address to the cloud connector")
}

func containerLogPaths(logdir string) ([]string, error) {
	files, err := ioutil.ReadDir(logdir)
	if err != nil {
		return nil, err
	}
	var outfiles []string
	for _, f := range files {
		// Filter out containers not belonging to the current pod.
		if !strings.Contains(f.Name(), viper.GetString("pod_name")) {
			continue
		}
		outfiles = append(outfiles, path.Join(logdir, f.Name()))
	}
	return outfiles, nil
}

func doLogTail(writer io.Writer, path string) {
	t, err := tail.TailFile(path, tail.Config{
		Follow:   true,
		Poll:     true,
		Location: &tail.SeekInfo{Offset: 0, Whence: os.SEEK_SET},
	})
	if err != nil {
		log.WithError(err).Fatalf("Failed to start tailing file: %s", path)
	}
	log.Infof("Tailing file: %s", path)
	for line := range t.Lines {
		writer.Write([]byte(line.Text))
	}
}

func main() {
	services.PostFlagSetupAndParse()

	podName := viper.GetString("pod_name")
	var svcName string

	// Pod names look like "vizier-api-65fc66d7-drjnq", so taking the 2nd element gets us the service.
	if splits := strings.Split(podName, "-"); len(splits) > 1 {
		svcName = splits[1]
	}

	var lwriter io.Writer

	for i := retryAttempts; i >= 0; i-- {
		writer, err := logwriter.SetupLogWriter(viper.GetString("cloud_connector_addr"), podName, svcName)
		if err == nil {
			lwriter = writer
			log.Infof("Connection to to %s succeeded", viper.GetString("cloud_connector_addr"))
			break
		}

		if i > 0 {
			log.WithError(err).Errorf("Could not connect to cloud connector for log forwarding, retries remaining: %d", i)
			time.Sleep(retryDelay)
		} else {
			log.WithError(err).Fatalf("Failed all retry attempts to connect to cloud connector")
		}
	}

	pathsSet := map[string]struct{}{}

	for range time.Tick(time.Second) {
		paths, err := containerLogPaths(viper.GetString("container_logs"))
		if err != nil {
			log.WithError(err).Fatalf("Could not get paths for container logs to tail in directory %s", viper.GetString("container_logs"))
		}
		for _, path := range paths {
			if _, present := pathsSet[path]; !present {
				go doLogTail(lwriter, path)
				pathsSet[path] = struct{}{}
			}
		}
	}
}
