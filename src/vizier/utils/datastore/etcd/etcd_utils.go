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

package etcd

import (
	"context"
	"errors"

	"go.etcd.io/etcd/api/v3/etcdserverpb"
	clientv3 "go.etcd.io/etcd/client/v3"
)

// The maximum number of operations that can be done in a single transaction, set by the clientv3 cluster.
var maxTxnOps = 128

// The maximum number of bytes we should include a in single transaction (max allowed by clientv3: 1.5MiB).
var maxNumBytes = 1048576 // (1 MiB)

func createBatches(ops []clientv3.Op) ([][]clientv3.Op, error) {
	var batches [][]clientv3.Op

	var currBatch []clientv3.Op
	currBatchBytes := 0

	for _, op := range ops {
		opBytes := len(op.ValueBytes())
		if opBytes > maxNumBytes {
			return nil, errors.New("clientv3 operation bytes larger than max request bytes")
		}

		if len(currBatch) == maxTxnOps || (currBatchBytes+opBytes > maxNumBytes) {
			batches = append(batches, currBatch)
			currBatch = nil
			currBatchBytes = 0
		}

		currBatch = append(currBatch, op)
		currBatchBytes += opBytes
	}

	if currBatch != nil {
		batches = append(batches, currBatch)
	}
	return batches, nil
}

func batchOps(ctx context.Context, client *clientv3.Client, ops []clientv3.Op) ([]*etcdserverpb.ResponseOp, error) {
	var totalOutput []*etcdserverpb.ResponseOp

	batches, err := createBatches(ops)
	if err != nil {
		return nil, err
	}

	for _, batch := range batches {
		output, err := client.Txn(ctx).If().Then(batch...).Commit()
		if err != nil {
			return nil, err
		}
		totalOutput = append(totalOutput, output.Responses...)
	}

	return totalOutput, nil
}
