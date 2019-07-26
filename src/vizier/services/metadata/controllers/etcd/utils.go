package etcd

import (
	"context"
	"math"

	v3 "github.com/coreos/etcd/clientv3"
	etcdpb "github.com/coreos/etcd/etcdserver/etcdserverpb"
)

// The maximum number of operations that can be done in a single transaction, set by the etcd cluster.
var maxTxnOps = 128

// BatchOps performs the given transaction operations in batches.
func BatchOps(client *v3.Client, ops []v3.Op) ([]*etcdpb.ResponseOp, error) {
	batch := 0
	var totalOutput []*etcdpb.ResponseOp
	var err error
	for batch*maxTxnOps < len(ops) {

		batchSlice := ops[batch*maxTxnOps : int(math.Min(float64((batch+1)*maxTxnOps), float64(len(ops))))]
		output, e := client.Txn(context.TODO()).If().Then(batchSlice...).Commit()
		totalOutput = append(totalOutput, output.Responses...)
		if e != nil {
			err = e
		}
		batch++
	}
	return totalOutput, err
}
