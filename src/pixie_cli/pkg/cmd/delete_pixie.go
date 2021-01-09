package cmd

import (
	"os"
	"time"

	"github.com/spf13/cobra"
	"github.com/spf13/viper"
	"k8s.io/client-go/kubernetes"
	"pixielabs.ai/pixielabs/src/utils/shared/k8s"

	// Blank import necessary for kubeConfig to work.
	_ "k8s.io/client-go/plugin/pkg/client/auth"

	"pixielabs.ai/pixielabs/src/pixie_cli/pkg/utils"
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
		tasks = append(tasks, newTaskWrapper("Deleting namespace", func() error {
			return od.DeleteNamespace()
		}))
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

func deleteVizier(clientset *kubernetes.Clientset, ns string) error {
	return k8s.DeleteAllResources(clientset, ns, "component=vizier")
}
