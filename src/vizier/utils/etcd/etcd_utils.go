package etcd

import (
	"context"
	"errors"

	etcd "github.com/coreos/etcd/clientv3"
	etcdpb "github.com/coreos/etcd/etcdserver/etcdserverpb"
)

// The maximum number of operations that can be done in a single transaction, set by the etcd cluster.
var maxTxnOps = 128

// The maximum number of bytes we should include a in single transaction (max allowed by etcd: 1.5MiB).
var maxNumBytes = 1048576 // (1 MiB)

func createBatches(ops []etcd.Op) ([][]etcd.Op, error) {
	var batches [][]etcd.Op

	var currBatch []etcd.Op
	currBatchBytes := 0

	for _, op := range ops {
		opBytes := len(op.ValueBytes())
		if opBytes > maxNumBytes {
			return nil, errors.New("etcd operation bytes larger than max request bytes")
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

func batchOps(ctx context.Context, client *etcd.Client, ops []etcd.Op) ([]*etcdpb.ResponseOp, error) {
	var totalOutput []*etcdpb.ResponseOp

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
