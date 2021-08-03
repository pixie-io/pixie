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

package utils

import (
	"crypto/rand"
	"crypto/rsa"
	"encoding/json"

	"github.com/gogo/protobuf/proto"
	"github.com/lestrrat-go/jwx/jwa"
	"github.com/lestrrat-go/jwx/jwe"
	"github.com/lestrrat-go/jwx/jwk"

	"px.dev/pixie/src/api/proto/vizierpb"
)

// CreateEncryptionOptions creates a RSA Key pair and wraps it into a set of
// encryptionOpts and decryptionOpts and returns those for use.
func CreateEncryptionOptions() (*vizierpb.ExecuteScriptRequest_EncryptionOptions, *vizierpb.ExecuteScriptRequest_EncryptionOptions, error) {
	privKey, err := rsa.GenerateKey(rand.Reader, 4096)
	if err != nil {
		return nil, nil, err
	}

	jwkPublic, err := jwk.New(privKey.PublicKey)
	if err != nil {
		return nil, nil, err
	}

	jwkPublicJSON, err := json.Marshal(jwkPublic)
	if err != nil {
		return nil, nil, err
	}

	jwkPrivate, err := jwk.New(privKey)
	if err != nil {
		return nil, nil, err
	}

	jwkPrivateJSON, err := json.Marshal(jwkPrivate)
	if err != nil {
		return nil, nil, err
	}

	encOpts := &vizierpb.ExecuteScriptRequest_EncryptionOptions{
		JwkKey:         string(jwkPublicJSON),
		KeyAlg:         jwa.RSA_OAEP_256.String(),
		ContentAlg:     jwa.A256GCM.String(),
		CompressionAlg: jwa.Deflate.String(),
	}

	decOpts := &vizierpb.ExecuteScriptRequest_EncryptionOptions{
		JwkKey:         string(jwkPrivateJSON),
		KeyAlg:         jwa.RSA_OAEP_256.String(),
		ContentAlg:     jwa.A256GCM.String(),
		CompressionAlg: jwa.Deflate.String(),
	}
	return encOpts, decOpts, nil
}

// DecodeRowBatch uses the decoder option to decode the RowBatch data.
func DecodeRowBatch(decOpts *vizierpb.ExecuteScriptRequest_EncryptionOptions, encodedBatch []byte) (*vizierpb.RowBatchData, error) {
	privKey := &rsa.PrivateKey{}
	err := jwk.ParseRawKey([]byte(decOpts.JwkKey), privKey)
	if err != nil {
		return nil, err
	}
	var keyAlg jwa.KeyEncryptionAlgorithm
	err = keyAlg.Accept(decOpts.KeyAlg)
	if err != nil {
		return nil, err
	}
	decrypted, err := jwe.Decrypt(encodedBatch, keyAlg, privKey)
	if err != nil {
		return nil, err
	}
	batch := &vizierpb.RowBatchData{}
	err = proto.Unmarshal(decrypted, batch)
	if err != nil {
		return nil, err
	}
	return batch, nil
}
