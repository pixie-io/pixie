package bridge

import (
	"context"
	"errors"
	"strings"
	"sync"
	"time"

	"github.com/blang/semver"
	"github.com/gogo/protobuf/types"
	log "github.com/sirupsen/logrus"
	batchv1 "k8s.io/api/batch/v1"
	corev1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/fields"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/kubernetes/scheme"

	// Blank import necessary for kubeConfig to work.
	"k8s.io/client-go/discovery"
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

const k8sStateUpdatePeriod = 10 * time.Second

const privateImageRepo = "gcr.io/pl-dev-infra"
const publicImageRepo = "gcr.io/pixie-prod"

// K8sJobHandler manages k8s jobs.
// TODO(michelle): Refactor and move job-related operations from the VizierInfo
// interface here.
type K8sJobHandler interface {
	CleanupCronJob(string, time.Duration, chan bool)
}

// K8sVizierInfo is responsible for fetching Vizier information through K8s.
type K8sVizierInfo struct {
	clientset            *kubernetes.Clientset
	clusterVersion       string
	clusterName          string
	currentPodStatus     map[string]*cvmsgspb.PodStatus
	k8sStateLastUpdated  time.Time
	numNodes             int32
	numInstrumentedNodes int32
	mu                   sync.Mutex
}

// NewK8sVizierInfo creates a new K8sVizierInfo.
func NewK8sVizierInfo(clusterName string) (*K8sVizierInfo, error) {
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

	clusterVersion := ""

	discoveryClient, err := discovery.NewDiscoveryClientForConfig(kubeConfig)
	if err != nil {
		log.WithError(err).Error("Failed to get discovery client from kubeConfig")
	}

	version, err := discoveryClient.ServerVersion()
	if err != nil {
		log.WithError(err).Error("Failed to get server version from discovery client")
	} else {
		clusterVersion = version.GitVersion
	}

	vzInfo := &K8sVizierInfo{
		clientset:      clientset,
		clusterVersion: clusterVersion,
		clusterName:    clusterName,
	}

	go func() {
		for _ = time.Tick(k8sStateUpdatePeriod); ; {
			vzInfo.UpdateK8sState()
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

const nanosPerSecond = int64(1000 * 1000)

func nanosToTimestampProto(nanos int64) *types.Timestamp {
	seconds := nanos / nanosPerSecond
	remainderNanos := int32(nanos % nanosPerSecond)
	return &types.Timestamp{
		Seconds: seconds,
		Nanos:   remainderNanos,
	}
}

// GetPodLogs gets the k8s logs for the pod with the given name.
func (v *K8sVizierInfo) GetPodLogs(podName string, previous bool, container string) (string, error) {
	resp := v.clientset.CoreV1().Pods(plNamespace).GetLogs(podName, &corev1.PodLogOptions{Previous: previous, Container: container}).Do(context.Background())
	rawResp, err := resp.Raw()
	if err != nil {
		return "", err
	}
	return string(rawResp), nil
}

// UpdateK8sState gets the relevant state of the cluster, such as pod statuses, at the current moment in time.
func (v *K8sVizierInfo) UpdateK8sState() {
	nodesList, err := v.clientset.CoreV1().Nodes().List(context.Background(), metav1.ListOptions{})
	if err != nil {
		return
	}
	// Get only control-plane pods.
	cpPodsList, err := v.clientset.CoreV1().Pods(plNamespace).List(context.Background(), metav1.ListOptions{
		LabelSelector: "plane=control",
	})
	if err != nil {
		return
	}
	// Get only pem.
	pemPodsList, err := v.clientset.CoreV1().Pods(plNamespace).List(context.Background(), metav1.ListOptions{
		LabelSelector: "name=vizier-pem",
	})
	if err != nil {
		return
	}
	// Get the count of healthy PEMs.
	healthyPemCount := 0
	for _, p := range pemPodsList.Items {
		podPb, err := protoutils.PodToProto(&p)
		if err != nil {
			return
		}

		if podPb.Status != nil && podPb.Status.Phase == metadatapb.RUNNING {
			healthyPemCount++
		}
	}

	now := time.Now()

	podMap := make(map[string]*cvmsgspb.PodStatus)
	for _, p := range cpPodsList.Items {
		podPb, err := protoutils.PodToProto(&p)
		if err != nil {
			return
		}

		status := metadatapb.PHASE_UNKNOWN
		msg := ""
		containers := make([]*cvmsgspb.ContainerStatus, 0)
		if podPb.Status != nil {
			status = podPb.Status.Phase
			msg = podPb.Status.Reason
			for _, c := range podPb.Status.ContainerStatuses {
				containers = append(containers, &cvmsgspb.ContainerStatus{
					Name:      c.Name,
					Message:   c.Message,
					Reason:    c.Reason,
					State:     c.ContainerState,
					CreatedAt: nanosToTimestampProto(c.StartTimestampNS),
				})
			}
		}
		name := podPb.Metadata.Name
		ns := plNamespace
		events := make([]*cvmsgspb.K8SEvent, 0)

		eventsInterface := v.clientset.CoreV1().Events(plNamespace)
		selector := eventsInterface.GetFieldSelector(&name, &ns, nil, nil)
		options := metav1.ListOptions{FieldSelector: selector.String()}
		evs, err := eventsInterface.List(context.Background(), options)

		if err == nil {
			// Limit to last 5 events.
			start := len(evs.Items) - 5
			if start < 0 {
				start = 0
			}
			end := len(evs.Items)

			for start < end {
				e := evs.Items[start]
				events = append(events, &cvmsgspb.K8SEvent{
					Message:   e.Message,
					FirstTime: nanosToTimestampProto(e.FirstTimestamp.UnixNano()),
					LastTime:  nanosToTimestampProto(e.LastTimestamp.UnixNano()),
				})
				start++
			}
		} else {
			log.WithError(err).Info("Error getting K8s events")
		}

		s := &cvmsgspb.PodStatus{
			Name:          name,
			Status:        status,
			StatusMessage: msg,
			Containers:    containers,
			CreatedAt:     nanosToTimestampProto(podPb.Metadata.CreationTimestampNS),
			Events:        events,
		}
		podMap[name] = s
	}

	v.mu.Lock()
	defer v.mu.Unlock()

	v.currentPodStatus = podMap
	v.k8sStateLastUpdated = now
	v.numNodes = int32(len(nodesList.Items))
	v.numInstrumentedNodes = int32(healthyPemCount)
}

// GetPodStatuses gets the pod statuses and the last time they were updated.
func (v *K8sVizierInfo) GetK8sState() (map[string]*cvmsgspb.PodStatus, int32, int32, time.Time) {
	v.mu.Lock()
	defer v.mu.Unlock()

	return v.currentPodStatus, v.numNodes, v.numInstrumentedNodes, v.k8sStateLastUpdated
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
			imgPath := imgTag[0]

			tagVers := semver.MustParse(val)

			repoPath := publicImageRepo
			if len(tagVers.Pre) > 0 {
				repoPath = privateImageRepo
			}
			imgPath = repoPath + imgPath[strings.Index(imgPath, "/vizier/"):]

			job.Spec.Template.Spec.Containers[i].Image = imgPath + ":" + val
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

// CleanupCronJob periodically cleans up any completed jobs that were run by the specified cronjob.
func (v *K8sVizierInfo) CleanupCronJob(cronJob string, duration time.Duration, quitCh chan bool) {
	policy := metav1.DeletePropagationBackground
	ticker := time.NewTicker(duration)
	defer ticker.Stop()
	for {
		select {
		case <-ticker.C:
			jobs, err := v.clientset.BatchV1().Jobs(plNamespace).List(context.Background(), metav1.ListOptions{})
			if err != nil {
				log.WithError(err).Error("Could not list jobs")
				continue
			}
			for _, j := range jobs.Items {
				if len(j.ObjectMeta.OwnerReferences) > 0 && j.ObjectMeta.OwnerReferences[0].Name == cronJob && j.Status.Succeeded == 1 {
					err = v.clientset.BatchV1().Jobs(plNamespace).Delete(context.Background(), j.ObjectMeta.Name, metav1.DeleteOptions{
						PropagationPolicy: &policy,
					})
					if err != nil {
						log.WithError(err).Error("Could not delete job")
					}
				}
			}
		case <-quitCh:
			return
		}
	}
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

// UpdateClusterID updates the cluster ID in the cluster secrets.
func (v *K8sVizierInfo) UpdateClusterID(id string) error {
	s, err := v.clientset.CoreV1().Secrets(plNamespace).Get(context.Background(), "pl-cluster-secrets", metav1.GetOptions{})
	if err != nil {
		return err
	}
	s.Data["cluster-id"] = []byte(id)

	s, err = v.clientset.CoreV1().Secrets(plNamespace).Update(context.Background(), s, metav1.UpdateOptions{})
	return err
}
