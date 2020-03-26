package cmd

import (
	"errors"

	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/vizier"
)

func mustConnectDefaultVizier(cloudAddr string) *vizier.Connector {
	c, err := connectDefaultVizier(cloudAddr)
	if err != nil {
		log.WithError(err).Fatal("Failed to connect to vizier")
	}
	return c
}

func connectDefaultVizier(cloudAddr string) (*vizier.Connector, error) {
	l, err := vizier.NewLister(cloudAddr)
	if err != nil {
		return nil, err
	}

	vzInfo, err := l.GetViziersInfo()
	if err != nil {
		return nil, err
	}

	if len(vzInfo) == 0 {
		return nil, errors.New("no Viziers available")
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
			return nil, err
		}
	}

	v, err := vizier.NewConnector(cloudAddr, selectedVzInfo, vzConn)
	if err != nil {
		return nil, err
	}
	return v, nil
}
