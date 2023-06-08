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

package scriptrunner

import (
	"context"
	"reflect"
	"sync/atomic"
	"time"

	log "github.com/sirupsen/logrus"
	"gopkg.in/yaml.v2"
	corev1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/labels"
	"k8s.io/client-go/informers"
	informercorev1 "k8s.io/client-go/informers/core/v1"
	"k8s.io/client-go/kubernetes"
	listercorev1 "k8s.io/client-go/listers/core/v1"
	"k8s.io/client-go/tools/cache"

	"px.dev/pixie/src/shared/cvmsgspb"
	"px.dev/pixie/src/utils"
)

// ConfigMapSource pulls cron scripts from config maps.
type ConfigMapSource struct {
	stop     func()
	informer informercorev1.ConfigMapInformer
}

// NewConfigMapSource constructs a [Source] that extracts cron scripts from config maps with the label "purpose=cron-script".
// Each config map must contain
//   - a script.pxl with the pixel script
//   - a configs.yaml which will be stored in the Configs field of [cvmsgspb.CronScript]
//   - a cron.yaml that contains a "frequency_s" key
func NewConfigMapSource(client kubernetes.Interface, namespace string) *ConfigMapSource {
	return &ConfigMapSource{
		informer: informers.NewSharedInformerFactoryWithOptions(
			client,
			12*time.Hour,
			informers.WithNamespace(namespace),
			informers.WithTweakListOptions(func(options *metav1.ListOptions) {
				options.LabelSelector = "purpose=cron-script"
			}),
		).Core().V1().ConfigMaps(),
	}
}

// Start watches for updates to matching configmaps and sends resulting updates on updatesCh.
func (source *ConfigMapSource) Start(ctx context.Context, updatesCh chan<- *cvmsgspb.CronScriptUpdate) (map[string]*cvmsgspb.CronScript, error) {
	stopCh := make(chan struct{})
	isInitialized := &atomic.Bool{}
	_, err := source.informer.Informer().AddEventHandler(configMapEventHandlers(isInitialized, updatesCh))
	if err != nil {
		return nil, err
	}
	go source.informer.Informer().Run(stopCh)
	cache.WaitForCacheSync(ctx.Done(), source.informer.Informer().HasSynced)
	initialScripts, err := getInitialScripts(source.informer.Lister())
	if err != nil {
		close(stopCh)
		return nil, err
	}
	isInitialized.Store(true)
	source.stop = func() { close(stopCh) }
	return initialScripts, nil
}

func getInitialScripts(lister listercorev1.ConfigMapLister) (map[string]*cvmsgspb.CronScript, error) {
	configMaps, err := lister.List(labels.Everything())
	if err != nil {
		return nil, err
	}
	scripts := map[string]*cvmsgspb.CronScript{}
	for _, configMap := range configMaps {
		id, cronScript, err := configmapToCronScript(configMap)
		if err != nil {
			logCronScriptParseError(err)
			continue
		}
		scripts[id] = cronScript
	}
	return scripts, nil
}

func configMapEventHandlers(isInitialized *atomic.Bool, updatesCh chan<- *cvmsgspb.CronScriptUpdate) cache.ResourceEventHandlerFuncs {
	return cache.ResourceEventHandlerFuncs{
		AddFunc: func(obj interface{}) {
			configmap := obj.(*corev1.ConfigMap)
			if !isInitialized.Load() {
				return
			}
			updatesCh <- makeUpdate(configmap)
		},
		UpdateFunc: func(oldObj, newObj interface{}) {
			if reflect.DeepEqual(oldObj, newObj) {
				return
			}
			configmap := newObj.(*corev1.ConfigMap)
			updatesCh <- makeUpdate(configmap)
		},
		DeleteFunc: func(obj interface{}) {
			configmap := obj.(*corev1.ConfigMap)
			updatesCh <- makeDelete(configmap)
		},
	}
}

func makeDelete(configmap *corev1.ConfigMap) *cvmsgspb.CronScriptUpdate {
	_, script, err := configmapToCronScript(configmap)
	if err != nil {
		logCronScriptParseError(err)
		return nil
	}
	return &cvmsgspb.CronScriptUpdate{
		Msg: &cvmsgspb.CronScriptUpdate_DeleteReq{
			DeleteReq: &cvmsgspb.DeleteCronScriptRequest{
				ScriptID: script.ID,
			},
		},
		Timestamp: time.Now().Unix(),
	}
}

func makeUpdate(configmap *corev1.ConfigMap) *cvmsgspb.CronScriptUpdate {
	_, script, err := configmapToCronScript(configmap)
	if err != nil {
		logCronScriptParseError(err)
		return nil
	}
	return &cvmsgspb.CronScriptUpdate{
		Msg: &cvmsgspb.CronScriptUpdate_UpsertReq{
			UpsertReq: &cvmsgspb.RegisterOrUpdateCronScriptRequest{
				Script: script,
			},
		},
		Timestamp: time.Now().Unix(),
	}
}

// Stop stops further updates from being sent.
func (source *ConfigMapSource) Stop() {
	source.stop()
}

func logCronScriptParseError(err error) {
	log.WithError(err).Error("Failed to parse cron.yaml from configmap cron script")
}

func configmapToCronScript(configmap *corev1.ConfigMap) (string, *cvmsgspb.CronScript, error) {
	id := string(configmap.UID)
	cronScript := &cvmsgspb.CronScript{
		ID:      utils.ProtoFromUUIDStrOrNil(id),
		Script:  configmap.Data["script.pxl"],
		Configs: configmap.Data["configs.yaml"],
	}
	var cronData cronYAML
	err := yaml.Unmarshal([]byte(configmap.Data["cron.yaml"]), &cronData)
	if err != nil {
		return "", nil, err
	}
	cronScript.FrequencyS = cronData.FrequencyS
	return id, cronScript, nil
}

type cronYAML struct {
	FrequencyS int64 `yaml:"frequency_s"`
}
