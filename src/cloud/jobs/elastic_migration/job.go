package main

import (
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"

	"pixielabs.ai/pixielabs/src/cloud/jobs/elastic_migration/controller"
	"pixielabs.ai/pixielabs/src/cloud/jobs/elastic_migration/schema"
	"pixielabs.ai/pixielabs/src/cloud/shared/esutils"
)

func init() {
	pflag.String("es_index", "", "The name of the index to create.")
	pflag.String("mapping_file", "schema.json", "The file containing the mapping in the schema directory.")
	pflag.String("es_url", "https://pl-elastic-es-http:9200", "The URL for the elastic cluster,")
	pflag.String("es_ca_cert", "/es-certs/tls.crt", "The CA cert for elastic.")
	pflag.String("es_user", "elastic", "The user for elastic.")
	pflag.String("es_passwd", "elastic", "The password for elastic.")
}

func main() {
	pflag.Parse()
	viper.AutomaticEnv()
	viper.SetEnvPrefix("PL")
	viper.BindPFlags(pflag.CommandLine)

	elasticURL := viper.GetString("es_url")
	index := viper.GetString("es_index")

	log.Infof("Connecting to elastic cluster at '%s'", elasticURL)
	log.Infof("Creating index '%s'", index)

	es, err := esutils.NewEsClient(&esutils.Config{
		URL:        []string{elasticURL},
		User:       viper.GetString("es_user"),
		Passwd:     viper.GetString("es_passwd"),
		CaCertFile: viper.GetString("es_ca_cert"),
	})

	if err != nil {
		log.WithError(err).Fatalf("Failed to connect to %s", elasticURL)
	}
	schemaFile := viper.GetString("mapping_file")
	srcMapping, err := schema.Asset(schemaFile)
	if err != nil {
		log.WithError(err).Fatalf("Can't find mapping '%s'", schemaFile)
	}

	im := controller.NewIndexManager(es)
	err = im.PrepareIndex(index, string(srcMapping))
	if err != nil {
		log.WithError(err).Fatalf("Failed to create '%s'", index)
	}

	log.Infof("Index '%s' successfully created", index)
}
