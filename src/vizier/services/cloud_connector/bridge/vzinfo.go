package bridge

import (
	"context"
	"errors"
	"strings"
	"sync"
	"time"

	batchv1 "k8s.io/api/batch/v1"
	corev1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/fields"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/kubernetes/scheme"

	// Blank import necessary for kubeConfig to work.
	_ "k8s.io/client-go/plugin/pkg/client/auth/gcp"
	"k8s.io/client-go/rest"
	"k8s.io/client-go/tools/cache"

	"pixielabs.ai/pixielabs/src/shared/cvmsgspb"
	protoutils "pixielabs.ai/pixielabs/src/shared/k8s"
	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	"pixielabs.ai/pixielabs/src/shared/version"
)

// TODO(michelle): Make namespace a flag that can be passed in.
const plNamespace = "pl"

const podUpdatePeriod = 10 * time.Second

// K8sVizierInfo is responsible for fetching Vizier information through K8s.
type K8sVizierInfo struct {
	clientset            *kubernetes.Clientset
	clusterVersion       string
	clusterName          string
	currentPodStatus     map[string]*cvmsgspb.PodStatus
	podStatusLastUpdated time.Time
	mu                   sync.Mutex
}

// NewK8sVizierInfo creates a new K8sVizierInfo.
func NewK8sVizierInfo(clusterVersion string, clusterName string) (*K8sVizierInfo, error) {
	// There is a specific config for services running in the cluster.
	kubeConfig, err := rest.InClusterConfig()
	if err != nil {
		return nil, err
	}

	// Create k8s client.
	clientset, err := kubernetes.NewForConfig(kubeConfig)
	if err != nil {
		return nil, err
	}

	vzInfo := &K8sVizierInfo{
		clientset:      clientset,
		clusterVersion: clusterVersion,
		clusterName:    clusterName,
	}

	go func() {
		for _ = time.Tick(podUpdatePeriod); ; {
			vzInfo.UpdatePodStatuses()
		}
	}()

	return vzInfo, nil
}

// GetVizierClusterInfo gets the K8s cluster info for the current running vizier.
func (v *K8sVizierInfo) GetVizierClusterInfo() (*cvmsgspb.VizierClusterInfo, error) {
	clusterUID, err := v.GetClusterUID()
	if err != nil {
		return nil, err
	}
	return &cvmsgspb.VizierClusterInfo{
		ClusterUID:     clusterUID,
		ClusterName:    v.clusterName,
		ClusterVersion: v.clusterVersion,
		VizierVersion:  version.GetVersion().ToString(),
	}, nil
}

// GetAddress gets the external address of Vizier's proxy service.
func (v *K8sVizierInfo) GetAddress() (string, int32, error) {
	// TODO(michelle): Make the service name a flag that can be passed in.
	proxySvc, err := v.clientset.CoreV1().Services(plNamespace).Get(context.Background(), "vizier-proxy-service", metav1.GetOptions{})
	if err != nil {
		return "", int32(0), err
	}

	ip := ""
	port := int32(0)

	if proxySvc.Spec.Type == corev1.ServiceTypeNodePort {
		nodesList, err := v.clientset.CoreV1().Nodes().List(context.Background(), metav1.ListOptions{})
		if err != nil {
			return "", int32(0), err
		}
		if len(nodesList.Items) == 0 {
			return "", int32(0), errors.New("Could not find node for NodePort")
		}

		// Just select the first node for now.
		// Don't want to randomly pick, because it'll affect DNS.
		nodeAddrs := nodesList.Items[0].Status.Addresses

		for _, nodeAddr := range nodeAddrs {
			if nodeAddr.Type == corev1.NodeInternalIP {
				ip = nodeAddr.Address
			}
		}
		if ip == "" {
			return "", int32(0), errors.New("Could not determine IP address of node for NodePort")
		}

		for _, portSpec := range proxySvc.Spec.Ports {
			if portSpec.Name == "tcp-https" {
				port = portSpec.NodePort
			}
		}
		if port <= 0 {
			return "", int32(0), errors.New("Could not determine port for vizier service")
		}
	} else if proxySvc.Spec.Type == corev1.ServiceTypeLoadBalancer {
		// It's possible to have more than one external IP. Just select the first one for now.
		if len(proxySvc.Status.LoadBalancer.Ingress) == 0 {
			return "", int32(0), errors.New("Proxy service has no external IPs")
		}
		ip = proxySvc.Status.LoadBalancer.Ingress[0].IP

		for _, portSpec := range proxySvc.Spec.Ports {
			if portSpec.Name == "tcp-https" {
				port = portSpec.Port
			}
		}
		if port <= 0 {
			return "", int32(0), errors.New("Could not determine port for vizier service")
		}
	} else {
		return "", int32(0), errors.New("Unexpected service type")
	}

	externalAddr := ip

	return externalAddr, port, nil
}

// GetClusterUID gets UID for the cluster, represented by the kube-system namespace UID.
func (v *K8sVizierInfo) GetClusterUID() (string, error) {
	ksNS, err := v.clientset.CoreV1().Namespaces().Get(context.Background(), "kube-system", metav1.GetOptions{})
	if err != nil {
		return "", err
	}
	return string(ksNS.UID), nil
}

// UpdatePodStatuses gets the status of the pods at the current moment in time.
func (v *K8sVizierInfo) UpdatePodStatuses() {

	podsList, err := v.clientset.CoreV1().Pods(plNamespace).List(context.Background(), metav1.ListOptions{})
	if err != nil {
		return
	}

	now := time.Now()

	podMap := make(map[string]*cvmsgspb.PodStatus)
	for _, p := range podsList.Items {
		podPb, err := protoutils.PodToProto(&p)
		if err != nil {
			return
		}

		status := metadatapb.PHASE_UNKNOWN
		if podPb.Status != nil {
			status = podPb.Status.Phase
		}
		name := podPb.Metadata.Name

		s := &cvmsgspb.PodStatus{
			Name:   name,
			Status: status,
		}
		podMap[name] = s
	}

	v.mu.Lock()
	defer v.mu.Unlock()

	v.currentPodStatus = podMap
	v.podStatusLastUpdated = now
}

// GetPodStatuses gets the pod statuses and the last time they were updated.
func (v *K8sVizierInfo) GetPodStatuses() (map[string]*cvmsgspb.PodStatus, time.Time) {
	v.mu.Lock()
	defer v.mu.Unlock()

	return v.currentPodStatus, v.podStatusLastUpdated
}

// ParseJobYAML parses the yaml string into a k8s job and applies the image tag and env subtitutions.
func (v *K8sVizierInfo) ParseJobYAML(yamlStr string, imageTag map[string]string, envSubtitutions map[string]string) (*batchv1.Job, error) {
	decode := scheme.Codecs.UniversalDeserializer().Decode
	obj, _, err := decode([]byte(yamlStr), nil, nil)
	if err != nil {
		return nil, err
	}

	job, ok := obj.(*batchv1.Job)
	if !ok {
		return nil, errors.New("YAML could not be decoded to job")
	}

	// Add proper image tag and env substitutions.
	for i, c := range job.Spec.Template.Spec.Containers {
		if val, ok := imageTag[c.Name]; ok {
			imgTag := strings.Split(job.Spec.Template.Spec.Containers[i].Image, ":")
			job.Spec.Template.Spec.Containers[i].Image = imgTag[0] + ":" + val
		}
		for j, e := range c.Env {
			if val, ok := envSubtitutions[e.Name]; ok {
				job.Spec.Template.Spec.Containers[i].Env[j].Value = val
			}
		}
	}

	return job, nil
}

// LaunchJob starts the specified job.
func (v *K8sVizierInfo) LaunchJob(j *batchv1.Job) (*batchv1.Job, error) {
	// TODO(michelle): Don't hardcode namespace
	job, err := v.clientset.BatchV1().Jobs("pl").Create(context.Background(), j, metav1.CreateOptions{})
	if err != nil {
		return nil, err
	}

	return job, nil
}

// CreateSecret creates the K8s secret.
func (v *K8sVizierInfo) CreateSecret(name string, literals map[string]string) error {
	// Attempt to delete the secret first, if it already exists.
	v.clientset.CoreV1().Secrets(plNamespace).Delete(context.Background(), name, metav1.DeleteOptions{})

	secret := &corev1.Secret{}
	secret.SetGroupVersionKind(corev1.SchemeGroupVersion.WithKind("Secret"))

	secret.Name = name
	secret.Data = map[string][]byte{}

	for k, v := range literals {
		secret.Data[k] = []byte(v)
	}

	_, err := v.clientset.CoreV1().Secrets(plNamespace).Create(context.Background(), secret, metav1.CreateOptions{})
	if err != nil {
		return err
	}
	return nil
}

// DeleteJob deletes the job with the specified name.
func (v *K8sVizierInfo) DeleteJob(name string) error {
	policy := metav1.DeletePropagationBackground

	return v.clientset.BatchV1().Jobs(plNamespace).Delete(context.Background(), name, metav1.DeleteOptions{
		PropagationPolicy: &policy,
	})
}

// GetJob gets the job with the specified name.
func (v *K8sVizierInfo) GetJob(name string) (*batchv1.Job, error) {
	return v.clientset.BatchV1().Jobs(plNamespace).Get(context.Background(), name, metav1.GetOptions{})
}

// WaitForJobCompletion waits for the job with given name to complete.
func (v *K8sVizierInfo) WaitForJobCompletion(name string) (bool, error) {
	watcher := cache.NewListWatchFromClient(v.clientset.BatchV1().RESTClient(), "jobs", plNamespace, fields.OneTermEqualSelector("metadata.name", name))

	w, err := watcher.Watch(metav1.ListOptions{})
	if err != nil {
		return false, err
	}

	for c := range w.ResultChan() {
		o, ok := c.Object.(*batchv1.Job)
		if ok {
			if !o.Status.CompletionTime.IsZero() {
				return true, nil // Job has completed.
			}
			if o.Status.Failed > 0 {
				return false, nil
			}
		}
	}
	return true, nil
}
