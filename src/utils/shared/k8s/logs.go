package k8s

import (
	"archive/zip"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"strings"

	log "github.com/sirupsen/logrus"
	v12 "k8s.io/api/core/v1"
	v1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"
)

// LogCollector collect logs for Pixie and cluster setup information.
type LogCollector struct {
	k8sConfig    *rest.Config
	k8sClientSet *kubernetes.Clientset
	ns           string
}

// NewLogCollector creates a new log collector.
func NewLogCollector(ns string) *LogCollector {
	cfg := GetConfig()
	cs := GetClientset(cfg)
	return &LogCollector{
		k8sConfig:    cfg,
		k8sClientSet: cs,
		ns:           ns,
	}
}

func fileNameFromParams(podName string, containerName string, prev bool) string {
	suffix := "log"
	if prev {
		suffix = "prev.log"
	}
	return fmt.Sprintf("%s__%s.%s", podName, containerName, suffix)
}

func (c *LogCollector) logPodInfoToZipFile(zf *zip.Writer, pod v12.Pod, containerName string, prev bool) error {
	fName := fileNameFromParams(pod.Name, containerName, prev)
	w, err := zf.Create(fName)
	if err != nil {
		return err
	}
	defer zf.Flush()

	logOpts := &v12.PodLogOptions{
		Container: containerName,
		Previous:  prev,
	}
	req := c.k8sClientSet.CoreV1().Pods(pod.Namespace).GetLogs(pod.Name, logOpts)
	podLogs, err := req.Stream(context.Background())
	if err != nil {
		return err
	}
	defer podLogs.Close()
	io.Copy(w, podLogs)
	return nil
}

func (c *LogCollector) logKubeCmd(zf *zip.Writer, fName string, arg ...string) error {
	cmd := exec.Command("kubectl", arg...)
	w, err := zf.Create(fName)
	defer zf.Flush()

	if err != nil {
		return err
	}

	stdoutPipe, err := cmd.StdoutPipe()
	if err != nil {
		return err
	}

	cmd.Start()
	go io.Copy(w, stdoutPipe)
	return cmd.Wait()
}

func (c *LogCollector) writePodDescription(zf *zip.Writer, pod v12.Pod) error {
	w, err := zf.Create(fmt.Sprintf("pod_%s_describe.json", pod.Name))
	defer zf.Flush()

	if err != nil {
		return err
	}

	enc := json.NewEncoder(w)
	return enc.Encode(pod)
}

// CollectPixieLogs collects logs for all Pixie pods and write them to the zip file fName.
func (c *LogCollector) CollectPixieLogs(fName string) error {
	if !strings.HasSuffix(fName, ".zip") {
		return errors.New("fname must have .zip suffix")
	}

	f, err := os.Create(fName)
	if err != nil {
		return err
	}
	defer f.Close()

	zf := zip.NewWriter(f)
	defer zf.Close()

	pods, err := c.k8sClientSet.CoreV1().Pods(c.ns).List(context.Background(), v1.ListOptions{})
	if err != nil {
		return err
	}

	for _, pod := range pods.Items {
		for _, containerStatus := range pod.Status.ContainerStatuses {

			// Ignore prev logs, they might not exist.
			_ = c.logPodInfoToZipFile(zf, pod, containerStatus.Name, true)

			err := c.logPodInfoToZipFile(zf, pod, containerStatus.Name, false)
			if err != nil {
				log.WithError(err).Warnf("Failed to log pod: %s", pod.Name)
			}
		}
		err = c.writePodDescription(zf, pod)
		if err != nil {
			log.WithError(err).Warnf("failed to write pod description")
		}
	}

	err = c.logKubeCmd(zf, "nodes.log", "describe", "node")
	if err != nil {
		log.WithError(err).Warn("failed to log node info")
	}

	err = c.logKubeCmd(zf, "services.log", "-n", c.ns, "get", "services", "-o", "wide")
	if err != nil {
		log.WithError(err).Warn("failed to log services")
	}

	return nil
}
