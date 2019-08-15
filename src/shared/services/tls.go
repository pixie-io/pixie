package services

import (
	"crypto/tls"
	"crypto/x509"
	"fmt"
	"io/ioutil"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"
)

// DefaultServerTLSConfig has the TLS config setup by the default service flags.
func DefaultServerTLSConfig() (*tls.Config, error) {
	tlsCert := viper.GetString("server_tls_cert")
	tlsKey := viper.GetString("server_tls_key")
	tlsCACert := viper.GetString("tls_ca_cert")

	log.WithFields(log.Fields{
		"tlsCertFile": tlsCert,
		"tlsKeyFile":  tlsKey,
		"tlsCA":       tlsCACert,
	}).Info("Loading HTTP TLS certs")

	pair, err := tls.LoadX509KeyPair(tlsCert, tlsKey)
	if err != nil {
		return nil, fmt.Errorf("failed to load keys: %s", err.Error())
	}

	certPool := x509.NewCertPool()
	ca, err := ioutil.ReadFile(tlsCACert)
	if err != nil {
		return nil, fmt.Errorf("failed to read CA cert: %s", err.Error())
	}

	// Append the client certificates from the CA.
	if ok := certPool.AppendCertsFromPEM(ca); !ok {
		return nil, fmt.Errorf("failed to append CA cert")
	}

	return &tls.Config{
		Certificates: []tls.Certificate{pair},
		NextProtos:   []string{"h2"},
		ClientCAs:    certPool,
	}, nil
}
