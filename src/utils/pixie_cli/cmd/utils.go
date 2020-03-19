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

	passthrough := false
	selectedVzInfo := vzInfo[0]

	if selectedVzInfo.Config != nil {
		passthrough = selectedVzInfo.Config.PassthroughEnabled
	}

	u := utils.UUIDFromProtoOrNil(selectedVzInfo.ID)

	var vzConn *vizier.ConnectionInfo
	if !passthrough {
		vzConn, err = l.GetVizierConnection(u)
		if err != nil {
			log.WithError(err).Fatal("Failed to create Vizier connection")
		}
	}

	v, err := vizier.NewConnector(cloudAddr, selectedVzInfo, vzConn)
	if err != nil {
		log.WithError(err).Fatal("Failed to connect to Vizier")
	}
	return v
}
