package cmd

import (
	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/vizier"
)

func mustConnectDefaultVizier(cloudAddr string) *vizier.Connector {
	l, err := vizier.NewLister(cloudAddr)
	if err != nil {
		log.WithError(err).Fatal("Failed to create Vizier lister")
	}

	vzInfo, err := l.GetViziersInfo()
	if err != nil {
		log.WithError(err).Fatal("Failed to get Vizier info")
	}

	if len(vzInfo) == 0 {
		log.WithError(err).Fatal("No Viziers available")
	}

	u := utils.UUIDFromProtoOrNil(vzInfo[0].ID)

	vzConn, err := l.GetVizierConnection(u)
	if err != nil {
		log.WithError(err).Fatal("Failed to create Vizier connection")
	}

	v, err := vizier.NewConnector(vzConn)
	if err != nil {
		log.WithError(err).Fatal("Failed to connect to Vizier")
	}
	return v
}
