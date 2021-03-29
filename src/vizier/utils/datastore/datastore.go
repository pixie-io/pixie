package datastore

import "time"

// Getter is a datastore that implements a simple way to get values.
type Getter interface {
	Get(key string) ([]byte, error)
}

// MultiGetter is a datastore that implements methods that get multiple keys at once.
type MultiGetter interface {
	Getter
	GetWithRange(from string, to string) ([]string, [][]byte, error)
	GetWithPrefix(prefix string) ([]string, [][]byte, error)
}

// Setter is a datastore that implements a simple way to set values.
type Setter interface {
	Set(key string, value string) error
}

// TTLSetter is a datastore that implements a setter with a TTL.
// The set key and value should be purged form the datastore once the TTL expires.
type TTLSetter interface {
	Setter
	SetWithTTL(key string, value string, ttl time.Duration) error
}

// Deleter is a datastore that implements a simple way to delete values.
type Deleter interface {
	Delete(key string) error
}

// MultiDeleter is a datastore that implements methods that delete multiple keys at once.
type MultiDeleter interface {
	Deleter
	DeleteAll(keys []string) error
	DeleteWithPrefix(prefix string) error
}

// Closer is a datastore that can be closed commit changes and cleanup any pending resources.
type Closer interface {
	Close() error
}

// MultiGetterSetterDeleterCloser combines MultiGetter, TTLSetter, MultiDeleter, and Closer.
type MultiGetterSetterDeleterCloser interface {
	MultiGetter
	TTLSetter
	MultiDeleter
	Closer
}
