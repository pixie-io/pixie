package cmd

import (
	"io/ioutil"
	"os"
	"os/exec"
	"path"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
)

// NewCmdDeletePixie creates a new "delete" command.
func NewCmdDeletePixie() *cobra.Command {
	return &cobra.Command{
		Use:   "delete",
		Short: "Deletes Pixie on the current K8s cluster",
		Run: func(cmd *cobra.Command, args []string) {
			clobberAll, _ := cmd.Flags().GetBool("clobber_namespace")
			deletePixie(clobberAll)
		},
	}
}

func deletePixie(clobberAll bool) {
	// Extract yamls into tmp dir, to use for deletion.
	dir, err := ioutil.TempDir("", "vizier_yamls")
	if err != nil {
		log.WithError(err).Fatal("Could not create tmp directory")
	}

	defer os.RemoveAll(dir)

	// Create a file for each asset, and write the asset's contents to the file.
	for _, asset := range AssetNames() {
		contents, err := Asset(asset)
		if err != nil {
			log.WithError(err).Fatal("Could not load asset")
		}

		fname := path.Join(dir, path.Base(asset))
		f, err := os.Create(fname)
		defer f.Close()
		err = ioutil.WriteFile(fname, contents, 0644)
		if err != nil {
			log.WithError(err).Fatal("Could not write to file")
		}
	}

	log.Info("Deleting Vizier pods/services")
	err = deleteFile(path.Join(dir, "vizier.yaml"))
	if err != nil {
		log.WithError(err).Error("Could not delete Vizier")
	}

	if clobberAll {
		log.Info("Deleting NATS operator")
		err = deleteFile(path.Join(dir, "nats.yaml"))
		if err != nil {
			log.WithError(err).Error("Could not delete NATS operator")
		}

		log.Info("Deleting etcd operator")
		err = deleteFile(path.Join(dir, "etcd.yaml"))
		if err != nil {
			log.WithError(err).Error("Could not delete etcd operator")
		}
	}
}

func deleteFile(filePath string) error {
	kcmd := exec.Command("kubectl", "delete", "-f", filePath)
	return kcmd.Run()
}
