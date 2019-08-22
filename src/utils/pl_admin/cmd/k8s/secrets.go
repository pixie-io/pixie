package k8s

import (
	"bytes"
	"crypto/tls"
	"fmt"
	"io/ioutil"
	"os/exec"
	"strings"

	log "github.com/sirupsen/logrus"
	"k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/util/validation"
	"k8s.io/client-go/kubernetes"
)

// DeleteSecret deletes the given secret in kubernetes.
func DeleteSecret(clientset *kubernetes.Clientset, namespace, name string) {
	err := clientset.CoreV1().Secrets(namespace).Delete(name, &metav1.DeleteOptions{})
	if err != nil {
		log.WithError(err).Info("could not delete secret")
	} else {
		log.Info(fmt.Sprintf("Deleted secret: %s", name))
	}
}

// Contents below are copied and modified from
// https://github.com/kubernetes/kubectl/blob/3874cf79897cfe1e070e592391792658c44b78d4/pkg/generate/versioned/secret.go.

// CreateGenericSecret creates a generic secret in kubernetes.
func CreateGenericSecret(clientset *kubernetes.Clientset, namespace, name string, fromFiles map[string]string) {
	secret := &v1.Secret{}
	secret.SetGroupVersionKind(v1.SchemeGroupVersion.WithKind("Secret"))

	secret.Name = name
	secret.Data = map[string][]byte{}

	handleFromFileSources(secret, fromFiles)

	_, err := clientset.CoreV1().Secrets(namespace).Create(secret)
	if err != nil {
		log.WithError(err).Fatal("could not create generic secret")
	}
	log.Info(fmt.Sprintf("Created generic secret: %s", name))
}

// CreateGenericSecretFromLiterals creates a generic secret in kubernetes using literals.
func CreateGenericSecretFromLiterals(clientset *kubernetes.Clientset, namespace, name string, fromLiterals map[string]string) {
	secret := &v1.Secret{}
	secret.SetGroupVersionKind(v1.SchemeGroupVersion.WithKind("Secret"))

	secret.Name = name
	secret.Data = map[string][]byte{}

	for k, v := range fromLiterals {
		secret.Data[k] = []byte(v)
	}

	_, err := clientset.CoreV1().Secrets(namespace).Create(secret)
	if err != nil {
		log.WithError(err).Fatal("could not create generic secret")
	}
	log.Info(fmt.Sprintf("Created generic secret: %s", name))
}

// CreateTLSSecret creates a TLS secret in kubernetes.
func CreateTLSSecret(clientset *kubernetes.Clientset, namespace, name string, key string, cert string) {
	tlsCrt, err := readFile(cert)
	if err != nil {
		log.WithError(err).Fatal("failed to read cert for TLS secret")
	}
	tlsKey, err := readFile(key)
	if err != nil {
		log.WithError(err).Fatal("failed to read key for TLS secret")
	}

	if _, err := tls.X509KeyPair(tlsCrt, tlsKey); err != nil {
		log.WithError(err).Fatal("failed to load key pair for TLS secret")
	}

	secret := &v1.Secret{}
	secret.Name = name
	secret.Type = v1.SecretTypeTLS
	secret.Data = map[string][]byte{}
	secret.Data[v1.TLSCertKey] = []byte(tlsCrt)
	secret.Data[v1.TLSPrivateKeyKey] = []byte(tlsKey)

	_, err = clientset.CoreV1().Secrets(namespace).Create(secret)
	if err != nil {
		log.WithError(err).Fatal("could not create TLS secret")
	}
	log.Info(fmt.Sprintf("Created TLS secret: %s", name))
}

// CreateDockerConfigJSONSecret creates a secret in the docker config format.
// Currently the golang v1.Secret API doesn't perform the massaging of the credentials file that invoking
// kubectl with a docker-registry secret (like below) does.
func CreateDockerConfigJSONSecret(clientset *kubernetes.Clientset, namespace, name, credsFile string) {
	_, err := clientset.CoreV1().Secrets(namespace).Get(name, metav1.GetOptions{})
	if err == nil {
		DeleteSecret(clientset, namespace, name)
	}

	credsData, err := readFile(credsFile)
	if err != nil {
		log.WithError(err).Fatal(fmt.Sprintf("Could not read file: %s", credsFile))
	}

	kcmd := exec.Command("kubectl", "create", "secret", "docker-registry", name, "-n", namespace,
		"--docker-server=gcr.io", "--docker-username=_json_key",
		fmt.Sprintf("--docker-password=%s", credsData))

	var stderr bytes.Buffer
	kcmd.Stderr = &stderr
	err = kcmd.Run()
	if err != nil {
		log.WithError(err).Fatalf("failed to create docker-registry secret: %+v", stderr.String())
	}
	log.Info(fmt.Sprintf("Created secret: %s", name))
}

// readFile just reads a file into a byte array.
func readFile(file string) ([]byte, error) {
	b, err := ioutil.ReadFile(file)
	if err != nil {
		return []byte{}, fmt.Errorf("Cannot read file %v, %v", file, err)
	}
	return b, nil
}

func handleFromFileSources(secret *v1.Secret, fromFiles map[string]string) {
	for keyName, filePath := range fromFiles {
		err := addKeyFromFileToSecret(secret, keyName, filePath)
		if err != nil {
			log.WithError(err).Fatal("error adding key from file to secret")
		}
	}
}

func addKeyFromFileToSecret(secret *v1.Secret, keyName, filePath string) error {
	data, err := ioutil.ReadFile(filePath)
	if err != nil {
		return err
	}
	return addKeyFromLiteralToSecret(secret, keyName, data)
}

func addKeyFromLiteralToSecret(secret *v1.Secret, keyName string, data []byte) error {
	if errs := validation.IsConfigMapKey(keyName); len(errs) != 0 {
		return fmt.Errorf("%q is not a valid key name for a Secret: %s", keyName, strings.Join(errs, ";"))
	}

	if _, entryExists := secret.Data[keyName]; entryExists {
		return fmt.Errorf("cannot add key %s, another key by that name already exists", keyName)
	}
	secret.Data[keyName] = data
	return nil
}
