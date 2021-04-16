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
)

// DeleteCmd is the "delete" command.
var DeleteCmd = &cobra.Command{
	Use:   "delete",
	Short: "Deletes Pixie on the current K8s cluster",
	Run: func(cmd *cobra.Command, args []string) {
		clobberAll, _ := cmd.Flags().GetBool("clobber")
		ns, _ := cmd.Flags().GetString("namespace")
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
		os.Exit(1)
	}
}
