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

package testingutils

import (
	"bytes"
	"context"
	"fmt"

	"cloud.google.com/go/storage"
	"github.com/googleapis/google-cloud-go-testing/storage/stiface"
)

// MockGCSClient is a mock of GCS's storage client.
type MockGCSClient struct {
	stiface.Client
	buckets map[string]*MockGCSBucket
}

// NewMockGCSClient creates a new mock client for GCS.
func NewMockGCSClient(buckets map[string]*MockGCSBucket) stiface.Client {
	return &MockGCSClient{buckets: buckets}
}

// MockGCSBucket is a mock of a GCS storage bucket.
type MockGCSBucket struct {
	attrs   *storage.BucketAttrs
	objects map[string]*MockGCSObject
}

// NewMockGCSBucket creates a new mock bucket for GCS.
func NewMockGCSBucket(objects map[string]*MockGCSObject, attrs *storage.BucketAttrs) *MockGCSBucket {
	return &MockGCSBucket{
		attrs:   attrs,
		objects: objects,
	}
}

// MockGCSObject is a mock of a GCS object, storing contents and attrs.
type MockGCSObject struct {
	contents []byte
	attrs    *storage.ObjectAttrs
}

// NewMockGCSObject creates a new mock GCS object.
func NewMockGCSObject(contents []byte, attrs *storage.ObjectAttrs) *MockGCSObject {
	return &MockGCSObject{
		contents: contents,
		attrs:    attrs,
	}
}

// Bucket gets a mock gcs bucket.
func (c *MockGCSClient) Bucket(name string) stiface.BucketHandle {
	return MockGCSBucketHandle{c: c, name: name}
}

// MockGCSBucketHandle is a mock of GCS storage client's BucketHandle.
type MockGCSBucketHandle struct {
	stiface.BucketHandle
	c    *MockGCSClient
	name string
}

// Object returns object handle from the given path.
func (b MockGCSBucketHandle) Object(name string) stiface.ObjectHandle {
	return MockGCSObjectHandle{c: b.c, bucketName: b.name, name: name}
}

// MockGCSObjectHandle is a mock of GCS storage client's ObjectHandle.
type MockGCSObjectHandle struct {
	stiface.ObjectHandle
	c          *MockGCSClient
	bucketName string
	name       string
}

func (o MockGCSObjectHandle) getObj() (*MockGCSObject, error) {
	bkt, ok := o.c.buckets[o.bucketName]
	if !ok {
		return nil, fmt.Errorf("bucket %q not found", o.bucketName)
	}
	obj, ok := bkt.objects[o.name]
	if !ok {
		return nil, fmt.Errorf("object %q not found in bucket %q", o.name, o.bucketName)
	}
	return obj, nil
}

// NewReader returns a new Reader for the object.
func (o MockGCSObjectHandle) NewReader(context.Context) (stiface.Reader, error) {
	obj, err := o.getObj()
	if err != nil {
		return nil, err
	}
	return fakeReader{r: bytes.NewReader(obj.contents)}, nil
}

type fakeReader struct {
	stiface.Reader
	r *bytes.Reader
}

func (r fakeReader) Read(buf []byte) (int, error) {
	return r.r.Read(buf)
}

func (r fakeReader) Close() error {
	return nil
}

// Attrs returns a new Mocked attrs object.
func (o MockGCSObjectHandle) Attrs(context.Context) (*storage.ObjectAttrs, error) {
	obj, err := o.getObj()
	if err != nil {
		return nil, err
	}
	return obj.attrs, nil
}
