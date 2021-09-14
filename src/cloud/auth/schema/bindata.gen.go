// Code generated for package schema by go-bindata DO NOT EDIT. (@generated)
// sources:
// 000001_add_api_keys.down.sql
// 000001_add_api_keys.up.sql
// 000002_create_pgcrypto_extension.down.sql
// 000002_create_pgcrypto_extension.up.sql
// 000003_unsalt_api_key.down.sql
// 000003_unsalt_api_key.up.sql
// 000004_px_api_prefix.down.sql
// 000004_px_api_prefix.up.sql
// 000005_px_apikey_crypt.down.sql
// 000005_px_apikey_crypt.up.sql
// 000006_px_apikey_del_unsalted.down.sql
// 000006_px_apikey_del_unsalted.up.sql
package schema

import (
	"bytes"
	"compress/gzip"
	"fmt"
	"io"
	"io/ioutil"
	"os"
	"path/filepath"
	"strings"
	"time"
)

func bindataRead(data []byte, name string) ([]byte, error) {
	gz, err := gzip.NewReader(bytes.NewBuffer(data))
	if err != nil {
		return nil, fmt.Errorf("Read %q: %v", name, err)
	}

	var buf bytes.Buffer
	_, err = io.Copy(&buf, gz)
	clErr := gz.Close()

	if err != nil {
		return nil, fmt.Errorf("Read %q: %v", name, err)
	}
	if clErr != nil {
		return nil, err
	}

	return buf.Bytes(), nil
}

type asset struct {
	bytes []byte
	info  os.FileInfo
}

type bindataFileInfo struct {
	name    string
	size    int64
	mode    os.FileMode
	modTime time.Time
}

// Name return file name
func (fi bindataFileInfo) Name() string {
	return fi.name
}

// Size return file size
func (fi bindataFileInfo) Size() int64 {
	return fi.size
}

// Mode return file mode
func (fi bindataFileInfo) Mode() os.FileMode {
	return fi.mode
}

// Mode return file modify time
func (fi bindataFileInfo) ModTime() time.Time {
	return fi.modTime
}

// IsDir return file whether a directory
func (fi bindataFileInfo) IsDir() bool {
	return fi.mode&os.ModeDir != 0
}

// Sys return file is sys mode
func (fi bindataFileInfo) Sys() interface{} {
	return nil
}

var __000001_add_api_keysDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\x08\x71\x74\xf2\x71\x55\xf0\x74\x53\x70\x8d\xf0\x0c\x0e\x09\x56\x48\x2c\xc8\x8c\xcf\x4e\xad\x2c\xb6\xe6\x02\x04\x00\x00\xff\xff\xe7\x36\xb9\xd1\x1f\x00\x00\x00")

func _000001_add_api_keysDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000001_add_api_keysDownSql,
		"000001_add_api_keys.down.sql",
	)
}

func _000001_add_api_keysDownSql() (*asset, error) {
	bytes, err := _000001_add_api_keysDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000001_add_api_keys.down.sql", size: 31, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000001_add_api_keysUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x6c\x91\x41\x8f\x9b\x30\x10\x85\xef\xfc\x8a\xa7\x3d\x11\xa9\x59\xa5\x52\x6f\x7b\xa2\x8b\x57\xb2\x9a\x10\x9a\x18\x75\xf7\x84\x1c\x98\x05\x2b\xc4\x46\xb6\x49\x9a\x7f\x5f\xd9\x61\xa3\xb6\xda\x1b\xcc\xcc\x7b\xef\xf3\xcc\xf3\x8e\x65\x82\x81\xbd\x0a\x56\xec\xf9\xb6\x00\x7f\x41\xb1\x15\x60\xaf\x7c\x2f\xf6\x78\x98\x26\xd5\x2e\x8d\x73\xe3\xc3\x53\x92\x2c\x97\x10\xbd\x72\xf0\xf2\x30\x10\x1a\xa3\xbd\x54\xda\xe1\x48\x57\x07\xdf\x4b\x8f\x46\x6a\x1c\x08\x93\xa3\x16\xde\x40\x36\x0d\xb9\xd0\x22\x94\xea\xb7\x22\x64\x25\x7f\x4c\xe6\x48\x91\x7d\x5f\x33\xc8\x51\xd5\x51\x9f\x26\x40\xf4\x27\xf0\x3c\x88\x27\x47\x78\x37\x16\x3e\x24\x1e\xe9\xfa\x98\x00\xaa\x45\x55\xf1\x1c\x55\xc1\x7f\x56\x0c\x39\x7b\xc9\xaa\xb5\x40\x80\xac\x3b\xd2\x64\xa5\xa7\xfa\xfc\x2d\x5d\x7c\xb9\xb9\x19\xdb\xd5\xaa\x85\xba\x31\xf0\x1c\x07\x1a\x8c\xee\x94\xee\x42\x42\xa8\x19\xdb\x41\x69\x5c\x7a\xd5\xf4\xb1\x90\x95\x3c\xa4\x05\xcd\x59\x39\x75\x18\x28\x04\xcf\x46\x31\x3c\xac\xa7\xa8\xd6\xeb\x39\x63\x72\x64\x43\x2f\x2e\x60\x4e\x32\x17\x4d\x16\xe6\xfd\x1f\xf8\x8f\xc1\xcf\x4c\x84\x3a\x91\xf3\xf2\x34\xe2\xd2\x93\xbe\xcb\x70\x91\x0e\x8d\x25\xe9\xa9\x0d\x16\xf3\x67\x2d\x3d\x04\xdf\xb0\xbd\xc8\x36\xe5\x7d\x0b\xc5\xf6\xd7\xfd\xe1\x39\xb9\xc6\xaa\xd1\x2b\xa3\x6f\x18\x14\x29\xf0\x7c\x3b\x10\x9d\x46\x1f\x99\xda\xbf\xe6\xce\xd2\x36\xbd\xb4\xe9\xd7\xd5\x6a\xf5\xe1\x13\xce\x21\x1b\x3f\xc9\x21\xe2\x38\x39\xcc\x24\xe1\xef\x3f\x41\x82\xf9\x2e\xe9\x91\xae\xd1\xa0\xdc\xf1\x4d\xb6\x7b\xc3\x0f\xf6\x96\xaa\x76\x91\x2c\x9e\x92\x3f\x01\x00\x00\xff\xff\x00\x6b\x9e\x06\x70\x02\x00\x00")

func _000001_add_api_keysUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000001_add_api_keysUpSql,
		"000001_add_api_keys.up.sql",
	)
}

func _000001_add_api_keysUpSql() (*asset, error) {
	bytes, err := _000001_add_api_keysUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000001_add_api_keys.up.sql", size: 624, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000002_create_pgcrypto_extensionDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\x70\x8d\x08\x71\xf5\x0b\xf6\xf4\xf7\x53\xf0\x74\x53\x70\x8d\xf0\x0c\x0e\x09\x56\x50\x2a\x48\x4f\x2e\xaa\x2c\x28\xc9\x57\xb2\xe6\x02\x04\x00\x00\xff\xff\x57\x85\x06\xc2\x25\x00\x00\x00")

func _000002_create_pgcrypto_extensionDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000002_create_pgcrypto_extensionDownSql,
		"000002_create_pgcrypto_extension.down.sql",
	)
}

func _000002_create_pgcrypto_extensionDownSql() (*asset, error) {
	bytes, err := _000002_create_pgcrypto_extensionDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000002_create_pgcrypto_extension.down.sql", size: 37, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000002_create_pgcrypto_extensionUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x0e\x72\x75\x0c\x71\x55\x70\x8d\x08\x71\xf5\x0b\xf6\xf4\xf7\x53\xf0\x74\x53\xf0\xf3\x0f\x51\x70\x8d\xf0\x0c\x0e\x09\x56\x50\x2a\x48\x4f\x2e\xaa\x2c\x28\xc9\x57\xb2\xe6\x02\x04\x00\x00\xff\xff\x29\x5f\xd6\xbb\x2b\x00\x00\x00")

func _000002_create_pgcrypto_extensionUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000002_create_pgcrypto_extensionUpSql,
		"000002_create_pgcrypto_extension.up.sql",
	)
}

func _000002_create_pgcrypto_extensionUpSql() (*asset, error) {
	bytes, err := _000002_create_pgcrypto_extensionUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000002_create_pgcrypto_extension.up.sql", size: 43, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000003_unsalt_api_keyDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\xf0\xf4\x73\x71\x8d\x50\xc8\x4c\xa9\x88\x4f\x2c\xc8\x8c\xcf\x4e\xad\x2c\x8e\x2f\xcd\x2b\x4e\xcc\x29\x49\x4d\xb1\xe6\xe2\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x80\xc9\x73\x81\xf5\x39\xfb\xfb\x84\xfa\xfa\x29\xc0\xd4\x82\x64\xac\xb9\x00\x01\x00\x00\xff\xff\x71\xf1\x7b\x99\x52\x00\x00\x00")

func _000003_unsalt_api_keyDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000003_unsalt_api_keyDownSql,
		"000003_unsalt_api_key.down.sql",
	)
}

func _000003_unsalt_api_keyDownSql() (*asset, error) {
	bytes, err := _000003_unsalt_api_keyDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000003_unsalt_api_key.down.sql", size: 82, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000003_unsalt_api_keyUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x48\x2c\xc8\x8c\xcf\x4e\xad\x2c\xe6\x72\x74\x71\x51\x70\xf6\xf7\x09\xf5\xf5\x53\x28\xcd\x2b\x4e\xcc\x29\x49\x4d\x01\x49\x28\x94\x25\x16\x25\x67\x24\x16\x69\x18\x1a\x18\x18\x68\x5a\x73\x71\x39\x07\xb9\x3a\x86\xb8\x2a\x78\xfa\xb9\xb8\x46\x28\x64\xa6\x54\xc4\xc3\x4c\x88\x87\x69\xe3\xf2\xf7\x83\x1b\xab\x81\x6c\x96\xa6\x35\x17\x20\x00\x00\xff\xff\x1f\x84\x2e\x75\x7b\x00\x00\x00")

func _000003_unsalt_api_keyUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000003_unsalt_api_keyUpSql,
		"000003_unsalt_api_key.up.sql",
	)
}

func _000003_unsalt_api_keyUpSql() (*asset, error) {
	bytes, err := _000003_unsalt_api_keyUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000003_unsalt_api_key.up.sql", size: 123, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000004_px_api_prefixDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x0a\x0d\x70\x71\x0c\x71\x55\x48\x2c\xc8\x8c\xcf\x4e\xad\x2c\xe6\x52\x50\x50\x08\x76\x0d\x51\x28\xcd\x2b\x4e\xcc\x29\x49\x4d\x01\x09\x2a\xd8\x2a\x04\xb9\x06\xf8\x38\x3a\xbb\x6a\x20\x0b\xeb\x28\xa8\x17\x54\xe8\x26\x16\x64\xea\xaa\x6b\x72\x29\x84\x7b\xb8\x06\xb9\xa2\x6a\xf3\xf1\xf4\x76\x85\xab\x51\x55\xb7\xe6\x02\x04\x00\x00\xff\xff\xd0\xf2\xf3\x90\x6c\x00\x00\x00")

func _000004_px_api_prefixDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000004_px_api_prefixDownSql,
		"000004_px_api_prefix.down.sql",
	)
}

func _000004_px_api_prefixDownSql() (*asset, error) {
	bytes, err := _000004_px_api_prefixDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000004_px_api_prefix.down.sql", size: 108, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000004_px_api_prefixUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x0a\x0d\x70\x71\x0c\x71\x55\x48\x2c\xc8\x8c\xcf\x4e\xad\x2c\xe6\x52\x50\x50\x08\x76\x0d\x51\x28\xcd\x2b\x4e\xcc\x29\x49\x4d\x01\x09\x2a\xd8\x2a\xa8\x17\x54\xe8\x26\x16\x64\xea\xaa\x2b\xd4\xd4\xa0\xc8\x71\x29\x84\x7b\xb8\x06\xb9\xa2\xaa\xf7\xf3\x0f\x51\xf0\xf1\xf4\x76\x85\x6b\x53\x55\xb7\xe6\x02\x04\x00\x00\xff\xff\xee\xad\xa5\x6c\x69\x00\x00\x00")

func _000004_px_api_prefixUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000004_px_api_prefixUpSql,
		"000004_px_api_prefix.up.sql",
	)
}

func _000004_px_api_prefixUpSql() (*asset, error) {
	bytes, err := _000004_px_api_prefixUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000004_px_api_prefix.up.sql", size: 105, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000005_px_apikey_cryptDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\xf0\xf4\x73\x71\x8d\x50\xc8\x4c\xa9\x88\x4f\x2c\xc8\x8c\xcf\x4e\xad\x2c\x8e\xcf\x48\x2c\xce\x48\x4d\x01\xb1\xad\xb9\xb8\x1c\x7d\x42\x5c\x83\x14\x42\x1c\x9d\x7c\x5c\x15\x60\x2a\xb8\x14\x14\xc0\x7a\x9d\xfd\x7d\x42\x7d\xfd\x14\x48\x55\x9f\x9a\x97\x5c\x54\x59\x50\x02\xd3\x02\x08\x00\x00\xff\xff\xa9\x74\x39\x2c\x87\x00\x00\x00")

func _000005_px_apikey_cryptDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000005_px_apikey_cryptDownSql,
		"000005_px_apikey_crypt.down.sql",
	)
}

func _000005_px_apikey_cryptDownSql() (*asset, error) {
	bytes, err := _000005_px_apikey_cryptDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000005_px_apikey_crypt.down.sql", size: 135, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000005_px_apikey_cryptUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x84\x8e\xb1\xaa\x83\x40\x10\x45\xfb\xfd\x8a\x5b\xbe\x57\xf8\x7e\xc0\x6a\x9f\x2e\x24\x60\x14\xc4\x40\x3a\x99\xb8\x13\x5c\x14\x57\x9c\x35\x89\x7f\x1f\x24\x24\x92\x2a\xed\x9c\xb9\x87\xa3\xb3\xca\x94\xa8\xf4\x7f\x66\x40\xa3\xab\x3b\x5e\x44\x01\x3a\x4d\x91\x14\xd9\xf1\x90\x83\x87\x66\x5a\xc6\xc0\x76\x65\x38\x2f\x81\x29\x56\x2a\x8a\xb0\x23\x69\xd9\x62\xbd\x4a\xf0\x13\x0b\x08\x42\x7d\x60\x0b\x1a\x2c\xda\x0d\x87\x96\x02\x6e\x8c\x86\x06\xcc\xc2\xb8\xf8\x09\x24\xe2\x1b\x47\xc1\x5d\x19\xbd\xf7\xdd\x3c\xfe\xa9\xef\x31\x4f\xe9\x47\x49\x52\x1a\x5d\x19\xec\xf3\xd4\x9c\xe0\xec\xbd\x7e\x2d\xeb\xed\x59\x01\x45\xfe\x56\xfe\x6c\xe0\x37\x56\x8f\x00\x00\x00\xff\xff\x70\x3b\x7b\x9a\x02\x01\x00\x00")

func _000005_px_apikey_cryptUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000005_px_apikey_cryptUpSql,
		"000005_px_apikey_crypt.up.sql",
	)
}

func _000005_px_apikey_cryptUpSql() (*asset, error) {
	bytes, err := _000005_px_apikey_cryptUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000005_px_apikey_crypt.up.sql", size: 258, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000006_px_apikey_del_unsaltedDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x48\x2c\xc8\x8c\xcf\x4e\xad\x2c\xe6\x52\x50\x70\x74\x71\x51\x70\xf6\xf7\x09\xf5\xf5\x53\x28\xcd\x2b\x4e\xcc\x29\x49\x4d\x01\x49\x29\x94\x25\x16\x25\x67\x24\x16\x59\x73\x71\x11\xd6\x88\xa2\x1e\x10\x00\x00\xff\xff\x10\x20\xd9\x52\x68\x00\x00\x00")

func _000006_px_apikey_del_unsaltedDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000006_px_apikey_del_unsaltedDownSql,
		"000006_px_apikey_del_unsalted.down.sql",
	)
}

func _000006_px_apikey_del_unsaltedDownSql() (*asset, error) {
	bytes, err := _000006_px_apikey_del_unsaltedDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000006_px_apikey_del_unsalted.down.sql", size: 104, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000006_px_apikey_del_unsaltedUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x48\x2c\xc8\x8c\xcf\x4e\xad\x2c\xe6\x52\x50\x70\x09\xf2\x0f\x50\x70\xf6\xf7\x09\xf5\xf5\x53\x28\xcd\x2b\x4e\xcc\x29\x49\x4d\x01\xc9\x59\x73\x71\x11\xa1\x03\xac\x10\x10\x00\x00\xff\xff\xf4\x69\x58\xa4\x5a\x00\x00\x00")

func _000006_px_apikey_del_unsaltedUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000006_px_apikey_del_unsaltedUpSql,
		"000006_px_apikey_del_unsalted.up.sql",
	)
}

func _000006_px_apikey_del_unsaltedUpSql() (*asset, error) {
	bytes, err := _000006_px_apikey_del_unsaltedUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000006_px_apikey_del_unsalted.up.sql", size: 90, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

// Asset loads and returns the asset for the given name.
// It returns an error if the asset could not be found or
// could not be loaded.
func Asset(name string) ([]byte, error) {
	cannonicalName := strings.Replace(name, "\\", "/", -1)
	if f, ok := _bindata[cannonicalName]; ok {
		a, err := f()
		if err != nil {
			return nil, fmt.Errorf("Asset %s can't read by error: %v", name, err)
		}
		return a.bytes, nil
	}
	return nil, fmt.Errorf("Asset %s not found", name)
}

// MustAsset is like Asset but panics when Asset would return an error.
// It simplifies safe initialization of global variables.
func MustAsset(name string) []byte {
	a, err := Asset(name)
	if err != nil {
		panic("asset: Asset(" + name + "): " + err.Error())
	}

	return a
}

// AssetInfo loads and returns the asset info for the given name.
// It returns an error if the asset could not be found or
// could not be loaded.
func AssetInfo(name string) (os.FileInfo, error) {
	cannonicalName := strings.Replace(name, "\\", "/", -1)
	if f, ok := _bindata[cannonicalName]; ok {
		a, err := f()
		if err != nil {
			return nil, fmt.Errorf("AssetInfo %s can't read by error: %v", name, err)
		}
		return a.info, nil
	}
	return nil, fmt.Errorf("AssetInfo %s not found", name)
}

// AssetNames returns the names of the assets.
func AssetNames() []string {
	names := make([]string, 0, len(_bindata))
	for name := range _bindata {
		names = append(names, name)
	}
	return names
}

// _bindata is a table, holding each asset generator, mapped to its name.
var _bindata = map[string]func() (*asset, error){
	"000001_add_api_keys.down.sql":              _000001_add_api_keysDownSql,
	"000001_add_api_keys.up.sql":                _000001_add_api_keysUpSql,
	"000002_create_pgcrypto_extension.down.sql": _000002_create_pgcrypto_extensionDownSql,
	"000002_create_pgcrypto_extension.up.sql":   _000002_create_pgcrypto_extensionUpSql,
	"000003_unsalt_api_key.down.sql":            _000003_unsalt_api_keyDownSql,
	"000003_unsalt_api_key.up.sql":              _000003_unsalt_api_keyUpSql,
	"000004_px_api_prefix.down.sql":             _000004_px_api_prefixDownSql,
	"000004_px_api_prefix.up.sql":               _000004_px_api_prefixUpSql,
	"000005_px_apikey_crypt.down.sql":           _000005_px_apikey_cryptDownSql,
	"000005_px_apikey_crypt.up.sql":             _000005_px_apikey_cryptUpSql,
	"000006_px_apikey_del_unsalted.down.sql":    _000006_px_apikey_del_unsaltedDownSql,
	"000006_px_apikey_del_unsalted.up.sql":      _000006_px_apikey_del_unsaltedUpSql,
}

// AssetDir returns the file names below a certain
// directory embedded in the file by go-bindata.
// For example if you run go-bindata on data/... and data contains the
// following hierarchy:
//     data/
//       foo.txt
//       img/
//         a.png
//         b.png
// then AssetDir("data") would return []string{"foo.txt", "img"}
// AssetDir("data/img") would return []string{"a.png", "b.png"}
// AssetDir("foo.txt") and AssetDir("notexist") would return an error
// AssetDir("") will return []string{"data"}.
func AssetDir(name string) ([]string, error) {
	node := _bintree
	if len(name) != 0 {
		cannonicalName := strings.Replace(name, "\\", "/", -1)
		pathList := strings.Split(cannonicalName, "/")
		for _, p := range pathList {
			node = node.Children[p]
			if node == nil {
				return nil, fmt.Errorf("Asset %s not found", name)
			}
		}
	}
	if node.Func != nil {
		return nil, fmt.Errorf("Asset %s not found", name)
	}
	rv := make([]string, 0, len(node.Children))
	for childName := range node.Children {
		rv = append(rv, childName)
	}
	return rv, nil
}

type bintree struct {
	Func     func() (*asset, error)
	Children map[string]*bintree
}

var _bintree = &bintree{nil, map[string]*bintree{
	"000001_add_api_keys.down.sql":              &bintree{_000001_add_api_keysDownSql, map[string]*bintree{}},
	"000001_add_api_keys.up.sql":                &bintree{_000001_add_api_keysUpSql, map[string]*bintree{}},
	"000002_create_pgcrypto_extension.down.sql": &bintree{_000002_create_pgcrypto_extensionDownSql, map[string]*bintree{}},
	"000002_create_pgcrypto_extension.up.sql":   &bintree{_000002_create_pgcrypto_extensionUpSql, map[string]*bintree{}},
	"000003_unsalt_api_key.down.sql":            &bintree{_000003_unsalt_api_keyDownSql, map[string]*bintree{}},
	"000003_unsalt_api_key.up.sql":              &bintree{_000003_unsalt_api_keyUpSql, map[string]*bintree{}},
	"000004_px_api_prefix.down.sql":             &bintree{_000004_px_api_prefixDownSql, map[string]*bintree{}},
	"000004_px_api_prefix.up.sql":               &bintree{_000004_px_api_prefixUpSql, map[string]*bintree{}},
	"000005_px_apikey_crypt.down.sql":           &bintree{_000005_px_apikey_cryptDownSql, map[string]*bintree{}},
	"000005_px_apikey_crypt.up.sql":             &bintree{_000005_px_apikey_cryptUpSql, map[string]*bintree{}},
	"000006_px_apikey_del_unsalted.down.sql":    &bintree{_000006_px_apikey_del_unsaltedDownSql, map[string]*bintree{}},
	"000006_px_apikey_del_unsalted.up.sql":      &bintree{_000006_px_apikey_del_unsaltedUpSql, map[string]*bintree{}},
}}

// RestoreAsset restores an asset under the given directory
func RestoreAsset(dir, name string) error {
	data, err := Asset(name)
	if err != nil {
		return err
	}
	info, err := AssetInfo(name)
	if err != nil {
		return err
	}
	err = os.MkdirAll(_filePath(dir, filepath.Dir(name)), os.FileMode(0755))
	if err != nil {
		return err
	}
	err = ioutil.WriteFile(_filePath(dir, name), data, info.Mode())
	if err != nil {
		return err
	}
	err = os.Chtimes(_filePath(dir, name), info.ModTime(), info.ModTime())
	if err != nil {
		return err
	}
	return nil
}

// RestoreAssets restores an asset under the given directory recursively
func RestoreAssets(dir, name string) error {
	children, err := AssetDir(name)
	// File
	if err != nil {
		return RestoreAsset(dir, name)
	}
	// Dir
	for _, child := range children {
		err = RestoreAssets(dir, filepath.Join(name, child))
		if err != nil {
			return err
		}
	}
	return nil
}

func _filePath(dir, name string) string {
	cannonicalName := strings.Replace(name, "\\", "/", -1)
	return filepath.Join(append([]string{dir}, strings.Split(cannonicalName, "/")...)...)
}
