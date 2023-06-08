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
	"testing"
	"time"

	"github.com/stretchr/testify/require"
	corev1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/kubernetes/fake"

	"px.dev/pixie/src/utils"
)

const ConfigMapUID = "5b054918-d670-45f3-99d4-015bb07036b3"

func TestConfigMapScriptsSource(t *testing.T) {
	t.Run("returns the initial scripts from a configmap", func(t *testing.T) {
		client := fake.NewSimpleClientset(cronScriptConfigMap())
		source := NewConfigMapSource(client, "pl")
		initialScripts, err := source.Start(context.Background(), nil)
		require.Equal(t, err, nil)

		require.Len(t, initialScripts, 1)
		initialScript := initialScripts[ConfigMapUID]
		require.Equal(t, "px.display()", initialScript.Script)
		require.Equal(t, "otelEndpointConfig: {url: example.com}", initialScript.Configs)
		require.Equal(t, int64(1), initialScript.FrequencyS)
	})

	t.Run("excludes ConfigMaps in other namespaces", func(t *testing.T) {
		configMapTemplate := cronScriptConfigMap()
		configMapTemplate.Namespace = "other"
		client := fake.NewSimpleClientset(configMapTemplate)
		source := NewConfigMapSource(client, "pl")
		initialScripts, _ := source.Start(context.Background(), nil)

		require.Len(t, initialScripts, 0)
	})

	t.Run("excludes ConfigMaps without purpose=cron-script labels", func(t *testing.T) {
		configMapTemplate := cronScriptConfigMap()
		delete(configMapTemplate.Labels, "purpose")
		client := fake.NewSimpleClientset(configMapTemplate)
		source := NewConfigMapSource(client, "pl")
		initialScripts, _ := source.Start(context.Background(), nil)

		require.Len(t, initialScripts, 0)
	})

	t.Run("Stop causes no further updates to be sent", func(t *testing.T) {
		client := fake.NewSimpleClientset()
		updatesCh := mockUpdatesCh()
		syncConfigMaps := NewConfigMapSource(client, "pl")
		_, _ = syncConfigMaps.Start(context.Background(), updatesCh)
		syncConfigMaps.Stop()

		_, _ = client.CoreV1().ConfigMaps("pl").Create(context.Background(), cronScriptConfigMap(), metav1.CreateOptions{})

		requireNoReceive(t, updatesCh, 10*time.Millisecond)
	})

	t.Run("sends updates for created scripts", func(t *testing.T) {
		client := fake.NewSimpleClientset()
		updatesCh := mockUpdatesCh()
		_, _ = NewConfigMapSource(client, "pl").Start(context.Background(), updatesCh)

		configMap := cronScriptConfigMap()
		_, _ = client.CoreV1().ConfigMaps("pl").Create(context.Background(), configMap, metav1.CreateOptions{})

		cronScript := requireReceiveWithin(t, updatesCh, time.Second).GetUpsertReq().GetScript()
		require.Equal(t, utils.ProtoFromUUIDStrOrNil(ConfigMapUID), cronScript.GetID())
		require.Equal(t, "px.display()", cronScript.GetScript())
		require.Equal(t, "otelEndpointConfig: {url: example.com}", cronScript.GetConfigs())
		require.Equal(t, int64(1), cronScript.GetFrequencyS())
	})

	t.Run("sends updates for updated scripts", func(t *testing.T) {
		configMap := cronScriptConfigMap()
		client := fake.NewSimpleClientset(configMap)
		updatesCh := mockUpdatesCh()
		_, _ = NewConfigMapSource(client, "pl").Start(context.Background(), updatesCh)

		configMap.Data["script.pxl"] += "2"
		_, _ = client.CoreV1().ConfigMaps("pl").Update(context.Background(), configMap, metav1.UpdateOptions{})

		cronScript := requireReceiveWithin(t, updatesCh, time.Second).GetUpsertReq().GetScript()
		require.Equal(t, utils.ProtoFromUUIDStrOrNil(ConfigMapUID), cronScript.GetID())
		require.Equal(t, configMap.Data["script.pxl"], cronScript.GetScript())
	})

	t.Run("sends updates for deleted scripts", func(t *testing.T) {
		configMap := cronScriptConfigMap()
		client := fake.NewSimpleClientset(configMap)
		updatesCh := mockUpdatesCh()
		_, _ = NewConfigMapSource(client, "pl").Start(context.Background(), updatesCh)

		_ = client.CoreV1().ConfigMaps("pl").Delete(context.Background(), configMap.Name, metav1.DeleteOptions{})

		deletedID := requireReceiveWithin(t, updatesCh, time.Second).GetDeleteReq().ScriptID
		require.Equal(t, utils.ProtoFromUUIDStrOrNil(ConfigMapUID), deletedID)
	})
}

func cronScriptConfigMap(customizers ...func(*corev1.ConfigMap)) *corev1.ConfigMap {
	cm := &corev1.ConfigMap{
		ObjectMeta: metav1.ObjectMeta{
			UID:       ConfigMapUID,
			Name:      "cron-script-1",
			Namespace: "pl",
			Labels:    map[string]string{"purpose": "cron-script"},
		},
		Data: map[string]string{
			"script.pxl":   "px.display()",
			"configs.yaml": "otelEndpointConfig: {url: example.com}",
			"cron.yaml":    "frequency_s: 1",
		},
	}
	for _, customize := range customizers {
		customize(cm)
	}
	return cm
}
