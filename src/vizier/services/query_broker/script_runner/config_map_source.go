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
	"time"

	log "github.com/sirupsen/logrus"
	"gopkg.in/yaml.v2"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/watch"
	clientv1 "k8s.io/client-go/kubernetes/typed/core/v1"

	"px.dev/pixie/src/shared/cvmsgspb"
	"px.dev/pixie/src/utils"
)

// ConfigMapSource pulls cron scripts from config maps.
type ConfigMapSource struct {
	scripts map[string]*cvmsgspb.CronScript
	stop    func()
	client  clientv1.ConfigMapInterface
}

// NewConfigMapSource constructs a [Source] that extracts cron scripts from config maps with the label "purpose=cron-script".
// Each config map must contain
//   - a script.pxl with the pixel script
//   - a configs.yaml which will be stored in the Configs field of [cvmsgspb.CronScript]
//   - a cron.yaml that contains a "frequency_s" key
func NewConfigMapSource(client clientv1.ConfigMapInterface) *ConfigMapSource {
	return &ConfigMapSource{client: client}
}

// Start watches for updates to matching configmaps and sends resulting updates on updatesCh.
func (source *ConfigMapSource) Start(baseCtx context.Context, updatesCh chan<- *cvmsgspb.CronScriptUpdate) error {
	options := metav1.ListOptions{LabelSelector: "purpose=cron-script"}
	watcher, err := source.client.Watch(baseCtx, options)
	if err != nil {
		return err
	}
	go configMapUpdater(watcher, updatesCh)
	configmaps, err := source.client.List(baseCtx, options)
	if err != nil {
		watcher.Stop()
		return err
	}
	scripts := map[string]*cvmsgspb.CronScript{}
	for _, configmap := range configmaps.Items {
		id, cronScript, err := configmapToCronScript(&configmap)
		if err != nil {
			logCronScriptParseError(err)
			continue
		}
		scripts[id] = cronScript
	}
	source.scripts = scripts
	source.stop = watcher.Stop
	return nil
}

// GetInitialScripts returns the initial set of scripts that all updates will be based on.
func (source *ConfigMapSource) GetInitialScripts() map[string]*cvmsgspb.CronScript {
	return source.scripts
}

// Stop stops further updates from being sent.
func (source *ConfigMapSource) Stop() {
	source.stop()
}

func configMapUpdater(watcher watch.Interface, updatesCh chan<- *cvmsgspb.CronScriptUpdate) {
	for event := range watcher.ResultChan() {
		switch event.Type {
		case watch.Modified, watch.Added:
			configmap := event.Object.(*v1.ConfigMap)
			id, script, err := configmapToCronScript(configmap)
			if err != nil {
				logCronScriptParseError(err)
				continue
			}
			cronScriptUpdate := &cvmsgspb.CronScriptUpdate{
				Msg: &cvmsgspb.CronScriptUpdate_UpsertReq{
					UpsertReq: &cvmsgspb.RegisterOrUpdateCronScriptRequest{
						Script: script,
					},
				},
				RequestID: id,
				Timestamp: time.Now().Unix(),
			}
			updatesCh <- cronScriptUpdate
		case watch.Deleted:
			configmap := event.Object.(*v1.ConfigMap)
			id, script, err := configmapToCronScript(configmap)
			if err != nil {
				logCronScriptParseError(err)
				continue
			}
			cronScriptUpdate := &cvmsgspb.CronScriptUpdate{
				Msg: &cvmsgspb.CronScriptUpdate_DeleteReq{
					DeleteReq: &cvmsgspb.DeleteCronScriptRequest{
						ScriptID: script.ID,
					},
				},
				RequestID: id,
				Timestamp: time.Now().Unix(),
			}
			updatesCh <- cronScriptUpdate
		}
	}
}

func logCronScriptParseError(err error) {
	log.WithError(err).Error("Failed to parse cron.yaml from configmap cron script")
}

func configmapToCronScript(configmap *v1.ConfigMap) (string, *cvmsgspb.CronScript, error) {
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
