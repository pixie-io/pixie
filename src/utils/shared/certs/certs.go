/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package certs

import (
	"crypto/rand"
	"crypto/rsa"
	"crypto/x509"
	"crypto/x509/pkix"
	"encoding/pem"
	"fmt"
	"math/big"
	"strings"
	"time"

	"px.dev/pixie/src/utils/shared/k8s"
)

const bitsize = 4096

var x509Name = pkix.Name{
	Organization: []string{"Pixie Labs Inc."},
	Country:      []string{"US"},
	Province:     []string{"California"},
	Locality:     []string{"San Francisco"},
}

type certGenerator struct {
	ca    *x509.Certificate
	caKey *rsa.PrivateKey
}

func newCertGenerator() (*certGenerator, error) {
	ca := &x509.Certificate{
		SerialNumber:          big.NewInt(1653),
		Subject:               x509Name,
		NotBefore:             time.Now(),
		NotAfter:              time.Now().AddDate(5, 0, 0),
		IsCA:                  true,
		ExtKeyUsage:           []x509.ExtKeyUsage{x509.ExtKeyUsageClientAuth, x509.ExtKeyUsageServerAuth},
		KeyUsage:              x509.KeyUsageKeyEncipherment | x509.KeyUsageDigitalSignature | x509.KeyUsageCertSign,
		BasicConstraintsValid: true,
	}

	caKey, err := rsa.GenerateKey(rand.Reader, bitsize)
	if err != nil {
		return nil, err
	}

	return &certGenerator{
		ca:    ca,
		caKey: caKey,
	}, nil
}

func (cg *certGenerator) generateSignedCertAndKey(dnsNames []string) ([]byte, []byte, error) {
	cert := &x509.Certificate{
		SerialNumber:          big.NewInt(1658),
		Subject:               x509Name,
		NotBefore:             time.Now(),
		NotAfter:              time.Now().AddDate(5, 0, 0),
		ExtKeyUsage:           []x509.ExtKeyUsage{x509.ExtKeyUsageClientAuth, x509.ExtKeyUsageServerAuth},
		KeyUsage:              x509.KeyUsageKeyEncipherment | x509.KeyUsageDigitalSignature,
		BasicConstraintsValid: true,
		DNSNames:              dnsNames,
	}
	privateKey, err := rsa.GenerateKey(rand.Reader, bitsize)
	if err != nil {
		return nil, nil, err
	}

	return cg.signCertAndKey(cert, privateKey)
}

func (cg *certGenerator) signedCA() ([]byte, error) {
	caCertData, _, err := cg.signCertAndKey(cg.ca, cg.caKey)
	if err != nil {
		return nil, err
	}
	return caCertData, nil
}

func (cg *certGenerator) signCertAndKey(cert *x509.Certificate, privateKey *rsa.PrivateKey) ([]byte, []byte, error) {
	certBytes, err := x509.CreateCertificate(rand.Reader, cert, cg.ca, &privateKey.PublicKey, cg.caKey)
	if err != nil {
		return nil, nil, err
	}

	certData := pem.EncodeToMemory(&pem.Block{Type: "CERTIFICATE", Bytes: certBytes})
	if err != nil {
		return nil, nil, err
	}

	keyData := pem.EncodeToMemory(&pem.Block{Type: "RSA PRIVATE KEY", Bytes: x509.MarshalPKCS1PrivateKey(privateKey)})
	if err != nil {
		return nil, nil, err
	}

	return certData, keyData, nil
}

func getVizierDNSNamesForNamespace(namespace string) []string {
	// Localhost must be here because etcd relies on it.
	return []string{
		fmt.Sprintf("*.%s.svc", namespace),
		fmt.Sprintf("*.%s.svc.cluster.local", namespace),
		fmt.Sprintf("*.%s.pod.cluster.local", namespace),
		fmt.Sprintf("*.pl-etcd.%s.svc", namespace),
		fmt.Sprintf("*.pl-etcd.%s.svc.cluster.local", namespace),
		"pl-nats",
		"pl-etcd",
		"localhost",
	}
}

func getCloudDNSNamesForNamespace(namespace string) []string {
	return []string{
		fmt.Sprintf("*.%s", namespace),
		fmt.Sprintf("*.%s.svc.cluster.local", namespace),
		fmt.Sprintf("*.%s.pod.cluster.local", namespace),
		fmt.Sprintf("*.pl-nats.%s.svc", namespace),
		"*.pl-nats",
		"pl-nats",
		"*.local",
		"localhost",
	}
}

// GenerateCloudCertYAMLs generates the yamls for cloud certs.
func GenerateCloudCertYAMLs(namespace string) (string, error) {
	cg, err := newCertGenerator()
	if err != nil {
		return "", err
	}

	clientCert, clientKey, err := cg.generateSignedCertAndKey(getCloudDNSNamesForNamespace(namespace))
	if err != nil {
		return "", err
	}
	serverCert, serverKey, err := cg.generateSignedCertAndKey(getCloudDNSNamesForNamespace(namespace))
	if err != nil {
		return "", err
	}
	caCert, err := cg.signedCA()
	if err != nil {
		return "", err
	}

	tlsCert, err := k8s.CreateGenericSecretFromLiterals(namespace, "service-tls-certs", map[string]string{
		"server.key": string(serverKey),
		"server.crt": string(serverCert),
		"ca.crt":     string(caCert),
		"client.key": string(clientKey),
		"client.crt": string(clientCert),
	})
	if err != nil {
		return "", err
	}
	yaml, err := k8s.ConvertResourceToYAML(tlsCert)
	if err != nil {
		return "", err
	}

	return fmt.Sprintf("---\n%s\n", yaml), nil
}

// GenerateVizierCertYAMLs generates the yamls for vizier certs.
func GenerateVizierCertYAMLs(namespace string) (string, error) {
	cg, err := newCertGenerator()
	if err != nil {
		return "", err
	}

	clientCert, clientKey, err := cg.generateSignedCertAndKey(getVizierDNSNamesForNamespace(namespace))
	if err != nil {
		return "", err
	}
	serverCert, serverKey, err := cg.generateSignedCertAndKey(getVizierDNSNamesForNamespace(namespace))
	if err != nil {
		return "", err
	}
	caCert, err := cg.signedCA()
	if err != nil {
		return "", err
	}

	var yamls []string

	proxyCert, err := k8s.CreateGenericSecretFromLiterals(namespace, "proxy-tls-certs", map[string]string{
		"tls.key": string(serverKey),
		"tls.crt": string(serverCert),
	})
	if err != nil {
		return "", err
	}
	pcYaml, err := k8s.ConvertResourceToYAML(proxyCert)
	if err != nil {
		return "", err
	}
	yamls = append(yamls, pcYaml)

	tlsCert, err := k8s.CreateGenericSecretFromLiterals(namespace, "service-tls-certs", map[string]string{
		"server.key": string(serverKey),
		"server.crt": string(serverCert),
		"ca.crt":     string(caCert),
		"client.key": string(clientKey),
		"client.crt": string(clientCert),
	})
	if err != nil {
		return "", err
	}
	tcYaml, err := k8s.ConvertResourceToYAML(tlsCert)
	if err != nil {
		return "", err
	}
	yamls = append(yamls, tcYaml)

	etcdPeerCert, err := k8s.CreateGenericSecretFromLiterals(namespace, "etcd-peer-tls-certs", map[string]string{
		"peer.key":    string(serverKey),
		"peer.crt":    string(serverCert),
		"peer-ca.crt": string(caCert),
	})
	if err != nil {
		return "", err
	}
	epYaml, err := k8s.ConvertResourceToYAML(etcdPeerCert)
	if err != nil {
		return "", err
	}
	yamls = append(yamls, epYaml)

	etcdClientCert, err := k8s.CreateGenericSecretFromLiterals(namespace, "etcd-client-tls-certs", map[string]string{
		"etcd-client.key":    string(clientKey),
		"etcd-client.crt":    string(clientCert),
		"etcd-client-ca.crt": string(caCert),
	})
	if err != nil {
		return "", err
	}
	ecYaml, err := k8s.ConvertResourceToYAML(etcdClientCert)
	if err != nil {
		return "", err
	}
	yamls = append(yamls, ecYaml)

	etcdServerCert, err := k8s.CreateGenericSecretFromLiterals(namespace, "etcd-server-tls-certs", map[string]string{
		"server.key":    string(serverKey),
		"server.crt":    string(serverCert),
		"server-ca.crt": string(caCert),
	})
	if err != nil {
		return "", err
	}
	esYaml, err := k8s.ConvertResourceToYAML(etcdServerCert)
	if err != nil {
		return "", err
	}
	yamls = append(yamls, esYaml)

	return "---\n" + strings.Join(yamls, "\n---\n"), nil
}
