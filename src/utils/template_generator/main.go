package main

import (
	"strings"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"

	"px.dev/pixie/src/utils/shared/k8s"
	"px.dev/pixie/src/utils/shared/yamls"
	vizieryamls "px.dev/pixie/src/utils/template_generator/vizier_yamls"
)

func init() {
	pflag.String("base", "", "Path to the tar containing the base Vizier YAMLs")
	pflag.String("out", "", "The output path")
	pflag.String("version", "", "The version string for the YAMLs")
	pflag.String("namespace", "pl", "The namespace to install K8s secrets to")
	pflag.String("image_secret_name", "pl-image-secret", "The name of the imagePullSecrets")
}

func main() {
	pflag.Parse()

	viper.AutomaticEnv()
	viper.SetEnvPrefix("PL")
	viper.BindPFlags(pflag.CommandLine)

	base := viper.GetString("base")
	out := viper.GetString("out")
	version := viper.GetString("version")
	namespace := viper.GetString("namespace")
	imageSecretName := viper.GetString("image_secret_name")

	if len(base) == 0 {
		log.Fatalln("Base YAML path (--base) is required")
	}
	if len(out) == 0 {
		log.Fatalln("Output YAML path (--out) is required")
	}
	if len(version) == 0 {
		log.Fatalln("Version (--version) is required")
	}

	log.WithField("base", base).Info("Base YAML path")
	log.WithField("out", out).Info("Output path")
	log.WithField("version", version).Info("Version")

	kubeConfig := k8s.GetConfig()
	clientset := k8s.GetClientset(kubeConfig)

	imagePullCreds := ""
	if strings.Contains(version, "-") { // Check if the version is an rc, if so, read and include the image pull secrets.
		secret := k8s.GetSecret(clientset, "plc", "vizier-image-secret")
		if secret != nil {
			imagePullCreds = string(secret.Data["vizier_image_secret.json"])
		}
	}

	templatedYAMLs, err := vizieryamls.GenerateTemplatedDeployYAMLsWithTar(clientset, base, version, namespace, imageSecretName, imagePullCreds)
	if err != nil {
		log.WithError(err).Fatal("failed to generate templated deployment YAMLs")
	}

	if err := yamls.ExtractYAMLs(templatedYAMLs, out, "pixie_yamls", yamls.MultiFileExtractYAMLFormat); err != nil {
		log.WithError(err).Fatal("failed to extract deployment YAMLs")
	}
}
