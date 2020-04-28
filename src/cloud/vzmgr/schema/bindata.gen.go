// Code generated for package schema by go-bindata DO NOT EDIT. (@generated)
// sources:
// 000001_create_cluster_tables.down.sql
// 000001_create_cluster_tables.up.sql
// 000002_create_pgcrypto_extension.down.sql
// 000002_create_pgcrypto_extension.up.sql
// 000003_create_index_table.down.sql
// 000003_create_index_table.up.sql
// 000004_create_shard_index.down.sql
// 000004_create_shard_index.up.sql
// 000005_add_passthrough_to_cluster_table.down.sql
// 000005_add_passthrough_to_cluster_table.up.sql
// 000006_add_project_id_to_cluster_table.down.sql
// 000006_add_project_id_to_cluster_table.up.sql
// 000007_update_cluster_status_enum.down.sql
// 000007_update_cluster_status_enum.up.sql
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

var __000001_create_cluster_tablesDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\x08\x71\x74\xf2\x71\x55\xf0\x74\x53\x70\x8d\xf0\x0c\x0e\x09\x56\x28\xcb\xac\xca\x4c\x2d\x8a\x4f\xce\x29\x2d\x2e\x49\x2d\x8a\xcf\xcc\x4b\xcb\xb7\xe6\x82\xa8\x8c\x0c\xc0\xa2\xb0\xb8\x24\xb1\xa4\xb4\x18\xa6\x04\xaf\x61\xd6\x5c\x80\x00\x00\x00\xff\xff\x12\xa8\x9d\x4b\x72\x00\x00\x00")

func _000001_create_cluster_tablesDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000001_create_cluster_tablesDownSql,
		"000001_create_cluster_tables.down.sql",
	)
}

func _000001_create_cluster_tablesDownSql() (*asset, error) {
	bytes, err := _000001_create_cluster_tablesDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000001_create_cluster_tables.down.sql", size: 114, mode: os.FileMode(436), modTime: time.Unix(1566845212, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000001_create_cluster_tablesUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x8c\x92\x51\x6f\xda\x30\x14\x85\xdf\xfd\x2b\x8e\xfa\x02\x48\x50\x31\x69\x6f\x7d\xca\xc0\xac\x51\xc1\x61\x89\xa3\x96\xa7\xc8\x4d\x2e\xc4\x1b\x24\x95\x6d\x40\xdb\xaf\x9f\x9c\x06\x16\x60\x9d\xf6\xe6\xd8\xf7\x7e\xf7\xe4\x9c\x3b\x89\x79\x20\x39\xf8\x8b\xe4\x22\x09\x23\x81\x70\x06\x11\x49\xf0\x97\x30\x91\x09\xee\xf6\x7b\x5d\x8c\x6a\x6b\xdf\xee\x1e\x18\x1b\x8d\x20\x4b\x6d\xe1\xd4\xeb\x96\x90\xd7\x95\x53\xba\xb2\xc8\xb7\x7b\xeb\xc8\xc0\xd0\x46\x5b\x67\x94\xd3\x75\x05\x5d\xad\x6b\xb3\x6b\xce\x43\xa8\x3c\x27\x6b\x9b\x0e\x53\x6f\x87\x20\x97\xdf\xb3\x76\xb4\x0c\xbe\xcc\x39\x0e\xfa\x97\x26\x93\x9d\x50\x7d\x06\x34\xd3\x08\xe1\x14\xae\xc6\xde\x12\xd6\xb5\x81\xf3\xf3\xdb\xaa\x7b\x06\xe8\x02\x69\x1a\x4e\x91\x8a\xf0\x5b\xca\x31\xe5\xb3\x20\x9d\x4b\x78\xd9\xd9\x86\x2a\x32\xca\x51\x76\xf8\xdc\x1f\x0c\xdf\x89\xb5\xd9\x64\xba\x80\xff\x89\x92\x50\x1f\x2b\x32\xa8\xd7\x37\xd8\xb6\xac\x41\x7b\x3b\x44\x3a\x9f\xb7\x04\xa9\x77\x64\x9d\xda\xbd\xe1\x58\x52\x75\xd1\x89\xa3\xb2\xc8\x0d\x29\x47\x85\xa7\xb4\xc7\x4c\x39\xc8\x70\xc1\x13\x19\x2c\x96\x67\x89\x22\x7a\x3e\xab\xea\x30\x09\xa5\x3a\x10\xd6\xda\x58\x07\x4b\x57\x13\x3c\xb5\x79\xca\xfc\xd3\x05\x78\xc8\x18\xb0\x8c\xc3\x45\x10\xaf\xf0\xc4\x57\x7d\x5d\x0c\xd8\xe0\x81\xb1\xb3\xd1\xab\xe5\xd9\x67\xeb\x94\xdb\x5b\x04\x09\xb8\x48\x17\xe8\xf7\x52\xf1\x24\xa2\x67\xd1\x1b\xa2\xf7\xc8\x83\xb9\x7c\x5c\xf9\x63\x2a\x3a\x1f\xd3\x30\x99\x44\x42\xf0\x89\xe4\xd3\x9e\x07\xff\x23\xc0\xcc\xc7\xff\xff\x29\x5e\x37\x7f\xe0\x7c\x49\x70\x7a\x47\xef\x81\x11\xb6\xca\x3a\x94\xa4\x8c\x7b\x25\xe5\x3c\xc7\xdf\x64\xe7\x9b\xae\x37\x7f\x84\x2c\xa1\x8a\xc2\xf8\x75\x6c\x31\x1d\x1d\xa7\x97\x83\x32\x79\xa9\x4c\xff\xd3\x78\x3c\x1e\x74\x9a\xad\xde\x54\xba\xda\xe0\x07\xfd\xfc\x4b\xf7\xf7\xa3\xcb\xda\x8a\xcc\x57\x5c\x51\x18\xd0\xda\x7e\x19\xc2\x69\x21\x2e\x0d\xbe\x89\xf3\xc6\xa3\x46\xd8\x2c\x8a\x79\xf8\x55\x7c\x50\x81\x98\xcf\x78\xcc\xc5\x84\x27\x57\x1e\x9f\xb6\xe3\x77\x00\x00\x00\xff\xff\xd2\xb0\x71\x05\x00\x04\x00\x00")

func _000001_create_cluster_tablesUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000001_create_cluster_tablesUpSql,
		"000001_create_cluster_tables.up.sql",
	)
}

func _000001_create_cluster_tablesUpSql() (*asset, error) {
	bytes, err := _000001_create_cluster_tablesUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000001_create_cluster_tables.up.sql", size: 1024, mode: os.FileMode(436), modTime: time.Unix(1570577344, 0)}
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

	info := bindataFileInfo{name: "000002_create_pgcrypto_extension.down.sql", size: 37, mode: os.FileMode(436), modTime: time.Unix(1570640344, 0)}
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

	info := bindataFileInfo{name: "000002_create_pgcrypto_extension.up.sql", size: 43, mode: os.FileMode(436), modTime: time.Unix(1570640344, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000003_create_index_tableDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\x08\x71\x74\xf2\x71\x55\xf0\x74\x53\x70\x8d\xf0\x0c\x0e\x09\x56\x28\xcb\xac\xca\x4c\x2d\x8a\xcf\xcc\x4b\x49\xad\x88\x2f\x2e\x49\x2c\x49\xb5\xe6\x02\x04\x00\x00\xff\xff\xa8\x9d\x63\xb4\x29\x00\x00\x00")

func _000003_create_index_tableDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000003_create_index_tableDownSql,
		"000003_create_index_table.down.sql",
	)
}

func _000003_create_index_tableDownSql() (*asset, error) {
	bytes, err := _000003_create_index_tableDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000003_create_index_table.down.sql", size: 41, mode: os.FileMode(436), modTime: time.Unix(1588021619, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000003_create_index_tableUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x6c\x8f\xc1\x6a\xeb\x30\x10\x45\xf7\xfa\x8a\xbb\x8b\x0d\x76\x78\x8b\xd7\x55\x56\xae\xad\x80\xa8\xe3\xb4\x8e\x5c\xc8\x4a\xa8\xf6\x24\x16\x04\x19\x24\xd9\x84\x7e\x7d\xb1\x9b\xd0\x42\xba\x1b\x66\x38\x77\xee\x49\x53\xc8\xde\x78\x04\xfd\x71\x21\xb4\x83\x0d\xda\x58\x0f\x63\x3b\xba\xc2\x07\x1d\x08\xa7\xc1\x81\x74\xdb\x63\x32\x9f\x86\xdc\x9a\xe5\x35\xcf\x24\x87\xcc\x9e\x4b\x7e\x5b\xaa\x05\x50\xdf\x40\xc4\x80\x25\x97\x20\x0a\x0c\x27\x84\x9e\xd0\x5e\x46\x1f\x66\x1a\xf7\x51\x99\x0e\x4d\x23\x0a\x34\x95\x78\x6b\x38\x0a\xbe\xcd\x9a\x52\x62\x1c\x4d\xa7\xce\x64\xc9\xe9\x40\x6a\xfa\x1f\xc5\x09\x03\x1c\xf9\x61\x74\x2d\xa9\x89\x9c\x37\x83\xc5\xa4\x5d\xdb\x6b\x17\x3d\xfd\x8b\x13\xc6\x80\xd7\x5a\xec\xb2\xfa\x88\x17\x7e\x8c\x7e\x3e\xc4\x2c\xde\x30\x96\xa6\x29\x72\x47\x73\xb9\x07\xb3\xab\xf1\xc1\xd8\x33\xde\x17\x11\xbf\x66\xa2\x3a\xf0\x5a\x42\x54\x72\xff\x87\xdd\xaf\xec\xe4\xa1\x54\x8c\x03\x2f\x79\x2e\x31\x1f\x57\x2b\x6c\xeb\xfd\xee\x9e\x71\xe3\x36\xec\x2b\x00\x00\xff\xff\x0d\xe1\xbc\xbe\x73\x01\x00\x00")

func _000003_create_index_tableUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000003_create_index_tableUpSql,
		"000003_create_index_table.up.sql",
	)
}

func _000003_create_index_tableUpSql() (*asset, error) {
	bytes, err := _000003_create_index_tableUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000003_create_index_table.up.sql", size: 371, mode: os.FileMode(436), modTime: time.Unix(1588021619, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000004_create_shard_indexDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\xf0\xf4\x73\x71\x8d\x50\x28\xcb\xac\xca\x4c\x2d\x8a\x4f\xce\x29\x2d\x2e\x49\x2d\x8a\x2f\xce\x48\x2c\x4a\x89\xcf\xcc\x4b\x49\xad\xb0\xe6\x02\x04\x00\x00\xff\xff\x7b\x8a\xfd\xde\x27\x00\x00\x00")

func _000004_create_shard_indexDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000004_create_shard_indexDownSql,
		"000004_create_shard_index.down.sql",
	)
}

func _000004_create_shard_indexDownSql() (*asset, error) {
	bytes, err := _000004_create_shard_indexDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000004_create_shard_index.down.sql", size: 39, mode: os.FileMode(436), modTime: time.Unix(1588021619, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000004_create_shard_indexUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x5c\xcd\xc1\xca\x82\x50\x10\xc5\xf1\xbd\x4f\x71\x96\x0a\x9f\xdf\x26\x82\xb0\x55\xa4\x0b\x37\x06\x95\xd0\x4e\xae\x3a\xe5\x80\xdc\x0b\x33\x63\x4a\x4f\x1f\xe5\xae\xed\x39\xfc\xf9\xa5\x29\x2e\x83\x93\x5e\xe1\x84\x60\x03\x61\x0c\x33\x09\x76\x69\xcb\xa6\x08\xf7\xef\x56\xd7\x65\x0e\x35\x61\xff\x80\x06\xcc\x84\x4e\xc8\x19\xc1\x79\xb0\xef\x69\x41\xf0\xb0\xc1\xd9\x7f\x74\x3c\x17\x87\x6b\x81\xb2\xca\x8b\x1b\x9e\xfc\x62\x92\xa6\x1b\x27\x35\x92\x46\x3f\x52\xb3\x06\xa7\xea\xe7\x45\xac\x53\xbb\x1a\x31\xf7\x59\x66\xb4\xd8\x1f\x36\xdb\x24\xd9\x47\xef\x00\x00\x00\xff\xff\xab\x7b\x94\xcb\xa6\x00\x00\x00")

func _000004_create_shard_indexUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000004_create_shard_indexUpSql,
		"000004_create_shard_index.up.sql",
	)
}

func _000004_create_shard_indexUpSql() (*asset, error) {
	bytes, err := _000004_create_shard_indexUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000004_create_shard_index.up.sql", size: 166, mode: os.FileMode(436), modTime: time.Unix(1588021619, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000005_add_passthrough_to_cluster_tableDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\xcb\xac\xca\x4c\x2d\x8a\x4f\xce\x29\x2d\x2e\x49\x2d\x8a\xcf\xcc\x4b\xcb\x57\x70\x09\xf2\x0f\x50\x70\xf6\xf7\x09\xf5\xf5\x53\x28\x48\x2c\x2e\x2e\xc9\x28\xca\x2f\x4d\xcf\x88\x4f\xcd\x4b\x4c\xca\x49\x4d\xb1\xe6\x02\x04\x00\x00\xff\xff\x46\x14\xdd\x93\x41\x00\x00\x00")

func _000005_add_passthrough_to_cluster_tableDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000005_add_passthrough_to_cluster_tableDownSql,
		"000005_add_passthrough_to_cluster_table.down.sql",
	)
}

func _000005_add_passthrough_to_cluster_tableDownSql() (*asset, error) {
	bytes, err := _000005_add_passthrough_to_cluster_tableDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000005_add_passthrough_to_cluster_table.down.sql", size: 65, mode: os.FileMode(436), modTime: time.Unix(1588021619, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000005_add_passthrough_to_cluster_tableUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x04\xc0\xb1\x0e\xc2\x20\x10\x06\xe0\xdd\xa7\xf8\xdf\xc3\xe9\x2a\xe7\x74\x96\xc4\xd0\x99\x54\x3d\x85\x84\x80\x39\xc0\xc1\xa7\xef\x47\x12\xf8\x8e\x40\x8b\x30\x7e\xf9\x9f\xd5\xe2\xb3\xcc\x3e\xd4\x62\xae\xef\x06\x72\x0e\x17\x2f\xdb\x6d\xc5\x77\xef\x7d\x24\x6b\xf3\x93\xa2\xd6\xfd\x51\xf4\x85\xc5\x7b\x61\x5a\xe1\xf8\x4a\x9b\x04\x0c\x9b\x7a\x3e\x1d\x01\x00\x00\xff\xff\x93\x5d\xdf\x73\x55\x00\x00\x00")

func _000005_add_passthrough_to_cluster_tableUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000005_add_passthrough_to_cluster_tableUpSql,
		"000005_add_passthrough_to_cluster_table.up.sql",
	)
}

func _000005_add_passthrough_to_cluster_tableUpSql() (*asset, error) {
	bytes, err := _000005_add_passthrough_to_cluster_tableUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000005_add_passthrough_to_cluster_table.up.sql", size: 85, mode: os.FileMode(436), modTime: time.Unix(1588021619, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000006_add_project_id_to_cluster_tableDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\xd2\xd5\x55\xc8\x2f\x4a\x8f\xcf\x4c\x51\x48\xcc\xc9\xcf\x4b\x55\xc8\x2c\x56\xc8\xcb\x2f\x57\x28\xc9\x48\x55\xc8\x2f\xcf\x4b\x2d\x52\xc8\x4f\x03\x73\x92\x73\x4a\x8b\x4b\x52\x8b\x14\x12\xd3\x13\x33\xf3\xf4\xb8\x1c\x7d\x42\x5c\x83\x14\x42\x1c\x9d\x7c\x5c\x15\xca\x32\xab\x32\x53\x8b\xe2\x61\x2a\x5c\x82\xfc\x03\x14\x9c\xfd\x7d\x42\x7d\xfd\x14\x0a\x8a\xf2\xb3\x52\x93\x4b\xe2\xf3\x12\x73\x53\xad\xb9\x00\x01\x00\x00\xff\xff\x0f\x27\x1c\xa0\x6c\x00\x00\x00")

func _000006_add_project_id_to_cluster_tableDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000006_add_project_id_to_cluster_tableDownSql,
		"000006_add_project_id_to_cluster_table.down.sql",
	)
}

func _000006_add_project_id_to_cluster_tableDownSql() (*asset, error) {
	bytes, err := _000006_add_project_id_to_cluster_tableDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000006_add_project_id_to_cluster_table.down.sql", size: 108, mode: os.FileMode(436), modTime: time.Unix(1588021619, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000006_add_project_id_to_cluster_tableUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x54\xcd\xb1\x0e\x82\x30\x14\x46\xe1\x9d\xa7\xf8\x37\x20\x01\xe3\xe2\xe4\x54\x05\xa7\x0a\x89\x29\x33\x69\xe0\x22\x35\xd8\x9a\xcb\x05\x12\x9f\xde\x41\x17\xf7\x73\xf2\xe5\x39\x92\xc0\xf7\xd6\xf5\x19\x5e\x1c\x1e\xd4\x49\xeb\xed\x93\x52\xb8\x19\x3e\x6c\x90\x91\x10\x36\x4f\x8c\x30\x40\x46\x37\xa3\x9b\x96\x59\x88\x33\xf8\x20\xf8\xbe\xbb\x48\x69\x53\xde\x60\xd4\x49\x97\x58\xdd\xdb\x11\xb7\xbf\x0e\xaa\x28\x70\xae\x75\x73\xad\xfe\x04\xac\x96\xbb\xd1\x72\x72\xd8\xa7\xa8\x6a\x83\xaa\xd1\x1a\x45\x79\x51\x8d\x36\x88\x7b\x1a\xec\x32\x49\x7c\x8c\x3e\x01\x00\x00\xff\xff\x33\x6c\xfc\x53\xa3\x00\x00\x00")

func _000006_add_project_id_to_cluster_tableUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000006_add_project_id_to_cluster_tableUpSql,
		"000006_add_project_id_to_cluster_table.up.sql",
	)
}

func _000006_add_project_id_to_cluster_tableUpSql() (*asset, error) {
	bytes, err := _000006_add_project_id_to_cluster_tableUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000006_add_project_id_to_cluster_table.up.sql", size: 163, mode: os.FileMode(436), modTime: time.Unix(1588021619, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000007_update_cluster_status_enumDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\x08\x89\x0c\x70\x55\x28\xcb\xac\xca\x4c\x2d\x8a\x2f\x2e\x49\x2c\x29\x2d\xb6\xe6\x72\x0e\x72\x75\x0c\x71\xc5\x22\xa5\xe0\x18\xac\xe0\xea\x17\xea\xab\xa0\xa1\x1e\xea\xe7\xed\xe7\x1f\xee\xa7\xae\xa3\xa0\xee\xe1\xea\xe8\x13\xe2\x11\x09\x62\x86\xfa\x21\x71\x5c\x3c\x83\x9d\xfd\xfd\xfc\x5c\x9d\x43\x5c\x5d\xd4\x35\xad\xb9\x00\x01\x00\x00\xff\xff\xe7\x38\xdf\xda\x70\x00\x00\x00")

func _000007_update_cluster_status_enumDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000007_update_cluster_status_enumDownSql,
		"000007_update_cluster_status_enum.down.sql",
	)
}

func _000007_update_cluster_status_enumDownSql() (*asset, error) {
	bytes, err := _000007_update_cluster_status_enumDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000007_update_cluster_status_enum.down.sql", size: 112, mode: os.FileMode(436), modTime: time.Unix(1588042807, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000007_update_cluster_status_enumUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x89\x0c\x70\x55\x28\xcb\xac\xca\x4c\x2d\x8a\x2f\x2e\x49\x2c\x29\x2d\x56\x70\x74\x71\x51\x08\x73\xf4\x09\x75\x55\x50\x0f\x0d\x70\x71\x0c\xf1\xf4\x73\x57\x57\x70\x74\x03\xa9\x56\x77\xf1\x0c\x76\xf6\xf7\xf3\x73\x75\x0e\x71\x75\x51\xb7\x06\x04\x00\x00\xff\xff\xaa\x49\x3f\xcd\x43\x00\x00\x00")

func _000007_update_cluster_status_enumUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000007_update_cluster_status_enumUpSql,
		"000007_update_cluster_status_enum.up.sql",
	)
}

func _000007_update_cluster_status_enumUpSql() (*asset, error) {
	bytes, err := _000007_update_cluster_status_enumUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000007_update_cluster_status_enum.up.sql", size: 67, mode: os.FileMode(436), modTime: time.Unix(1588042838, 0)}
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
	"000001_create_cluster_tables.down.sql":            _000001_create_cluster_tablesDownSql,
	"000001_create_cluster_tables.up.sql":              _000001_create_cluster_tablesUpSql,
	"000002_create_pgcrypto_extension.down.sql":        _000002_create_pgcrypto_extensionDownSql,
	"000002_create_pgcrypto_extension.up.sql":          _000002_create_pgcrypto_extensionUpSql,
	"000003_create_index_table.down.sql":               _000003_create_index_tableDownSql,
	"000003_create_index_table.up.sql":                 _000003_create_index_tableUpSql,
	"000004_create_shard_index.down.sql":               _000004_create_shard_indexDownSql,
	"000004_create_shard_index.up.sql":                 _000004_create_shard_indexUpSql,
	"000005_add_passthrough_to_cluster_table.down.sql": _000005_add_passthrough_to_cluster_tableDownSql,
	"000005_add_passthrough_to_cluster_table.up.sql":   _000005_add_passthrough_to_cluster_tableUpSql,
	"000006_add_project_id_to_cluster_table.down.sql":  _000006_add_project_id_to_cluster_tableDownSql,
	"000006_add_project_id_to_cluster_table.up.sql":    _000006_add_project_id_to_cluster_tableUpSql,
	"000007_update_cluster_status_enum.down.sql":       _000007_update_cluster_status_enumDownSql,
	"000007_update_cluster_status_enum.up.sql":         _000007_update_cluster_status_enumUpSql,
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
	"000001_create_cluster_tables.down.sql":            &bintree{_000001_create_cluster_tablesDownSql, map[string]*bintree{}},
	"000001_create_cluster_tables.up.sql":              &bintree{_000001_create_cluster_tablesUpSql, map[string]*bintree{}},
	"000002_create_pgcrypto_extension.down.sql":        &bintree{_000002_create_pgcrypto_extensionDownSql, map[string]*bintree{}},
	"000002_create_pgcrypto_extension.up.sql":          &bintree{_000002_create_pgcrypto_extensionUpSql, map[string]*bintree{}},
	"000003_create_index_table.down.sql":               &bintree{_000003_create_index_tableDownSql, map[string]*bintree{}},
	"000003_create_index_table.up.sql":                 &bintree{_000003_create_index_tableUpSql, map[string]*bintree{}},
	"000004_create_shard_index.down.sql":               &bintree{_000004_create_shard_indexDownSql, map[string]*bintree{}},
	"000004_create_shard_index.up.sql":                 &bintree{_000004_create_shard_indexUpSql, map[string]*bintree{}},
	"000005_add_passthrough_to_cluster_table.down.sql": &bintree{_000005_add_passthrough_to_cluster_tableDownSql, map[string]*bintree{}},
	"000005_add_passthrough_to_cluster_table.up.sql":   &bintree{_000005_add_passthrough_to_cluster_tableUpSql, map[string]*bintree{}},
	"000006_add_project_id_to_cluster_table.down.sql":  &bintree{_000006_add_project_id_to_cluster_tableDownSql, map[string]*bintree{}},
	"000006_add_project_id_to_cluster_table.up.sql":    &bintree{_000006_add_project_id_to_cluster_tableUpSql, map[string]*bintree{}},
	"000007_update_cluster_status_enum.down.sql":       &bintree{_000007_update_cluster_status_enumDownSql, map[string]*bintree{}},
	"000007_update_cluster_status_enum.up.sql":         &bintree{_000007_update_cluster_status_enumUpSql, map[string]*bintree{}},
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
