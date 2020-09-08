package etcd

import (
	"context"
	"errors"

	v3 "github.com/coreos/etcd/clientv3"
	etcdpb "github.com/coreos/etcd/etcdserver/etcdserverpb"
)

// The maximum number of operations that can be done in a single transaction, set by the etcd cluster.
var maxTxnOps = 128

// The maximum number of bytes we should include a in single transaction (max allowed by etcd: 1.5MiB).
var maxNumBytes = 1048576 // (1 MiB)

// BatchOps performs the given transaction operations in batches.
func BatchOps(client *v3.Client, ops []v3.Op) ([]*etcdpb.ResponseOp, error) {
	var totalOutput []*etcdpb.ResponseOp
	currBatch := make([]v3.Op, 0)
	currBatchBytes := 0

	for _, op := range ops {
		opBytes := len(op.ValueBytes())
		if opBytes > maxNumBytes {
			return nil, errors.New("Etcd operation bytes larger than max request bytes")
		}

		if len(currBatch) == maxTxnOps || (currBatchBytes+opBytes > maxNumBytes) {
			output, err := client.Txn(context.TODO()).If().Then(currBatch...).Commit()
			if err != nil {
				return nil, err
			}
			totalOutput = append(totalOutput, output.Responses...)

			currBatch = make([]v3.Op, 0)
			currBatchBytes = 0
		}

		currBatch = append(currBatch, op)
		currBatchBytes = currBatchBytes + opBytes
	}

	if len(currBatch) > 0 {
		output, err := client.Txn(context.TODO()).If().Then(currBatch...).Commit()
		if err != nil {
			return nil, err
		}
		totalOutput = append(totalOutput, output.Responses...)
	}

	return totalOutput, nil

}
