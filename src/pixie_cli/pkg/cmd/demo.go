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

package cmd

import (
	"archive/tar"
	"bytes"
	"compress/gzip"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"os"
	"strings"
	"time"

	"github.com/cenkalti/backoff/v4"
	"github.com/fatih/color"
	"github.com/segmentio/analytics-go/v3"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
	v1 "k8s.io/api/core/v1"
	k8s_errors "k8s.io/apimachinery/pkg/api/errors"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"

	"px.dev/pixie/src/pixie_cli/pkg/components"
	"px.dev/pixie/src/pixie_cli/pkg/pxanalytics"
	"px.dev/pixie/src/pixie_cli/pkg/pxconfig"
	"px.dev/pixie/src/pixie_cli/pkg/utils"
	"px.dev/pixie/src/utils/shared/k8s"
)

const manifestFile = "manifest.json"

var errNamespaceAlreadyExists = errors.New("namespace already exists")
var errCertMgrDoesNotExist = errors.New("cert-manager does not exist")

func init() {
	DemoCmd.PersistentFlags().String("artifacts", "https://storage.googleapis.com/pixie-prod-artifacts/prod-demo-apps", "The path to the demo apps")

	DemoCmd.AddCommand(interactDemoCmd)
	DemoCmd.AddCommand(listDemoCmd)
	DemoCmd.AddCommand(deployDemoCmd)
	DemoCmd.AddCommand(deleteDemoCmd)
}

// DemoCmd is the demo sub-command of the CLI to deploy and delete demo apps.
var DemoCmd = &cobra.Command{
	Use:   "demo",
	Short: "Manage demo apps",
	PersistentPreRun: func(cmd *cobra.Command, args []string) {
		// This pre run might be run from a subcommand. To bind the correct flag, we should check
		// the persistent flags on both the current command and the parent.
		if cmd.PersistentFlags().Lookup("artifacts") != nil {
			viper.BindPFlag("artifacts", cmd.PersistentFlags().Lookup("artifacts"))
			return
		}
		viper.BindPFlag("artifacts", cmd.Parent().PersistentFlags().Lookup("artifacts"))
	},
	Run: func(cmd *cobra.Command, args []string) {
		utils.Info("Nothing here... Please execute one of the subcommands")
		cmd.Help()
	},
}

var interactDemoCmd = &cobra.Command{
	Use:   "interact",
	Short: "Print instructions for interacting with demo post-deploy",
	Args:  cobra.ExactArgs(1),
	Run:   interactCmd,
	PreRun: func(cmd *cobra.Command, args []string) {
		pxanalytics.Client().Enqueue(&analytics.Track{
			UserId: pxconfig.Cfg().UniqueClientID,
			Event:  "Demo Print Interact Instructions",
		})
	},
	PostRun: func(cmd *cobra.Command, args []string) {
		pxanalytics.Client().Enqueue(&analytics.Track{
			UserId: pxconfig.Cfg().UniqueClientID,
			Event:  "Demo Print Interact Instructions Complete",
		})
	},
}

var listDemoCmd = &cobra.Command{
	Use:   "list",
	Short: "List available demo apps",
	Run:   listCmd,
	PreRun: func(cmd *cobra.Command, args []string) {
		pxanalytics.Client().Enqueue(&analytics.Track{
			UserId: pxconfig.Cfg().UniqueClientID,
			Event:  "Demo List Apps",
		})
	},
	PostRun: func(cmd *cobra.Command, args []string) {
		pxanalytics.Client().Enqueue(&analytics.Track{
			UserId: pxconfig.Cfg().UniqueClientID,
			Event:  "Demo List Apps Complete",
		})
	},
}

var deleteDemoCmd = &cobra.Command{
	Use:   "delete",
	Short: "Delete demo app",
	Args:  cobra.ExactArgs(1),
	Run:   deleteCmd,
	PreRun: func(cmd *cobra.Command, args []string) {
		pxanalytics.Client().Enqueue(&analytics.Track{
			UserId: pxconfig.Cfg().UniqueClientID,
			Event:  "Demo Delete App",
			Properties: analytics.NewProperties().
				Set("app", args[0]),
		})
	},
	PostRun: func(cmd *cobra.Command, args []string) {
		pxanalytics.Client().Enqueue(&analytics.Track{
			UserId: pxconfig.Cfg().UniqueClientID,
			Event:  "Demo Delete App Complete",
			Properties: analytics.NewProperties().
				Set("app", args[0]),
		})
	},
}

var deployDemoCmd = &cobra.Command{
	Use:   "deploy",
	Short: "Deploy demo app",
	Args:  cobra.ExactArgs(1),
	Run:   deployCmd,
	PreRun: func(cmd *cobra.Command, args []string) {
		pxanalytics.Client().Enqueue(&analytics.Track{
			UserId: pxconfig.Cfg().UniqueClientID,
			Event:  "Demo Deploy App",
			Properties: analytics.NewProperties().
				Set("app", args[0]),
		})
	},
	PostRun: func(cmd *cobra.Command, args []string) {
		defer pxanalytics.Client().Enqueue(&analytics.Track{
			UserId: pxconfig.Cfg().UniqueClientID,
			Event:  "Demo Deploy App Complete",
			Properties: analytics.NewProperties().
				Set("app", args[0]),
		})
	},
}

func interactCmd(cmd *cobra.Command, args []string) {
	appName := args[0]

	var err error
	defer func() {
		if err == nil {
			return
		}
		pxanalytics.Client().Enqueue(&analytics.Track{
			UserId: pxconfig.Cfg().UniqueClientID,
			Event:  "Demo Print Interact Instructions Error",
			Properties: analytics.NewProperties().
				Set("error", err.Error()),
		})
	}()

	manifest, err := downloadManifest(viper.GetString("artifacts"))
	if err != nil {
		// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
		log.WithError(err).Fatal("Could not download manifest file")
	}
	appSpec, ok := manifest[appName]
	// When a demo app is deprecated, its contents will be set to null in manifest.json.
	if !ok || appSpec == nil {
		utils.Fatalf("%s is not a supported demo app", appName)
	}
	instructions := strings.Join(appSpec.Instructions, "\n")

	p := func(s string, a ...interface{}) {
		fmt.Fprintf(os.Stderr, s, a...)
	}
	p(color.CyanString("Post-deploy instructions for %s demo app:\n\n", args[0]))
	p(instructions + "\n\n")
}

func listCmd(cmd *cobra.Command, args []string) {
	var err error
	defer func() {
		if err == nil {
			return
		}
		pxanalytics.Client().Enqueue(&analytics.Track{
			UserId: pxconfig.Cfg().UniqueClientID,
			Event:  "Demo List Apps Error",
			Properties: analytics.NewProperties().
				Set("error", err.Error()),
		})
	}()

	manifest, err := downloadManifest(viper.GetString("artifacts"))
	if err != nil {
		// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
		log.WithError(err).Fatal("Could not download manifest file")
	}

	w := components.CreateStreamWriter("table", os.Stdout)
	defer w.Finish()
	w.SetHeader("demo_list", []string{"Name", "Description"})
	for app, appSpec := range manifest {
		// When a demo app is deprecated, its contents will be set to null in manifest.json.
		if appSpec != nil {
			err = w.Write([]interface{}{app, appSpec.Description})
			if err != nil {
				log.WithError(err).Error("Failed to write demo app")
				continue
			}
		}
	}
}

func deleteCmd(cmd *cobra.Command, args []string) {
	appName := args[0]

	var err error
	defer func() {
		if err == nil {
			return
		}
		pxanalytics.Client().Enqueue(&analytics.Track{
			UserId: pxconfig.Cfg().UniqueClientID,
			Event:  "Demo Delete App Error",
			Properties: analytics.NewProperties().
				Set("app", appName).
				Set("error", err.Error()),
		})
	}()

	manifest, err := downloadManifest(viper.GetString("artifacts"))
	if err != nil {
		// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
		log.WithError(err).Fatal("Could not download manifest file")
	}
	if _, ok := manifest[appName]; !ok {
		utils.Fatalf("%s is not a supported demo app", appName)
	}

	kubeAPIConfig := k8s.GetClientAPIConfig()
	currentCluster := kubeAPIConfig.CurrentContext
	utils.Infof("Deleting demo app %s from the following cluster: %s", appName, currentCluster)
	clusterOk := components.YNPrompt("Is the cluster correct?", true)
	if !clusterOk {
		utils.Fatal("Cluster is not correct. Aborting.")
	}

	if !namespaceExists(appName) {
		utils.Fatalf("Namespace %s does not exist on cluster %s", appName, currentCluster)
	}

	if err = deleteDemoApp(appName); err != nil {
		// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
		log.WithError(err).Fatalf("Error deleting demo app %s from cluster %s", appName, currentCluster)
	} else {
		utils.Infof("Successfully deleted demo app %s from cluster %s", appName, currentCluster)
	}
}

func deployCmd(cmd *cobra.Command, args []string) {
	appName := args[0]

	var err error
	defer func() {
		if err == nil {
			return
		}
		pxanalytics.Client().Enqueue(&analytics.Track{
			UserId: pxconfig.Cfg().UniqueClientID,
			Event:  "Demo Deploy App Error",
			Properties: analytics.NewProperties().
				Set("app", appName).
				Set("error", err.Error()),
		})
	}()

	manifest, err := downloadManifest(viper.GetString("artifacts"))
	if err != nil {
		// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
		log.WithError(err).Fatal("Could not download manifest file")
	}

	appSpec, ok := manifest[appName]
	// When a demo app is deprecated, its contents will be set to null in manifest.json.
	if !ok || appSpec == nil {
		utils.Fatalf("%s is not a supported demo app", appName)
	}
	instructions := strings.Join(appSpec.Instructions, "\n")

	yamls, err := downloadDemoAppYAMLs(appName, viper.GetString("artifacts"))
	if err != nil {
		// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
		log.WithError(err).Fatalf("Could not download demo yaml apps for app '%s'", appName)
	}

	kubeAPIConfig := k8s.GetClientAPIConfig()
	currentCluster := kubeAPIConfig.CurrentContext
	utils.Infof("Deploying demo app %s to the following cluster: %s", appName, currentCluster)
	clusterOk := components.YNPrompt("Is the cluster correct?", true)
	if !clusterOk {
		utils.Error("Cluster is not correct. Aborting.")
		return
	}

	err = setupDemoApp(appName, yamls, appSpec.Dependencies)
	if err != nil {
		if errors.Is(err, errNamespaceAlreadyExists) {
			utils.Error("Failed to deploy demo application: namespace already exists.")
			return
		} else if errors.Is(err, errCertMgrDoesNotExist) {
			utils.Error("Failed to deploy demo application: cert-manager needs to be installed. To deploy, please follow instructions at https://cert-manager.io/docs/getting-started/")
			return
		}
		// Using log.Errorf rather than CLI log in order to track this unexpected error in Sentry.
		log.WithError(err).Errorf("Error deploying demo application, deleting namespace %s", appName)
		// Note: If you can specify the namespace for the demo app in the future, we shouldn't delete the namespace.
		if err = deleteDemoApp(appName); err != nil {
			// Using log.Errorf rather than CLI log in order to track this unexpected error in Sentry.
			log.WithError(err).Errorf("Error deleting namespace %s", appName)
		}
		utils.Fatal("Failed to deploy demo application.")
	}

	utils.Infof("Successfully deployed demo app %s to cluster %s.", args[0], currentCluster)

	p := func(s string, a ...interface{}) {
		fmt.Fprintf(os.Stderr, s, a...)
	}
	b := color.New(color.Bold)
	p(color.CyanString("==> ") + b.Sprint("Next Steps:\n\n"))
	p(instructions)
}

type manifestAppSpec struct {
	Description  string          `json:"description"`
	Instructions []string        `json:"instructions"`
	Dependencies map[string]bool `json:"dependencies"`
}

type manifest = map[string]*manifestAppSpec

func downloadGCSFileFromHTTP(dirURL, filename string) ([]byte, error) {
	// Get the data
	resp, err := http.Get(fmt.Sprintf("%s/%s", dirURL, filename))
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	return io.ReadAll(resp.Body)
}

func downloadManifest(artifacts string) (manifest, error) {
	jsonBytes, err := downloadGCSFileFromHTTP(artifacts, manifestFile)
	if err != nil {
		return nil, err
	}

	jsonManifest := make(manifest)
	err = json.Unmarshal(jsonBytes, &jsonManifest)
	if err != nil {
		return nil, err
	}
	return jsonManifest, nil
}

func deleteDemoApp(appName string) error {
	deleteDemo := []utils.Task{
		newTaskWrapper(fmt.Sprintf("Deleting demo app %s", appName), func() error {
			kubeConfig := k8s.GetConfig()
			clientset := k8s.GetClientset(kubeConfig)

			// Resources labeled as "pixie-demo-initial-cleanup" should be cleaned up first.
			od := k8s.ObjectDeleter{
				Clientset:  clientset,
				RestConfig: kubeConfig,
				Timeout:    2 * time.Minute,
			}

			_, err := od.DeleteByLabel(fmt.Sprintf("pixie-demo-initial-cleanup=true,pixie-demo=%s", appName))
			if err != nil {
				return err
			}

			// Delete the remaining resources before namespace deletion.
			od = k8s.ObjectDeleter{
				Clientset:  clientset,
				RestConfig: kubeConfig,
				Timeout:    2 * time.Minute,
			}

			_, err = od.DeleteByLabel(fmt.Sprintf("pixie-demo=%s", appName))
			if err != nil {
				return err
			}

			err = clientset.CoreV1().Namespaces().Delete(context.Background(), appName, metav1.DeleteOptions{})
			if err != nil {
				return err
			}
			t := time.NewTimer(180 * time.Second)
			defer t.Stop()

			s := time.NewTicker(5 * time.Second)
			defer s.Stop()

			for {
				select {
				case <-t.C:
					return errors.New("timeout waiting for namespace deletion")
				default:
					_, err := clientset.CoreV1().Namespaces().Get(context.Background(), appName, metav1.GetOptions{})
					if k8s_errors.IsNotFound(err) {
						return nil
					}
					if err != nil {
						return err
					}
					<-s.C
				}
			}
		}),
	}
	tr := utils.NewSerialTaskRunner(deleteDemo)
	return tr.RunAndMonitor()
}

func downloadDemoAppYAMLs(appName, artifacts string) (map[string][]byte, error) {
	targzBytes, err := downloadGCSFileFromHTTP(artifacts, fmt.Sprintf("%s.tar.gz", appName))
	if err != nil {
		return nil, err
	}
	gzipReader, err := gzip.NewReader(bytes.NewReader(targzBytes))
	if err != nil {
		return nil, err
	}
	defer gzipReader.Close()

	tarReader := tar.NewReader(gzipReader)
	outputYAMLs := map[string][]byte{}

	for {
		hdr, err := tarReader.Next()
		if err == io.EOF {
			break // End of archive
		}
		if err != nil {
			return nil, err
		}

		if !strings.HasSuffix(hdr.Name, ".yaml") {
			continue
		}

		contents, err := io.ReadAll(tarReader)
		if err != nil {
			return nil, err
		}
		outputYAMLs[hdr.Name] = contents
	}
	return outputYAMLs, nil
}

func namespaceExists(namespace string) bool {
	kubeConfig := k8s.GetConfig()
	clientset := k8s.GetClientset(kubeConfig)
	_, err := clientset.CoreV1().Namespaces().Get(context.Background(), namespace, metav1.GetOptions{})
	return err == nil
}

func createNamespace(namespace string) error {
	kubeConfig := k8s.GetConfig()
	clientset := k8s.GetClientset(kubeConfig)
	_, err := clientset.CoreV1().Namespaces().Create(context.Background(), &v1.Namespace{ObjectMeta: metav1.ObjectMeta{Name: namespace}}, metav1.CreateOptions{})
	return err
}

func certManagerExists() (bool, error) {
	kubeConfig := k8s.GetConfig()
	clientset := k8s.GetClientset(kubeConfig)

	deps, err := clientset.AppsV1().Deployments("").List(context.Background(), metav1.ListOptions{})
	if err != nil {
		return false, err
	}

	for _, d := range deps.Items {
		if d.Name == "cert-manager" {
			return true, nil
		}
	}

	return false, err
}

func setupDemoApp(appName string, yamls map[string][]byte, deps map[string]bool) error {
	kubeConfig := k8s.GetConfig()
	clientset := k8s.GetClientset(kubeConfig)

	// Check deps.
	if deps["cert-manager"] {
		certMgrExists, err := certManagerExists()
		if err != nil && !k8s_errors.IsNotFound(err) {
			return err
		}

		if !certMgrExists || k8s_errors.IsNotFound(err) {
			return errCertMgrDoesNotExist
		}
	}

	if namespaceExists(appName) {
		fmt.Printf("%s: namespace %s already exists. If created with px, run %s to remove\n",
			color.RedString("Error"), color.RedString(appName), color.GreenString(fmt.Sprintf("px demo delete %s", appName)))
		return errNamespaceAlreadyExists
	}

	tasks := []utils.Task{
		newTaskWrapper(fmt.Sprintf("Creating namespace %s", appName), func() error {
			return createNamespace(appName)
		}),
		newTaskWrapper(fmt.Sprintf("Deploying %s YAMLs", appName), func() error {
			for _, yamlBytes := range yamls {
				yamlBytes := yamlBytes
				bo := backoff.NewExponentialBackOff()
				bo.MaxElapsedTime = 5 * time.Minute

				op := func() error {
					return k8s.ApplyYAML(clientset, kubeConfig, appName, bytes.NewReader(yamlBytes), false)
				}

				err := backoff.Retry(op, bo)
				if err != nil {
					return err
				}
			}
			return nil
		}),
	}

	tr := utils.NewSerialTaskRunner(tasks)
	return tr.RunAndMonitor()
}
