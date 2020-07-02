package controller

import (
	"fmt"
	"strings"
)

// PrettifyClusterName uses heuristics to try to generate a better looking cluster name.
func PrettifyClusterName(name string) string {
	name = strings.ToLower(name)
	if strings.HasPrefix(name, "gke") {
		splits := strings.Split(name, "_")
		// GKE names are <gke>_<project>_<region>_<cluster_name>_<our suffix>
		if len(splits) > 3 && len(splits[3]) > 0 {
			return fmt.Sprintf("gke:%s", strings.Join(splits[3:], "_"))
		}
	} else if strings.HasPrefix(name, "arn") {
		// EKS names are "ARN::::CLUSTER/NAME"
		splits := strings.Split(name, ":")
		if len(splits) > 0 && len(splits[len(splits)-1]) > 0 {
			eksName := splits[len(splits)-1]
			sp := strings.Split(eksName, "/")
			if len(sp) > 0 && len(sp[1]) > 0 {
				eksName = sp[1]
			}
			return fmt.Sprintf("eks:%s", eksName)
		}
	} else if strings.HasPrefix(name, "aks-") {
		return fmt.Sprintf("aks:%s", strings.TrimPrefix(name, "aks-"))
	}
	return name
}
