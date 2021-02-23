package kvstore

//go:generate mockgen -source=cache.go  -destination=mock/kvstore_mock.gen.go KeyValueStore
