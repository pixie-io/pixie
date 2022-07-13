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
	"fmt"
	"time"

	"github.com/fatih/color"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"

	"px.dev/pixie/src/pixie_cli/pkg/components"
	"px.dev/pixie/src/pixie_cli/pkg/utils"
	"px.dev/pixie/src/pixie_cli/pkg/vizier"
	"px.dev/pixie/src/utils/shared/k8s"
)

func init() {
	DeleteCmd.Flags().BoolP("clobber", "d", true, "Whether to delete all dependencies in the cluster")
	DeleteCmd.Flags().StringP("namespace", "n", "", "The namespace where Pixie is located")
}

// DeleteCmd is the "delete" command.
var DeleteCmd = &cobra.Command{
	Use:   "delete",
	Short: "Deletes Pixie on the current K8s cluster",
	PreRun: func(cmd *cobra.Command, args []string) {
		viper.BindPFlag("clobber", cmd.Flags().Lookup("clobber"))
		viper.BindPFlag("namespace", cmd.Flags().Lookup("namespace"))
	},
	Run: func(cmd *cobra.Command, args []string) {
		clobberAll, _ := cmd.Flags().GetBool("clobber")
		ns, _ := cmd.Flags().GetString("namespace")
		if ns == "" {
			ns = vizier.MustFindVizierNamespace()
		}
		deletePixie(ns, clobberAll)
	},
}

func deletePixie(ns string, clobberAll bool) {
	kubeConfig := k8s.GetConfig()
	kubeAPIConfig := k8s.GetClientAPIConfig()
	clientset := k8s.GetClientset(kubeConfig)

	opNs, _ := vizier.FindOperatorNamespace(clientset)

	od := k8s.ObjectDeleter{
		Namespace:  ns,
		Clientset:  clientset,
		RestConfig: kubeConfig,
		Timeout:    2 * time.Minute,
	}
	opOd := k8s.ObjectDeleter{
		Namespace:  opNs,
		Clientset:  clientset,
		RestConfig: kubeConfig,
		Timeout:    2 * time.Minute,
	}

	tasks := make([]utils.Task, 0)

	currentCluster := kubeAPIConfig.CurrentContext
	var noClobberInfo string
	if clobberAll {
		utils.WithColor(color.New(color.FgRed)).Infof("This action will delete the entire '%s' namespace.", ns)
		noClobberInfo = " For a partial deletion which preserves the namespace, try `px delete --clobber=false`."
	}
	prompt := fmt.Sprintf("Confirm to proceed on cluster %s.", currentCluster)
	proceed := components.YNPrompt(prompt, true)
	if !proceed {
		utils.Errorf("User exited.%s", noClobberInfo)
		return
	}

	if clobberAll {
		tasks = append(tasks, newTaskWrapper("Deleting namespace", od.DeleteNamespace))
		if opNs != "" {
			tasks = append(tasks, newTaskWrapper("Deleting operator namespace", opOd.DeleteNamespace))
		}
		tasks = append(tasks, newTaskWrapper("Deleting cluster-scoped resources", func() error {
			_, err := od.DeleteByLabel("app=pl-monitoring")
			return err
		}))
	} else {
		tasks = append(tasks, newTaskWrapper("Deleting Vizier pods/services", func() error {
			_, err := od.DeleteByLabel("component=vizier")
			return err
		}))
	}

	delJr := utils.NewSerialTaskRunner(tasks)
	err := delJr.RunAndMonitor()
	if err != nil {
		utils.WithError(err).Fatal("Error deleting Pixie")
	}
}
