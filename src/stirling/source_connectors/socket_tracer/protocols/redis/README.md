# Redis tracing

This directory contains APIs used by Stirling's protocol tracing pipeline.

## Testing Redis failover setup

Redis failover is similar to the primary/backup in distributed systems. It's one of the Redis
clustering architecture [1]. The other technique is sharding [2], which is to distribute keys among
multiple coordinated Redis server instances.

To deploy a simple Redis failover cluster, use the following commands ([3] has more details):

```shell
kubectl create -f https://raw.githubusercontent.com/spotahome/redis-operator/master/example/operator/all-redis-operator-resources.yaml
kubectl create -f https://raw.githubusercontent.com/spotahome/redis-operator/master/example/redisfailover/basic.yaml
```

The above commands should create a list of pods in the default namespace:

```shell
redisoperator-57975fb5c-tkcfs        1/1     Running   0          20m
rfr-redisfailover-0                  1/1     Running   0          11m
rfr-redisfailover-1                  0/1     Pending   0          11m
rfr-redisfailover-2                  0/1     Pending   0          11m
rfs-redisfailover-5d49fd878f-89lbj   1/1     Running   0          11m
rfs-redisfailover-5d49fd878f-mdr9x   1/1     Running   0          11m
rfs-redisfailover-5d49fd878f-sxb2m   1/1     Running   0          11m
```

You should see records like these in the `redis_events.beta` (TODO(yzhao): Change to `redis_events`)
table, which essentially is a message exchanged between the failover processes of the Redis cluster:

```shell
{
time_: 1613604049816,
remote_addr: 10.245.35.240,
remote_port: 46810,
cmd: PUBLISH,
cmd_args: {
channel: __sentinel__:hello,
message: 10.245.35.240,26379,3f48dc18038d92582815433648b5eed5ddd4ae22,0,mymaster,10.245.35.243,6379,0
 },
resp: 3,
latency_ns: 186550,
pod: default/rfr-redisfailover-0
 }
```

[1] https://redis.io/topics/cluster-tutorial
[2] https://redis.io/topics/cluster-tutorial#redis-cluster-data-sharding
[3] https://github.com/spotahome/redis-operator
