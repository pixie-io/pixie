package certs

import (
	"crypto"
	"crypto/rand"
	"crypto/rsa"
	"crypto/tls"
	"crypto/x509"
	"crypto/x509/pkix"
	"encoding/pem"
	"fmt"
	"io/ioutil"
	"math/big"
	"os"
	"path"
	"strings"
	"time"

	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"

	"pixielabs.ai/pixielabs/src/utils/shared/k8s"
)

func generateCA(certPath string, bitsize int) (*x509.Certificate, crypto.PrivateKey, error) {
	ca := &x509.Certificate{
		SerialNumber: big.NewInt(1653),
		Subject: pkix.Name{
			Organization: []string{"Pixie Labs Inc."},
			Country:      []string{"US"},
			Province:     []string{"California"},
			Locality:     []string{"San Francisco"},
		},
		NotBefore:             time.Now(),
		NotAfter:              time.Now().AddDate(1, 0, 0),
		IsCA:                  true,
		ExtKeyUsage:           []x509.ExtKeyUsage{x509.ExtKeyUsageClientAuth, x509.ExtKeyUsageServerAuth},
		KeyUsage:              x509.KeyUsageKeyEncipherment | x509.KeyUsageDigitalSignature | x509.KeyUsageCertSign,
		BasicConstraintsValid: true,
	}

	caKey, err := rsa.GenerateKey(rand.Reader, bitsize)
	if err != nil {
		return nil, nil, err
	}

	err = signCertificate(certPath, "ca", ca, ca, caKey, caKey)
	if err != nil {
		return nil, nil, err
	}

	return ca, caKey, nil
}

func loadCA(caCert string, caKey string) (*x509.Certificate, crypto.PrivateKey, error) {
	caPair, err := tls.LoadX509KeyPair(caCert, caKey)
	if err != nil {
		return nil, nil, err
	}
	ca, err := x509.ParseCertificate(caPair.Certificate[0])
	if err != nil {
		return nil, nil, err
	}

	return ca, caPair.PrivateKey, nil
}

func generateCertificate(certPath string, certName string, caCert *x509.Certificate, caKey crypto.PrivateKey, bitsize int) error {
	// Prepare certificate.
	cert := &x509.Certificate{
		SerialNumber: big.NewInt(1658),
		Subject: pkix.Name{
			Organization: []string{"Pixie Labs Inc."},
			Country:      []string{"US"},
			Province:     []string{"California"},
			Locality:     []string{"San Francisco"},
		},
		NotBefore:             time.Now(),
		NotAfter:              time.Now().AddDate(1, 0, 0),
		ExtKeyUsage:           []x509.ExtKeyUsage{x509.ExtKeyUsageClientAuth, x509.ExtKeyUsageServerAuth},
		KeyUsage:              x509.KeyUsageKeyEncipherment | x509.KeyUsageDigitalSignature,
		BasicConstraintsValid: true,
		// Localhost must be here because etcd relies on it.
		DNSNames: []string{
			"*.local",
			"*.pl.svc",
			"*.pl.svc.cluster.local",
			"*.plc",
			"*.plc.svc.cluster.local",
			"*.plc-dev",
			"*.plc-dev.svc.cluster.local",
			"*.plc-staging",
			"*.plc-testing",
			"pl-etcd",
			"*.pl-etcd.pl.svc",
			"*.pl-etcd.pl.svc.cluster.local",
			"pl-nats",
			"*.pl-nats",
			"localhost",
		},
	}
	privateKey, err := rsa.GenerateKey(rand.Reader, bitsize)
	if err != nil {
		return err
	}

	err = signCertificate(certPath, certName, cert, caCert, caKey, privateKey)
	if err != nil {
		return err
	}

	return nil
}

func signCertificate(certPath string, certName string, cert *x509.Certificate, ca *x509.Certificate, caKey crypto.PrivateKey, privateKey *rsa.PrivateKey) error {
	// Self-sign certificate.
	certB, err := x509.CreateCertificate(rand.Reader, cert, ca, &privateKey.PublicKey, caKey)
	if err != nil {
		return err
	}

	certOut, err := os.Create(path.Join(certPath, fmt.Sprintf("%s.crt", certName)))
	if err != nil {
		return err
	}
	pem.Encode(certOut, &pem.Block{Type: "CERTIFICATE", Bytes: certB})
	certOut.Close()

	// Generate key.
	keyOut, err := os.OpenFile(path.Join(certPath, fmt.Sprintf("%s.key", certName)), os.O_WRONLY|os.O_CREATE|os.O_TRUNC, 0600)
	if err != nil {
		return err
	}
	pem.Encode(keyOut, &pem.Block{Type: "RSA PRIVATE KEY", Bytes: x509.MarshalPKCS1PrivateKey(privateKey)})
	keyOut.Close()
	return nil
}

func generateCerts(certPath string, caCertPath string, caKeyPath string, bitsize int) error {
	var ca *x509.Certificate
	var caKey crypto.PrivateKey
	var err error

	if caCertPath == "" {
		ca, caKey, err = generateCA(certPath, bitsize)
		if err != nil {
			return err
		}
	} else {
		ca, caKey, err = loadCA(caCertPath, caKeyPath)
		if err != nil {
			return err
		}
	}

	// Generate server certificate.
	err = generateCertificate(certPath, "server", ca, caKey, bitsize)
	if err != nil {
		return err
	}

	// Generate client certificate.
	err = generateCertificate(certPath, "client", ca, caKey, bitsize)
	if err != nil {
		return err
	}

	return nil
}

// DefaultInstallCerts installs certs using the default options.
func DefaultInstallCerts(namespace string, clientset *kubernetes.Clientset, config *rest.Config) {
	err := installCertsUsingClientset("", "", "", namespace, 4096, clientset, config)
	if err != nil {
		fmt.Printf("Failed to install certs")
	}
}

// DefaultGenerateCertYAMLs generates the yamls for certs with the default options.
func DefaultGenerateCertYAMLs(namespace string) (string, error) {
	return generateCertYAMLs("", "", "", namespace, 4096)
}

func generateCertYAMLs(certPath string, caCertPath string, caKeyPath string, namespace string, bitsize int) (string, error) {
	var err error

	deleteCerts := false
	if certPath == "" {
		certPath, err = ioutil.TempDir("", "certs")
		if err != nil {
			return "", err
		}
		deleteCerts = true
	}

	// Delete generated certs.
	defer func() {
		if deleteCerts {
			os.RemoveAll(certPath)
		}
	}()

	err = generateCerts(certPath, caCertPath, caKeyPath, bitsize)
	if err != nil {
		return "", err
	}

	serverKey := path.Join(certPath, "server.key")
	serverCert := path.Join(certPath, "server.crt")
	caCert := path.Join(certPath, "ca.crt")
	if caCertPath != "" {
		caCert = caCertPath
	}
	clientKey := path.Join(certPath, "client.key")
	clientCert := path.Join(certPath, "client.crt")

	yamls := make([]string, 0)

	// Create secrets in k8s.
	proxyCert, err := k8s.CreateTLSSecret(namespace, "proxy-tls-certs", serverKey, serverCert)
	if err != nil {
		return "", err
	}
	pcYaml, err := k8s.ConvertResourceToYAML(proxyCert)
	if err != nil {
		return "", err
	}
	yamls = append(yamls, pcYaml)

	tlsCert, err := k8s.CreateGenericSecret(namespace, "service-tls-certs", map[string]string{
		"server.key": serverKey,
		"server.crt": serverCert,
		"ca.crt":     caCert,
		"client.key": clientKey,
		"client.crt": clientCert,
	})
	if err != nil {
		return "", err
	}
	tcYaml, err := k8s.ConvertResourceToYAML(tlsCert)
	if err != nil {
		return "", err
	}
	yamls = append(yamls, tcYaml)

	etcdPeerCert, err := k8s.CreateGenericSecret(namespace, "etcd-peer-tls-certs", map[string]string{
		"peer.key":    serverKey,
		"peer.crt":    serverCert,
		"peer-ca.crt": caCert,
	})
	if err != nil {
		return "", err
	}
	epYaml, err := k8s.ConvertResourceToYAML(etcdPeerCert)
	if err != nil {
		return "", err
	}
	yamls = append(yamls, epYaml)

	etcdClientCert, err := k8s.CreateGenericSecret(namespace, "etcd-client-tls-certs", map[string]string{
		"etcd-client.key":    clientKey,
		"etcd-client.crt":    clientCert,
		"etcd-client-ca.crt": caCert,
	})
	if err != nil {
		return "", err
	}
	ecYaml, err := k8s.ConvertResourceToYAML(etcdClientCert)
	if err != nil {
		return "", err
	}
	yamls = append(yamls, ecYaml)

	etcdServerCert, err := k8s.CreateGenericSecret(namespace, "etcd-server-tls-certs", map[string]string{
		"server.key":    serverKey,
		"server.crt":    serverCert,
		"server-ca.crt": caCert,
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

func installCertsUsingClientset(certPath string, caCertPath string, caKeyPath string, namespace string, bitsize int, clientset *kubernetes.Clientset, config *rest.Config) error {
	// Delete secrets in k8s.
	k8s.DeleteSecret(clientset, namespace, "proxy-tls-certs")
	k8s.DeleteSecret(clientset, namespace, "service-tls-certs")
	k8s.DeleteSecret(clientset, namespace, "etcd-peer-tls-certs")
	k8s.DeleteSecret(clientset, namespace, "etcd-client-tls-certs")
	k8s.DeleteSecret(clientset, namespace, "etcd-server-tls-certs")

	yamls, err := generateCertYAMLs(certPath, caCertPath, caKeyPath, namespace, bitsize)

	if err != nil {
		return err
	}

	return k8s.ApplyYAML(clientset, config, namespace, strings.NewReader(yamls), false)
}

// InstallCerts generates the necessary certs and installs them in kubernetes.
func InstallCerts(certPath string, caCertPath string, caKeyPath string, namespace string, bitsize int) {
	// Authenticate with k8s cluster.
	config := k8s.GetConfig()
	clientset := k8s.GetClientset(config)

	err := installCertsUsingClientset(certPath, caCertPath, caKeyPath, namespace, bitsize, clientset, config)
	if err != nil {
		fmt.Printf("Failed to install certs")
	}
}
