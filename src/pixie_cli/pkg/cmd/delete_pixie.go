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
	"os"
	"time"

	"github.com/spf13/cobra"
	"github.com/spf13/viper"

	"px.dev/pixie/src/utils/shared/k8s"

	// Blank import necessary for kubeConfig to work.
	_ "k8s.io/client-go/plugin/pkg/client/auth"

	"px.dev/pixie/src/pixie_cli/pkg/utils"
	"px.dev/pixie/src/pixie_cli/pkg/vizier"
)

// DeleteCmd is the "delete" command.
var DeleteCmd = &cobra.Command{
	Use:   "delete",
	Short: "Deletes Pixie on the current K8s cluster",
	Run: func(cmd *cobra.Command, args []string) {
		clobberAll, _ := cmd.Flags().GetBool("clobber")
		ns, _ := cmd.Flags().GetString("namespace")
		nsSet := cmd.Flags().Changed("namespace")
		if !nsSet {
			ns = ""
		}
		deletePixie(ns, clobberAll)
	},
}

func init() {
	DeleteCmd.Flags().BoolP("clobber", "d", false, "Whether to delete all dependencies in the cluster")
	viper.BindPFlag("clobber", DeleteCmd.Flags().Lookup("clobber"))

	DeleteCmd.Flags().StringP("namespace", "n", "pl", "The namespace where pixie is located")
	viper.BindPFlag("namespace", DeleteCmd.Flags().Lookup("namespace"))
}

func deletePixie(ns string, clobberAll bool) {
	kubeConfig := k8s.GetConfig()
	clientset := k8s.GetClientset(kubeConfig)

	// Try to find the namespace where Pixie is deployed, if no namespace was specified.
	if ns == "" {
		vzNs, err := vizier.FindVizierNamespace(clientset)
		if err != nil {
			utils.WithError(err).Fatal("Failed to get Vizier namespace")
		}
		ns = vzNs
	}

	if ns == "" {
		utils.Info("Cannot find running Vizier instance")
		os.Exit(0)
	}

	od := k8s.ObjectDeleter{
		Namespace:  ns,
		Clientset:  clientset,
		RestConfig: kubeConfig,
		Timeout:    2 * time.Minute,
	}

	tasks := make([]utils.Task, 0)

	if clobberAll {
		tasks = append(tasks, newTaskWrapper("Deleting namespace", od.DeleteNamespace))
		tasks = append(tasks, newTaskWrapper("Deleting cluster-scoped resources", func() error {
			_, err := od.DeleteByLabel("app=pl-monitoring", k8s.AllResourceKinds...)
			return err
		}))
	} else {
		tasks = append(tasks, newTaskWrapper("Deleting Vizier pods/services", func() error {
			_, err := od.DeleteByLabel("component=vizier", k8s.AllResourceKinds...)
			return err
		}))
	}

	delJr := utils.NewSerialTaskRunner(tasks)
	err := delJr.RunAndMonitor()
	if err != nil {
		utils.WithError(err).Fatal("Error deleting Pixie")
	}
}
