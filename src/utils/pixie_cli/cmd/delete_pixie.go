package cmd

import (
	"os/exec"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
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
	if clobberAll {
		log.WithField("namespace", ns).Info("Deleting Pixie Namespace")
		kcmd := exec.Command("kubectl", "delete", "namespace", ns)
		if err := kcmd.Run(); err != nil {
			log.WithError(err).Fatal("failed to delete pixie")
		}
		log.Info("Pixie Deleted")
		return
	}

	log.Info("Deleting Vizier pods/services")
	err := deleteVizier(ns)
	if err != nil {
		log.WithError(err).Error("Could not delete Vizier")
	}
}

func deleteVizier(ns string) error {
	kcmd := exec.Command("kubectl", "-n", ns, "delete", "pods,deployments,services", "-l", "component=vizier")
	return kcmd.Run()
}
