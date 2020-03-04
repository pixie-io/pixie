/*
Adapted from https://github.com/kubernetes/client-go/blob/master/examples/leader-election/main.go
*/

package election

import (
	"context"
	"errors"
	"os"
	"os/signal"
	"syscall"
	"time"

	log "github.com/sirupsen/logrus"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	clientset "k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"
	"k8s.io/client-go/tools/clientcmd"
	"k8s.io/client-go/tools/leaderelection"
	"k8s.io/client-go/tools/leaderelection/resourcelock"
)

// K8sLeaderElectionMgr manages leader election among cloud connectors by talking to the leader election side car.
type K8sLeaderElectionMgr struct {
	// The name of the election to run.
	name string
	// The namespace to run the election in.
	namespace string
	// Wait time in seconds.
	waitTime float64
	// Kube config to use.
	kubeConfig string
}

// NewK8sLeaderElectionMgr creates a K8sLeaderElectionMgr.
func NewK8sLeaderElectionMgr(electionNamespace string, leaderElectionWaitS float64) (*K8sLeaderElectionMgr, error) {
	if electionNamespace == "" {
		return nil, errors.New("namespace must be specified for leader election")
	}
	// Might add more complex logic that necessitates errors, but for now not included.
	return &K8sLeaderElectionMgr{namespace: electionNamespace, name: "cloud-conn-election", waitTime: leaderElectionWaitS, kubeConfig: ""}, nil
}

func buildConfig(kubeconfig string) (*rest.Config, error) {
	if kubeconfig != "" {
		cfg, err := clientcmd.BuildConfigFromFlags("", kubeconfig)
		if err != nil {
			return nil, err
		}
		return cfg, nil
	}

	cfg, err := rest.InClusterConfig()
	if err != nil {
		return nil, err
	}
	return cfg, nil
}

// runElection manages the election.
func (le *K8sLeaderElectionMgr) runElection(id string, callback func(string)) {

	// leader election uses the Kubernetes API by writing to a
	// lock object, which can be a LeaseLock object (preferred),
	// a ConfigMap, or an Endpoints (deprecated) object.
	// Conflicting writes are detected and each client handles those actions
	// independently.
	config, err := buildConfig(le.kubeConfig)
	if err != nil {
		log.Fatal(err)
	}
	client := clientset.NewForConfigOrDie(config)

	// use a Go context so we can tell the leaderelection code when we
	// want to step down
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	// listen for interrupts or the Linux SIGTERM signal and cancel
	// our context, which the leader election code will observe and
	// step down
	ch := make(chan os.Signal, 1)
	signal.Notify(ch, os.Interrupt, syscall.SIGTERM)
	go func() {
		<-ch
		log.Info("Received termination, signaling shutdown")
		cancel()
	}()

	// we use the Lease lock type since edits to Leases are less common
	// and fewer objects in the cluster watch "all Leases".
	lock := &resourcelock.EndpointsLock{
		EndpointsMeta: metav1.ObjectMeta{
			Name:      le.name,
			Namespace: le.namespace,
		},
		Client: client.CoreV1(),
		LockConfig: resourcelock.ResourceLockConfig{
			Identity: id,
		},
	}

	// start the leader election code loop
	leaderelection.RunOrDie(ctx, leaderelection.LeaderElectionConfig{
		Lock: lock,
		// IMPORTANT: you MUST ensure that any code you have that
		// is protected by the lease must terminate **before**
		// you call cancel. Otherwise, you could have a background
		// loop still running and another process could
		// get elected before your background loop finished, violating
		// the stated goal of the lease.
		ReleaseOnCancel: true,
		LeaseDuration:   60 * time.Second,
		RenewDeadline:   15 * time.Second,
		RetryPeriod:     5 * time.Second,
		Callbacks: leaderelection.LeaderCallbacks{
			OnStartedLeading: func(ctx context.Context) {
				// we're notified when we start - this is where you would
				// usually put your code
			},
			OnStoppedLeading: func() {
				// we can do cleanup here
				log.Infof("leader lost: %s", id)
			},
			OnNewLeader: callback,
		},
	})
}

type leader struct {
	Name string `json:"name"`
}

// WaitForElection blocks until this node is the leader.
func (le *K8sLeaderElectionMgr) WaitForElection() error {
	l := &leader{}
	fn := func(str string) {
		l.Name = str
	}

	// Get the hostname of this pod.
	podHostname, err := os.Hostname()
	if err != nil {
		return err
	}

	go le.runElection(podHostname, fn)

	// Loop until we are elected.
	for {
		// If the leader is the pod then we exit the loop.
		if l.Name == podHostname {
			log.Infof("Pod '%s' is now leader.", podHostname)
			return nil
		}
		if l.Name != "" {
			// If this pod is not the leader then continue to wait.
			log.Infof("Pod '%s' not yet elected leader. Current leader is '%s'.", podHostname, l.Name)
		}
		time.Sleep(time.Duration(le.waitTime) * time.Second)
	}
}
