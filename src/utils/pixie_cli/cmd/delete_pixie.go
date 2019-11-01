package cmd

import (
	"os"
	"os/exec"

	"github.com/spf13/cobra"
	"github.com/spf13/viper"

	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/utils"
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
	var deleteJob utils.Task

	if clobberAll {
		deleteJob = newTaskWrapper("Deleting namespace", func() error {
			kcmd := exec.Command("kubectl", "delete", "namespace", ns)
			if err := kcmd.Run(); err != nil {
				return err
			}
			return nil
		})
	} else {
		deleteJob = newTaskWrapper("Deleting Vizier pods/services", func() error {
			err := deleteVizier(ns)
			if err != nil {
				return err
			}
			return nil
		})
	}

	delJr := utils.NewSerialTaskRunner([]utils.Task{deleteJob})
	err := delJr.RunAndMonitor()
	if err != nil {
		os.Exit(1)
	}
}

func deleteVizier(ns string) error {
	kcmd := exec.Command("kubectl", "-n", ns, "delete", "pods,deployments,services,daemonsets", "-l", "component=vizier")
	return kcmd.Run()
}
