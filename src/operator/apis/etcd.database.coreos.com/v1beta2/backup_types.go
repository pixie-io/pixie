// Copyright 2017 The etcd-operator Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

package v1beta2

import (
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
)

const (
	// AWS S3 related consts
	BackupStorageTypeS3          BackupStorageType = "S3"
	AWSSecretCredentialsFileName                   = "credentials"
	AWSSecretConfigFileName                        = "config"

	// Azure ABS related consts
	BackupStorageTypeABS      BackupStorageType = "ABS"
	AzureSecretStorageAccount                   = "storage-account"
	AzureSecretStorageKey                       = "storage-key"
	AzureCloudKey                               = "cloud"

	// Google GCS related consts
	BackupStorageTypeGCS BackupStorageType = "GCS"
	GCPAccessToken                         = "access-token"
	GCPCredentialsJson                     = "credentials.json"
)

type BackupStorageType string

// +k8s:deepcopy-gen:interfaces=k8s.io/apimachinery/pkg/runtime.Object

// EtcdBackupList is a list of EtcdBackup.
type EtcdBackupList struct {
	metav1.TypeMeta `json:",inline"`
	metav1.ListMeta `json:"metadata"`
	Items           []EtcdBackup `json:"items"`
}

// +genclient
// +k8s:deepcopy-gen:interfaces=k8s.io/apimachinery/pkg/runtime.Object

// EtcdBackup represents a Kubernetes EtcdBackup Custom Resource.
type EtcdBackup struct {
	metav1.TypeMeta   `json:",inline"`
	metav1.ObjectMeta `json:"metadata"`
	Spec              BackupSpec   `json:"spec"`
	Status            BackupStatus `json:"status,omitempty"`
}

// BackupSpec contains a backup specification for an etcd cluster.
type BackupSpec struct {
	// EtcdEndpoints specifies the endpoints of an etcd cluster.
	// When multiple endpoints are given, the backup operator retrieves
	// the backup from the endpoint that has the most up-to-date state.
	// The given endpoints must belong to the same etcd cluster.
	EtcdEndpoints []string `json:"etcdEndpoints,omitempty"`
	// StorageType is the etcd backup storage type.
	// We need this field because CRD doesn't support validation against invalid fields
	// and we cannot verify invalid backup storage source.
	StorageType BackupStorageType `json:"storageType"`
	// BackupPolicy configures the backup process.
	BackupPolicy *BackupPolicy `json:"backupPolicy,omitempty"`
	// BackupSource is the backup storage source.
	BackupSource `json:",inline"`
	// ClientTLSSecret is the secret containing the etcd TLS client certs and
	// must contain the following data items:
	// data:
	//    "etcd-client.crt": <pem-encoded-cert>
	//    "etcd-client.key": <pem-encoded-key>
	//    "etcd-client-ca.crt": <pem-encoded-ca-cert>
	ClientTLSSecret string `json:"clientTLSSecret,omitempty"`
}

// BackupSource contains the supported backup sources.
type BackupSource struct {
	// S3 defines the S3 backup source spec.
	S3 *S3BackupSource `json:"s3,omitempty"`
	// ABS defines the ABS backup source spec.
	ABS *ABSBackupSource `json:"abs,omitempty"`
	// GCS defines the GCS backup source spec.
	GCS *GCSBackupSource `json:"gcs,omitempty"`
}

// BackupPolicy defines backup policy.
type BackupPolicy struct {
	// TimeoutInSecond is the maximal allowed time in second of the entire backup process.
	TimeoutInSecond int64 `json:"timeoutInSecond,omitempty"`
	// BackupIntervalInSecond is to specify how often operator take snapshot
	// 0 is magic number to indicate one-shot backup
	BackupIntervalInSecond int64 `json:"backupIntervalInSecond,omitempty"`
	// MaxBackups is to specify how many backups we want to keep
	// 0 is magic number to indicate un-limited backups
	MaxBackups int `json:"maxBackups,omitempty"`
}

// BackupStatus represents the status of the EtcdBackup Custom Resource.
type BackupStatus struct {
	// Succeeded indicates if the backup has Succeeded.
	Succeeded bool `json:"succeeded"`
	// Reason indicates the reason for any backup related failures.
	Reason string `json:"Reason,omitempty"`
	// EtcdVersion is the version of the backup etcd server.
	EtcdVersion string `json:"etcdVersion,omitempty"`
	// EtcdRevision is the revision of etcd's KV store where the backup is performed on.
	EtcdRevision int64 `json:"etcdRevision,omitempty"`
	// LastSuccessDate indicate the time to get snapshot last time
	LastSuccessDate metav1.Time `json:"lastSuccessDate,omitempty"`
}

// S3BackupSource provides the spec how to store backups on S3.
type S3BackupSource struct {
	// Path is the full s3 path where the backup is saved.
	// The format of the path must be: "<s3-bucket-name>/<path-to-backup-file>"
	// e.g: "mybucket/etcd.backup"
	Path string `json:"path"`

	// The name of the secret object that stores the AWS credential and config files.
	// The file name of the credential MUST be 'credentials'.
	// The file name of the config MUST be 'config'.
	// The profile to use in both files will be 'default'.
	//
	// AWSSecret overwrites the default etcd operator wide AWS credential and config.
	AWSSecret string `json:"awsSecret"`

	// Endpoint if blank points to aws. If specified, can point to s3 compatible object
	// stores.
	Endpoint string `json:"endpoint,omitempty"`

	// ForcePathStyle forces to use path style over the default subdomain style.
	// This is useful when you have an s3 compatible endpoint that doesn't support
	// subdomain buckets.
	ForcePathStyle bool `json:"forcePathStyle"`
}

// ABSBackupSource provides the spec how to store backups on ABS.
type ABSBackupSource struct {
	// Path is the full abs path where the backup is saved.
	// The format of the path must be: "<abs-container-name>/<path-to-backup-file>"
	// e.g: "myabscontainer/etcd.backup"
	Path string `json:"path"`

	// The name of the secret object that stores the Azure storage credential
	ABSSecret string `json:"absSecret"`
}

// GCSBackupSource provides the spec how to store backups on GCS.
type GCSBackupSource struct {
	// Path is the full GCS path where the backup is saved.
	// The format of the path must be: "<gcs-bucket-name>/<path-to-backup-file>"
	// e.g: "mygcsbucket/etcd.backup"
	Path string `json:"path"`

	// The name of the secret object that stores the Google storage credential
	// containing at most ONE of the following:
	// An access token with file name of 'access-token'.
	// JSON credentials with file name of 'credentials.json'.
	//
	// If omitted, client will use the default application credentials.
	GCPSecret string `json:"gcpSecret,omitempty"`
}
