// Code generated for package schema by go-bindata DO NOT EDIT. (@generated)
// sources:
// 000001_create_plugin_releases_table.down.sql
// 000001_create_plugin_releases_table.up.sql
// 000002_create_retention_releases_table.down.sql
// 000002_create_retention_releases_table.up.sql
// 000003_create_org_retention_table.down.sql
// 000003_create_org_retention_table.up.sql
// 000004_create_retention_scripts_table.down.sql
// 000004_create_retention_scripts_table.up.sql
// 000005_fix_preset_scripts_type.down.sql
// 000005_fix_preset_scripts_type.up.sql
// 000006_add_pgcrypto.down.sql
// 000006_add_pgcrypto.up.sql
// 000007_update_retention_scripts_table.down.sql
// 000007_update_retention_scripts_table.up.sql
// 000008_drop_plugin_version_retention_scripts.down.sql
// 000008_drop_plugin_version_retention_scripts.up.sql
// 000009_add_plugin_custom_export_url.down.sql
// 000009_add_plugin_custom_export_url.up.sql
// 000010_add_retention_plugin_insecure.down.sql
// 000010_add_retention_plugin_insecure.up.sql
// 000011_add_retention_plugin_insecure_default.down.sql
// 000011_add_retention_plugin_insecure_default.up.sql
// 000012_drop_unique_name_constraint.down.sql
// 000012_drop_unique_name_constraint.up.sql
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

var __000001_create_plugin_releases_tableDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\x08\x71\x74\xf2\x71\x55\xf0\x74\x53\x70\x8d\xf0\x0c\x0e\x09\x56\x28\xc8\x29\x4d\xcf\xcc\x8b\x2f\x4a\xcd\x49\x4d\x2c\x4e\x2d\xb6\xe6\x02\x04\x00\x00\xff\xff\x04\x3a\xd0\x5c\x26\x00\x00\x00")

func _000001_create_plugin_releases_tableDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000001_create_plugin_releases_tableDownSql,
		"000001_create_plugin_releases_table.down.sql",
	)
}

func _000001_create_plugin_releases_tableDownSql() (*asset, error) {
	bytes, err := _000001_create_plugin_releases_tableDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000001_create_plugin_releases_table.down.sql", size: 38, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000001_create_plugin_releases_tableUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x84\x91\x41\x6f\xe2\x30\x10\x85\xef\xf9\x15\xef\x08\x52\x58\xed\xae\xf6\xb6\xa7\xec\x2a\xda\x45\x05\x4a\x21\x50\x71\x8a\x26\xf1\x40\x2c\x25\x76\x6a\x4f\xa0\xfd\xf7\x95\x21\x20\x40\x55\x7b\xb3\xfc\xfc\xbe\xf1\x7b\xf3\x77\x91\x26\x59\x8a\x2c\xf9\x33\x49\xd1\xd6\xdd\x4e\x9b\xdc\x71\xcd\xe4\xd9\x63\x10\x01\xa3\x11\xfe\x77\x0d\x99\x91\x63\x52\x54\xd4\x0c\x43\x0d\x63\x6b\x1d\xa4\xe2\xde\xf2\x2d\xc2\xe9\x7a\x4f\xae\xac\xc8\x0d\x7e\x7c\xff\xf9\x6b\x88\xd9\x63\x86\xd9\x6a\x32\x89\x4f\x9c\x04\x9d\xd1\x2f\x1d\x43\x2b\x36\xa2\xb7\x9a\xdd\x1d\x27\x86\x6f\xb9\x0c\x8a\x42\xf1\x76\x25\xe0\xe0\xb4\xb0\x0b\x73\xb4\xfa\x6a\x8a\x62\x5f\x3a\xdd\x8a\xb6\x06\x54\xd8\x4e\xee\x7e\x7a\xad\xdf\xa0\x7a\xc2\x72\xfd\xef\xf2\xaf\xda\xee\x2c\xc4\xa2\xf3\x1f\x65\x3e\xa9\xfc\x2a\xbd\x33\xab\x18\x9e\x9b\x35\x3b\xec\xd9\xf9\x30\xc0\x6e\xaf\x63\xf4\xd5\x06\xef\xf9\xc1\xa7\x61\x02\x50\x74\xc3\x5e\xa8\x69\x41\x82\x43\xa5\xcb\xea\xa6\x18\xf2\xe8\x5a\x45\xc2\x2a\x50\xfb\x63\x4e\x82\x6c\x3c\x4d\x97\x59\x32\x9d\xf7\xac\xe7\x8a\xa5\x62\x07\x45\x42\x70\x2c\x61\x07\xd6\x40\x7b\xb0\x09\x8b\x55\x77\x01\x61\x1d\x8c\x95\x63\x63\x24\x94\x5f\x2c\xf9\xf9\x7d\x61\x6d\xcd\x64\xe2\x28\x02\xe6\x8b\xf1\x34\x59\x6c\xf0\x90\x6e\x30\xd0\x2a\x3e\xe7\x3b\x96\xba\x9a\x8d\x9f\x56\xe9\xed\x7d\x34\xfc\x1d\xbd\x07\x00\x00\xff\xff\xad\x69\xfe\xd8\x7e\x02\x00\x00")

func _000001_create_plugin_releases_tableUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000001_create_plugin_releases_tableUpSql,
		"000001_create_plugin_releases_table.up.sql",
	)
}

func _000001_create_plugin_releases_tableUpSql() (*asset, error) {
	bytes, err := _000001_create_plugin_releases_tableUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000001_create_plugin_releases_table.up.sql", size: 638, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000002_create_retention_releases_tableDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\x08\x71\x74\xf2\x71\x55\xf0\x74\x53\x70\x8d\xf0\x0c\x0e\x09\x56\x48\x49\x2c\x49\x8c\x2f\x4a\x2d\x49\xcd\x2b\xc9\xcc\xcf\x8b\x2f\xc8\x29\x4d\xcf\xcc\x8b\x2f\x4a\xcd\x49\x4d\x2c\x4e\x2d\xb6\xe6\x02\x04\x00\x00\xff\xff\xd6\x40\x79\xfe\x35\x00\x00\x00")

func _000002_create_retention_releases_tableDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000002_create_retention_releases_tableDownSql,
		"000002_create_retention_releases_table.down.sql",
	)
}

func _000002_create_retention_releases_tableDownSql() (*asset, error) {
	bytes, err := _000002_create_retention_releases_tableDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000002_create_retention_releases_table.down.sql", size: 53, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000002_create_retention_releases_tableUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x8c\x93\x5f\x6f\xd3\x30\x14\xc5\xdf\xf3\x29\xce\xdb\x3a\x29\x9b\xf8\xb7\xbd\xf0\x54\x86\x87\x06\x25\x85\xac\x05\x4d\x08\x45\x4e\x7c\xd3\x18\xa5\x76\xe6\x3f\xed\x2a\xc4\x77\x47\x76\x93\x2e\x54\xaa\xc4\x5b\xeb\xf8\xde\xf3\xbb\xe7\x5c\xdf\xe4\x6c\xba\x60\x58\x4c\xdf\xcd\x18\x04\x77\xbc\x30\xe4\x48\x39\xa9\x55\xd1\xb5\x7e\x25\x55\x61\xa8\x25\x6e\xc9\x62\x92\x00\x17\x17\x58\x34\x04\xaf\xe4\xa3\x27\x48\x11\xae\xd6\x92\x0c\x6a\x6d\xe0\x1a\xc2\xbe\xe8\x32\x41\xff\xab\x90\x02\x1b\x6e\xaa\x86\x9b\xc9\xcb\x17\xaf\xde\x9c\x23\x9b\x2f\x90\x2d\x67\xb3\xf4\xb9\x9d\xa5\xf5\x37\x32\xd8\x90\xb1\x52\x2b\xe8\x7a\xd4\x0a\xbd\x3e\x5c\xc3\x5d\x38\xb7\x84\x4a\xab\x5a\xae\xbc\xe1\x81\xd3\xa2\xa4\x56\xab\x15\x9c\x0e\xb2\x43\x93\xff\x10\x75\x41\xe9\xa8\xd7\xa0\x02\x6f\xc9\x40\x11\x09\x0b\xa7\x61\x3b\xaa\x64\xbd\x83\x54\xd0\x46\x90\x09\x67\x43\x25\xc5\xfb\xc1\x3c\x1c\xcc\x1b\xf9\x70\x24\xf0\xcb\x6a\x75\xc0\xc1\x7b\x76\x3b\x5d\xce\x16\x38\xfb\xfd\xe7\xac\x67\x9b\xa2\x95\x36\xa2\x75\x86\x02\xa4\xad\x8c\xec\x9c\x4d\xc3\xff\xad\x91\xce\x91\x42\xb9\xdb\x5b\x64\xf4\x46\x06\x9c\x6d\x23\xab\xe6\x99\xbb\xe2\x0a\xa4\x78\xd9\xd2\x25\x18\xaf\x1a\x7c\xbc\x9f\x67\x90\x16\xf4\xd4\x51\xe5\x48\xf4\xfc\x8e\x4b\x05\xc5\xd7\x94\xf6\x2a\x29\xb8\x12\x10\x54\x73\xdf\x3a\xd4\x86\x1e\x3d\xa9\x6a\x17\xf3\x8c\x34\x45\x4f\x13\xe7\xf8\xf1\xf3\xc0\xbc\xcc\x67\x3d\x44\xa7\xa5\x72\xd1\x34\x8e\x8e\xaf\x06\x48\xa9\x56\x10\xba\xf2\x6b\x52\x2e\x5a\x01\x5e\x6a\xef\xc6\x49\x0f\xd3\x9c\xd9\xd3\x6e\xfe\xd3\xa2\xf0\xa6\x3d\x04\x7d\x7d\x75\xf5\xfa\xfa\x7c\x14\xf0\x30\x05\x3d\x75\xda\x38\x90\x12\x11\xad\xc7\x8c\x0a\xb6\xd1\xbe\x15\x28\xc3\x36\x28\xd7\x6f\x50\x5f\x57\xec\xeb\x4e\x6b\x7c\x6f\xc8\x35\x64\xa2\xe3\x36\x5a\x3e\x6c\x09\x47\xe5\xad\xd3\xeb\xe8\x8a\xd3\x43\x3a\x3a\xa8\x88\x30\xb1\x34\x43\xaa\x41\x90\xb7\xad\xde\x16\xfb\x92\xb1\x6a\xa9\x75\x4b\x5c\xa5\x49\x02\x7c\xc9\xef\x3e\x4f\xf3\x07\x7c\x62\x0f\x98\x1c\x9e\x56\x3a\xac\x7b\x64\x5a\x66\x77\x5f\x97\xec\xe4\xe7\xdb\x79\xce\xee\x3e\x64\x27\x5b\x20\x67\xb7\x2c\x67\xd9\x0d\xbb\xc7\xd1\xdb\x9f\x8c\xef\x25\xe7\x6f\x93\xbf\x01\x00\x00\xff\xff\x5e\x64\xbc\x09\x38\x04\x00\x00")

func _000002_create_retention_releases_tableUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000002_create_retention_releases_tableUpSql,
		"000002_create_retention_releases_table.up.sql",
	)
}

func _000002_create_retention_releases_tableUpSql() (*asset, error) {
	bytes, err := _000002_create_retention_releases_tableUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000002_create_retention_releases_table.up.sql", size: 1080, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000003_create_org_retention_tableDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\x08\x71\x74\xf2\x71\x55\xf0\x74\x53\x70\x8d\xf0\x0c\x0e\x09\x56\xc8\x2f\x4a\x8f\x4f\x49\x2c\x49\x8c\x2f\x4a\x2d\x49\xcd\x2b\xc9\xcc\xcf\x8b\x2f\xc8\x29\x4d\xcf\xcc\x2b\xb6\xe6\x02\x04\x00\x00\xff\xff\x00\xb0\xba\xc2\x31\x00\x00\x00")

func _000003_create_org_retention_tableDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000003_create_org_retention_tableDownSql,
		"000003_create_org_retention_table.down.sql",
	)
}

func _000003_create_org_retention_tableDownSql() (*asset, error) {
	bytes, err := _000003_create_org_retention_tableDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000003_create_org_retention_table.down.sql", size: 49, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000003_create_org_retention_tableUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x7c\x91\xcd\x6e\xea\x30\x10\x85\xf7\x79\x8a\xb3\x4c\xa4\xc0\xfd\xd1\xdd\xdd\x15\x05\x53\xa5\xa5\xa1\x0d\xc9\x82\x55\x64\x92\x09\xb6\x14\xd9\xc8\x4e\x40\xbc\x7d\xe5\xfc\x00\x45\xb4\x3b\x5b\x73\xe6\x7c\x33\x67\xe6\x09\x9b\xa5\x0c\xe9\xec\x69\xc5\xa0\xcd\x3e\x2f\x79\xc3\x73\x43\x0d\xa9\x46\x6a\x95\x1f\xea\x76\x2f\x95\x85\xef\x01\x93\x49\xa7\x90\x25\xa4\x45\x23\xc8\xfd\x70\x12\x1a\x82\xbb\xbf\xb4\xe8\xd5\x20\xc5\x77\x35\x95\xbf\x4a\x69\xbb\xc7\xd4\xc3\xd8\x99\x65\xd1\x02\xf1\x3a\x45\x9c\xad\x56\x61\x6f\xda\x77\xdd\xf8\x46\x0b\xe8\xaa\x7b\x0d\x86\x27\x21\x0b\x71\x41\x3a\xdc\x23\xc4\xd5\xe7\xc8\x4d\x21\xb8\xf1\xff\xfc\xfe\xfb\x2f\xb8\xc7\x1d\xc9\x58\xa9\x15\xec\x81\x0a\x59\x49\xb2\x83\xfd\xc0\x32\x54\x13\xb7\xf4\x88\xe6\x20\x63\xf7\x8f\x88\xb9\x56\x95\xdc\xb7\x86\xbb\x0c\x2d\x0a\xad\x1a\xee\x52\x74\x9e\xad\x25\x33\x29\x06\x01\xb9\x59\xeb\x96\x2c\x2a\x6d\x6e\x36\x9e\x22\x75\x79\x5a\xa1\xdb\xba\x44\xa5\xeb\x5a\x9f\xba\x72\xf1\xd5\x79\xdc\xa1\x84\x54\x78\x78\xba\x7c\xd8\xc7\x4e\xfb\xd1\x52\x41\x3d\xd2\x85\xcd\xdd\xad\x0a\x73\x3e\x34\x54\xe2\x65\xb3\x8e\x9d\xe8\x0e\xb1\x3b\x37\xc4\x43\xcf\x03\xde\x93\xe8\x6d\x96\x6c\xf1\xca\xb6\xf0\xfb\x7b\x86\xd7\xd0\x03\xb7\x7b\x16\x47\x1f\x19\xfb\xae\xba\x5c\x27\x2c\x7a\x8e\x7b\x83\x4b\x29\x1c\x33\x0d\x90\xb0\x25\x4b\x58\x3c\x67\x1b\xdc\x4d\xef\xdf\xea\xbc\xe0\xbf\xf7\x19\x00\x00\xff\xff\x07\xa0\x1b\xd3\xb9\x02\x00\x00")

func _000003_create_org_retention_tableUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000003_create_org_retention_tableUpSql,
		"000003_create_org_retention_table.up.sql",
	)
}

func _000003_create_org_retention_tableUpSql() (*asset, error) {
	bytes, err := _000003_create_org_retention_tableUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000003_create_org_retention_table.up.sql", size: 697, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000004_create_retention_scripts_tableDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\x08\x71\x74\xf2\x71\x55\xf0\x74\x53\x70\x8d\xf0\x0c\x0e\x09\x56\x28\xc8\x29\x4d\xcf\xcc\x8b\x2f\x4a\x2d\x49\xcd\x2b\xc9\xcc\xcf\x8b\x2f\x4e\x2e\xca\x2c\x28\x29\xb6\xe6\x02\x04\x00\x00\xff\xff\x7f\x9b\x06\x2e\x2f\x00\x00\x00")

func _000004_create_retention_scripts_tableDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000004_create_retention_scripts_tableDownSql,
		"000004_create_retention_scripts_table.down.sql",
	)
}

func _000004_create_retention_scripts_tableDownSql() (*asset, error) {
	bytes, err := _000004_create_retention_scripts_tableDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000004_create_retention_scripts_table.down.sql", size: 47, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000004_create_retention_scripts_tableUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x9c\x94\xcf\x6e\xdb\x30\x0c\xc6\xef\x79\x8a\xef\x98\x00\x69\xb1\x7f\xed\x65\xa7\xae\x75\x07\x63\x59\xda\x79\xf5\xa1\x18\x06\x43\xb1\x99\x5a\x83\x2a\x79\xa2\xdc\xb4\x6f\x3f\xc8\x96\x6c\x27\xcb\x7a\xd8\x4d\x14\xc9\xdf\x27\x12\xa4\x2e\xb3\xe4\xe2\x2e\xc1\xdd\xc5\xa7\x55\x82\x46\xb5\x0f\x52\x17\x96\x1c\x69\x27\x8d\x2e\xb8\xb4\xb2\x71\x8c\xf9\x0c\x38\x39\x81\xb1\x0f\x85\xac\x20\x19\xae\x26\x6f\x61\x57\x1b\xd4\xc2\xdb\x92\xd1\x47\x83\xb4\xd8\x28\xaa\x4e\x67\x88\x09\x79\x9e\x5e\x61\x7d\x73\x87\x75\xbe\x5a\x2d\x7b\x56\xd0\x1a\x71\xe9\x15\xcc\xb6\x3b\xf5\x2e\xb8\x5a\xb8\xce\x0e\x5c\xc9\x11\x8d\xad\xb1\x1e\x3f\x32\x9e\x84\x2d\x6b\x61\xe7\x6f\xdf\xbc\xfb\xb0\x38\x94\x7a\x22\xcb\xd2\xe8\x28\x14\xcd\xff\x54\x8b\xe9\xaf\x4a\xf6\x90\xa3\xd5\x05\xbe\xd9\xfc\xa2\xd2\x79\xee\x18\x7b\xac\x4f\xc1\xab\xc5\x23\x45\x56\x77\xde\xa3\x4d\x30\x9d\xf3\xd5\xb7\x55\xd4\x87\x86\x96\x88\xbd\x8b\xbf\xb0\x53\x67\xc4\x9e\x9f\x9d\xbd\x3f\x5f\x04\x5c\x69\xb4\x1f\x17\xee\x0e\x42\xea\xfe\x8d\xa2\x74\xad\x50\xb8\x7d\x5e\x4d\x58\x43\x68\x00\x05\xc2\xd6\xd2\xef\x96\x74\xf9\x52\xb0\x7f\x50\x6d\x76\x30\x5b\x47\x7a\xda\x2e\xae\x4d\xab\x2a\xd8\x56\x2f\x21\x35\x98\x4a\xa3\x2b\xf6\xd0\xbd\x6c\xed\x02\x93\x9e\x1b\x63\x5d\xd1\x5a\x15\xbb\x96\x67\x2b\xec\x6a\x59\xd6\x9d\x55\x09\x27\xb0\x93\x4a\x61\x43\x60\xd2\x0e\xce\x78\xda\x24\xef\x78\xb5\xaa\x65\x47\xb6\x90\x15\x47\xb0\x92\xec\x7c\xdf\x82\x8b\x07\x95\x71\x25\xc6\xd7\xc3\xe8\x53\xa4\x5b\xd0\x63\xe3\x5e\x96\x10\xcc\xed\x23\x31\xa4\xf3\x4e\x86\xd1\x10\x4a\x8d\x28\xa9\xe3\xaa\x75\xfd\x9b\x88\xfb\x59\xf9\xf1\x33\x56\x1b\x26\x55\x7a\x71\xb2\xf4\xef\x41\xc6\xb0\xdc\x5d\xb5\xc1\xb1\x31\x46\x91\xd0\x81\x26\xb9\x68\x2c\x31\xb9\xc0\x73\x35\x59\x18\x0b\x6d\x0e\x37\x44\x20\x04\x86\xab\xc6\x9a\x27\x59\x79\xe0\xcb\x74\xb7\xc2\x75\xb7\x45\x23\x7c\x10\x9d\x01\xb7\x59\xfa\xf5\x22\xbb\xc7\x97\xe4\x1e\xf3\x61\x21\xba\xa6\xe7\xeb\xf4\x5b\x9e\x60\xde\xff\x25\xcb\xe9\x9c\x77\xfe\xeb\x9b\x2c\x49\x3f\xaf\xfb\xd4\xe1\x47\x58\x1e\xac\xeb\x02\x59\x72\x9d\x64\xc9\xfa\x32\xf9\x3e\xfe\x73\x8a\x04\x13\xcf\x7d\x78\x8c\x9b\x2d\x3e\xce\xfe\x04\x00\x00\xff\xff\xa5\xd5\x8e\x8d\x15\x05\x00\x00")

func _000004_create_retention_scripts_tableUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000004_create_retention_scripts_tableUpSql,
		"000004_create_retention_scripts_table.up.sql",
	)
}

func _000004_create_retention_scripts_tableUpSql() (*asset, error) {
	bytes, err := _000004_create_retention_scripts_tableUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000004_create_retention_scripts_table.up.sql", size: 1301, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000005_fix_preset_scripts_typeDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x48\x49\x2c\x49\x8c\x2f\x4a\x2d\x49\xcd\x2b\xc9\xcc\xcf\x8b\x2f\xc8\x29\x4d\xcf\xcc\x8b\x2f\x4a\xcd\x49\x4d\x2c\x4e\x2d\x56\x70\x09\xf2\x0f\x50\x70\xf6\xf7\x09\xf5\xf5\x53\x28\x28\x4a\x2d\x4e\x2d\x89\x2f\x4e\x2e\xca\x2c\x28\x29\xb6\xe6\x22\xc1\x18\x47\x17\x17\x34\xed\x0a\x59\xc5\xf9\x79\xd1\xb1\xd6\x5c\x80\x00\x00\x00\xff\xff\x6e\xff\x03\x83\x8d\x00\x00\x00")

func _000005_fix_preset_scripts_typeDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000005_fix_preset_scripts_typeDownSql,
		"000005_fix_preset_scripts_type.down.sql",
	)
}

func _000005_fix_preset_scripts_typeDownSql() (*asset, error) {
	bytes, err := _000005_fix_preset_scripts_typeDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000005_fix_preset_scripts_type.down.sql", size: 141, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000005_fix_preset_scripts_typeUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x48\x49\x2c\x49\x8c\x2f\x4a\x2d\x49\xcd\x2b\xc9\xcc\xcf\x8b\x2f\xc8\x29\x4d\xcf\xcc\x8b\x2f\x4a\xcd\x49\x4d\x2c\x4e\x2d\x56\x70\x09\xf2\x0f\x50\x70\xf6\xf7\x09\xf5\xf5\x53\x28\x28\x4a\x2d\x4e\x2d\x89\x2f\x4e\x2e\xca\x2c\x28\x29\xb6\xe6\x22\xc1\x18\x47\x17\x17\x34\xed\x0a\x49\x95\x25\xa9\x89\xd6\x5c\x80\x00\x00\x00\xff\xff\xf5\x39\xd0\xc6\x8c\x00\x00\x00")

func _000005_fix_preset_scripts_typeUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000005_fix_preset_scripts_typeUpSql,
		"000005_fix_preset_scripts_type.up.sql",
	)
}

func _000005_fix_preset_scripts_typeUpSql() (*asset, error) {
	bytes, err := _000005_fix_preset_scripts_typeUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000005_fix_preset_scripts_type.up.sql", size: 140, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000006_add_pgcryptoDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\x70\x8d\x08\x71\xf5\x0b\xf6\xf4\xf7\x53\xf0\x74\x53\x70\x8d\xf0\x0c\x0e\x09\x56\x50\x2a\x48\x4f\x2e\xaa\x2c\x28\xc9\x57\xb2\xe6\x02\x04\x00\x00\xff\xff\x57\x85\x06\xc2\x25\x00\x00\x00")

func _000006_add_pgcryptoDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000006_add_pgcryptoDownSql,
		"000006_add_pgcrypto.down.sql",
	)
}

func _000006_add_pgcryptoDownSql() (*asset, error) {
	bytes, err := _000006_add_pgcryptoDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000006_add_pgcrypto.down.sql", size: 37, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000006_add_pgcryptoUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x0e\x72\x75\x0c\x71\x55\x70\x8d\x08\x71\xf5\x0b\xf6\xf4\xf7\x53\xf0\x74\x53\xf0\xf3\x0f\x51\x70\x8d\xf0\x0c\x0e\x09\x56\x50\x2a\x48\x4f\x2e\xaa\x2c\x28\xc9\x57\xb2\xe6\x02\x04\x00\x00\xff\xff\x29\x5f\xd6\xbb\x2b\x00\x00\x00")

func _000006_add_pgcryptoUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000006_add_pgcryptoUpSql,
		"000006_add_pgcrypto.up.sql",
	)
}

func _000006_add_pgcryptoUpSql() (*asset, error) {
	bytes, err := _000006_add_pgcryptoUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000006_add_pgcrypto.up.sql", size: 43, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000007_update_retention_scripts_tableDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x9c\x94\x4f\x6f\xdb\x3c\x0c\xc6\xef\xfe\x14\xcf\x31\x01\x92\xe2\x7d\xb7\xb5\x97\x9e\xba\xd6\x1d\x8c\x65\x69\xe7\x26\xc0\x8a\x61\x30\x14\x9b\xa9\xb5\x29\x92\x27\xca\x49\xfb\xed\x07\xd9\xf2\x9f\x76\x69\x0f\xbb\x89\x22\xf9\x7b\x44\x82\xd4\x55\x7a\x73\x8b\xd5\xc5\xc7\x45\x8c\xe4\x1a\xf1\xb7\xe4\x6e\x75\x87\x4a\xd5\x0f\x52\x67\x96\x1c\x69\x27\x8d\xce\x38\xb7\xb2\x72\x7c\x1e\x45\xf3\x39\x52\xda\x93\x75\xd8\x88\xfc\x17\x9c\x41\x65\x69\x2f\x4d\xcd\x70\x62\xa3\x08\x9c\x97\xb4\x13\x27\xd1\x65\x1a\x5f\xac\xe2\x80\x7e\x0d\x88\x49\x04\xcc\xe7\x30\xf6\x21\x93\x05\x24\xc3\x95\xe4\x2d\x1c\x4a\x83\x52\x78\x5b\x32\xda\x68\x90\xf6\x0a\xc5\x49\x84\x2e\x61\xbd\x4e\xae\xb0\xbc\x59\x61\xb9\x5e\x2c\x66\x2d\x2b\x68\x0d\xb8\xe4\x0a\x66\xdb\x9c\x5a\x17\x5c\x29\x5c\x63\x07\xae\xe4\x0e\x8d\xad\xb1\x1e\x3f\x30\xf6\xc2\xe6\xa5\xb0\x93\xff\xff\x7b\xf7\x61\xfa\x52\x6a\x4f\x96\xa5\xd1\x9d\x50\x67\xfe\xa3\x5a\x97\xfe\xa6\x64\x0b\x39\x5a\x5d\xe0\x9b\xcd\x4f\xca\x9d\xe7\x0e\xb1\xc7\xfa\x14\xbc\x5a\xec\xa8\x63\x35\xe7\x67\xb4\x11\xa6\x71\xbe\xf9\xb6\x82\xda\xd0\xd0\x12\xf1\xec\xe2\x2f\xec\xd8\xd9\x61\xcf\x4e\x4f\xdf\x9f\x4d\x03\x2e\x37\xda\x8f\x0b\x37\x07\x21\x75\xfb\x46\x91\xbb\x5a\x28\xdc\x3e\x2e\x46\xac\x3e\x34\x80\x02\x61\x6b\xe9\x77\x4d\x3a\x7f\xca\xd8\x3f\xa8\x34\x07\x98\xad\x23\x3d\x6e\x17\x97\xa6\x56\x05\x6c\xad\x67\x90\x1a\x4c\xb9\xd1\x05\x7b\xe8\xb3\x6c\xed\x02\x93\x1e\x2b\x63\x5d\x56\x5b\xd5\x75\x6d\x9d\x2e\x70\x28\x65\x5e\x36\x56\x21\x9c\xc0\x41\x2a\x85\x0d\x81\x49\x3b\x38\xe3\x69\xa3\xbc\xe3\xd5\xaa\x9a\x1d\xd9\x4c\x16\xdc\x81\x95\x64\xe7\xfb\x16\x5c\xdc\xab\x0c\x2b\x31\xbc\x1e\x46\x9f\x20\xd9\x82\x76\x95\x7b\x9a\x41\x30\xd7\x3b\x62\x48\xe7\x9d\x0c\xa3\x21\x94\x1a\x50\x52\x77\xab\xd6\xf4\x6f\x24\xee\x67\xe5\xfb\x8f\xae\xda\x30\xa9\xd2\x8b\x93\xa5\xd7\x07\x19\xfd\x72\x37\xd5\x06\xc7\xc6\x18\x45\x42\x07\x9a\xe4\xac\xb2\xc4\xe4\x02\xcf\x95\x64\x61\x2c\xb4\x79\xb9\x21\x02\x21\x30\x5c\x55\xd6\xec\x65\xe1\x81\x4f\xe3\xdd\x0a\xd7\xcd\x16\x0d\xf0\x5e\x34\x02\x6e\xd3\xe4\xcb\x45\x7a\x8f\xcf\xf1\x3d\x26\xfd\x42\x34\x4d\x5f\x2f\x93\xaf\xeb\x18\x93\xf6\x2f\x99\x8d\xe7\xbc\xf1\x5f\xdf\xa4\x71\xf2\x69\xd9\xa6\xf6\x3f\xc2\xec\xc5\xba\x4e\x91\xc6\xd7\x71\x1a\x2f\x2f\xe3\xd1\xc7\xa9\x48\x30\xf1\xc4\x87\x77\x71\xd1\xf4\x3c\xfa\x13\x00\x00\xff\xff\x4c\xc3\xfb\x11\x6e\x05\x00\x00")

func _000007_update_retention_scripts_tableDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000007_update_retention_scripts_tableDownSql,
		"000007_update_retention_scripts_table.down.sql",
	)
}

func _000007_update_retention_scripts_tableDownSql() (*asset, error) {
	bytes, err := _000007_update_retention_scripts_tableDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000007_update_retention_scripts_table.down.sql", size: 1390, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000007_update_retention_scripts_tableUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x9c\x93\x4f\x73\xd3\x30\x10\xc5\xef\xfe\x14\xef\x98\xcc\xa4\x1d\xfe\xb5\x97\x9e\x4a\xa3\x80\x87\x90\x14\x37\x9e\xa1\x27\x8f\x6c\x6f\x23\x81\x91\x3c\x92\xe2\x90\x6f\xcf\x58\x96\x12\x37\x94\x1e\xb8\x79\xb5\x6f\x7f\x4f\xde\x5d\xcd\xb3\xf5\x3d\x36\xb7\x1f\x97\x0c\xe9\x02\xec\x7b\xfa\xb0\x79\x40\xdb\xec\xb6\x52\x15\x86\x1c\x29\x27\xb5\x2a\x6c\x65\x64\xeb\xec\x4d\x92\xdc\x65\xec\x76\xc3\x42\xc5\xbf\x74\x98\x24\xc0\xc5\x05\xb4\xd9\x16\xb2\x86\xb4\x70\x82\xfa\x08\x7b\xa1\x21\x78\x1f\x4b\x8b\x41\x0d\x52\xbc\x6c\xa8\xbe\x4c\x10\x0b\xf2\x3c\x9d\x63\xb5\xde\x60\x95\x2f\x97\xb3\x81\x15\xbc\x4e\xb8\x74\x0e\xfd\xe4\xbf\x86\x14\x9c\xe0\xce\xc7\x81\x2b\x6d\x44\xe3\x49\x9b\x1e\x7f\x62\x74\xdc\x54\x82\x9b\xc9\xdb\x37\xef\x3e\x4c\xcf\xad\x3a\x32\x56\x6a\x15\x8d\x62\xf8\x9f\x6e\xb1\xfc\x55\xcb\x01\xf2\xe2\xdf\x05\xbe\x2e\x7f\x50\xe5\xe0\x9d\x09\x95\xd1\x2a\x66\x2c\x99\x4e\x56\xd4\x5b\x9e\x30\x2f\xb5\x30\x64\x15\xff\x45\xd1\xc6\x7f\x3f\x33\x1a\x61\x7c\xf2\xd5\x6b\xd7\x34\x48\x43\xb7\xf8\xb3\x83\xbf\xb0\xe3\x64\xc4\x5e\x5f\x5d\xbd\xbf\x9e\x06\x1c\xfd\x6e\xb5\x71\xc5\xce\x34\xf1\x7e\x79\xb6\xc4\x5e\xc8\x4a\xf8\xa8\xe6\x8e\x63\x2f\x9b\x06\x25\xc1\x92\x72\x70\xfa\x12\x9f\xb9\x15\x54\x63\x67\xa5\xda\x1e\x65\x25\xb7\x84\x9f\x74\xe8\x87\x81\xd6\xc8\x8e\x57\x87\xfe\x0e\x23\x8b\xf2\xe0\x88\x07\x67\x69\x8b\xd6\x90\x25\x3f\xc7\xbd\x20\x27\xc8\x40\x1b\x28\x7d\x3e\x64\x8e\x20\x0c\x47\xad\xd1\x9d\xac\xa9\x46\x79\x18\xaf\x47\x38\xf6\x8b\x70\x82\x97\x5a\x37\xc4\xd5\x2c\x49\x80\xfb\x2c\xfd\x7a\x9b\x3d\xe2\x0b\x7b\xc4\xe4\x38\x38\xdf\x8a\x7c\x95\x7e\xcb\x19\x26\xc3\x73\x98\x8d\xe7\xe1\xf3\x8b\x75\xc6\xd2\x4f\xab\xa1\xf4\xb8\xd4\xb3\xb3\x8d\x9b\x22\x63\x0b\x96\xb1\xd5\x1d\x1b\x3d\xe9\x86\xb8\x25\x3b\xe9\xe5\x51\x97\x4c\x6f\x92\x3f\x01\x00\x00\xff\xff\x69\x60\x3b\x71\x08\x04\x00\x00")

func _000007_update_retention_scripts_tableUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000007_update_retention_scripts_tableUpSql,
		"000007_update_retention_scripts_table.up.sql",
	)
}

func _000007_update_retention_scripts_tableUpSql() (*asset, error) {
	bytes, err := _000007_update_retention_scripts_tableUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000007_update_retention_scripts_table.up.sql", size: 1032, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000008_drop_plugin_version_retention_scriptsDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\xc8\x29\x4d\xcf\xcc\x8b\x2f\x4a\x2d\x49\xcd\x2b\xc9\xcc\xcf\x8b\x2f\x4e\x2e\xca\x2c\x28\x29\x56\x70\x74\x71\x51\x70\xf6\xf7\x09\xf5\xf5\x83\xa9\x29\x4b\x2d\x2a\xce\xcc\xcf\x53\x28\x4b\x2c\x4a\xce\x48\x2c\xd2\x30\x34\x30\x32\xd1\x54\xf0\xf3\x0f\x51\xf0\x0b\xf5\xf1\xb1\xe6\x02\x04\x00\x00\xff\xff\x1f\xc6\x09\xad\x57\x00\x00\x00")

func _000008_drop_plugin_version_retention_scriptsDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000008_drop_plugin_version_retention_scriptsDownSql,
		"000008_drop_plugin_version_retention_scripts.down.sql",
	)
}

func _000008_drop_plugin_version_retention_scriptsDownSql() (*asset, error) {
	bytes, err := _000008_drop_plugin_version_retention_scriptsDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000008_drop_plugin_version_retention_scripts.down.sql", size: 87, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000008_drop_plugin_version_retention_scriptsUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\xc8\x29\x4d\xcf\xcc\x8b\x2f\x4a\x2d\x49\xcd\x2b\xc9\xcc\xcf\x8b\x2f\x4e\x2e\xca\x2c\x28\x29\x56\x70\x09\xf2\x0f\x50\x70\xf6\xf7\x09\xf5\xf5\x83\x29\x2a\x4b\x2d\x2a\xce\xcc\xcf\xb3\xe6\x02\x04\x00\x00\xff\xff\xd0\x69\xa2\xcb\x41\x00\x00\x00")

func _000008_drop_plugin_version_retention_scriptsUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000008_drop_plugin_version_retention_scriptsUpSql,
		"000008_drop_plugin_version_retention_scripts.up.sql",
	)
}

func _000008_drop_plugin_version_retention_scriptsUpSql() (*asset, error) {
	bytes, err := _000008_drop_plugin_version_retention_scriptsUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000008_drop_plugin_version_retention_scripts.up.sql", size: 65, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000009_add_plugin_custom_export_urlDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\xc8\x2f\x4a\x8f\x4f\x49\x2c\x49\x8c\x2f\x4a\x2d\x49\xcd\x2b\xc9\xcc\xcf\x8b\x2f\xc8\x29\x4d\xcf\xcc\x2b\x56\x70\x09\xf2\x0f\x50\x70\xf6\xf7\x09\xf5\xf5\x53\x48\x2e\x2d\x2e\xc9\xcf\x8d\x4f\xad\x28\xc8\x2f\x2a\x89\x2f\x2d\xca\xb1\xe6\x02\x04\x00\x00\xff\xff\x0d\x59\xfa\x58\x46\x00\x00\x00")

func _000009_add_plugin_custom_export_urlDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000009_add_plugin_custom_export_urlDownSql,
		"000009_add_plugin_custom_export_url.down.sql",
	)
}

func _000009_add_plugin_custom_export_urlDownSql() (*asset, error) {
	bytes, err := _000009_add_plugin_custom_export_urlDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000009_add_plugin_custom_export_url.down.sql", size: 70, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000009_add_plugin_custom_export_urlUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\xc8\x2f\x4a\x8f\x4f\x49\x2c\x49\x8c\x2f\x4a\x2d\x49\xcd\x2b\xc9\xcc\xcf\x8b\x2f\xc8\x29\x4d\xcf\xcc\x2b\x56\x70\x74\x71\x51\x48\x2e\x2d\x2e\xc9\xcf\x8d\x4f\xad\x28\xc8\x2f\x2a\x89\x2f\x2d\xca\x51\x48\xaa\x2c\x49\x4d\xb4\xe6\x02\x04\x00\x00\xff\xff\x53\xf3\x1b\x24\x44\x00\x00\x00")

func _000009_add_plugin_custom_export_urlUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000009_add_plugin_custom_export_urlUpSql,
		"000009_add_plugin_custom_export_url.up.sql",
	)
}

func _000009_add_plugin_custom_export_urlUpSql() (*asset, error) {
	bytes, err := _000009_add_plugin_custom_export_urlUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000009_add_plugin_custom_export_url.up.sql", size: 68, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000010_add_retention_plugin_insecureDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\xc8\x2f\x4a\x8f\x4f\x49\x2c\x49\x8c\x2f\x4a\x2d\x49\xcd\x2b\xc9\xcc\xcf\x8b\x2f\xc8\x29\x4d\xcf\xcc\x2b\x56\x70\x09\xf2\x0f\x50\x70\xf6\xf7\x09\xf5\xf5\x53\xc8\xcc\x2b\x4e\x4d\x2e\x2d\x4a\x8d\x2f\xc9\x29\xb6\xe6\x42\x36\x00\xab\xe6\xf8\xa2\xd4\x9c\xd4\xc4\xe2\x54\x54\x43\x12\x73\x72\xf2\xcb\xe3\x51\x8d\x02\x04\x00\x00\xff\xff\x7e\x73\xac\xa5\x8c\x00\x00\x00")

func _000010_add_retention_plugin_insecureDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000010_add_retention_plugin_insecureDownSql,
		"000010_add_retention_plugin_insecure.down.sql",
	)
}

func _000010_add_retention_plugin_insecureDownSql() (*asset, error) {
	bytes, err := _000010_add_retention_plugin_insecureDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000010_add_retention_plugin_insecure.down.sql", size: 140, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000010_add_retention_plugin_insecureUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x6c\xcc\x31\x0a\x02\x41\x0c\x05\xd0\xde\x53\xe4\x1e\x56\x23\xbb\xdd\x56\xb2\x7d\x88\xfa\x19\x06\x42\x22\x49\x06\xaf\x6f\x61\x63\x31\x07\x78\xaf\x1d\xe7\x7e\xa7\xb3\xdd\x8e\x9d\x3c\x3a\xbf\xa4\x84\x03\x05\xab\xe1\xc6\x6f\x9d\x7d\x58\x52\xdb\x36\x1a\x96\x78\xce\x00\x97\x26\x3d\xdc\x15\x62\xd7\xcb\x7f\xb0\xc4\x1c\x50\x48\xe2\x97\x88\xaa\x7f\x78\x5d\x7d\x03\x00\x00\xff\xff\xa7\x17\x29\x54\x8c\x00\x00\x00")

func _000010_add_retention_plugin_insecureUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000010_add_retention_plugin_insecureUpSql,
		"000010_add_retention_plugin_insecure.up.sql",
	)
}

func _000010_add_retention_plugin_insecureUpSql() (*asset, error) {
	bytes, err := _000010_add_retention_plugin_insecureUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000010_add_retention_plugin_insecure.up.sql", size: 140, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000011_add_retention_plugin_insecure_defaultDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x7c\xcc\xb1\x0a\x02\x31\x0c\x00\xd0\xdd\xaf\xc8\x7f\x38\x55\xaf\x4e\xd5\x93\xa3\x37\x87\xa0\xe1\x28\x84\x54\x92\x14\x7f\xdf\xc1\x45\x44\xdc\x1f\x2f\x95\x9a\x17\xa8\xe9\x50\x32\xdc\x29\x08\x8d\x83\x35\x5a\x57\x7c\xc8\xd8\x9a\xa2\xb1\x30\x39\x3b\xbc\xe9\x71\x2e\xeb\xf9\x02\x24\xd2\x9f\xd8\xd4\xf9\x36\x8c\x31\xc4\x61\x5a\xe6\x2b\x4c\xf9\x94\xd6\x52\xf7\xbb\xcf\xb8\xdb\x86\x3f\xf3\xaf\xf4\x4f\xf7\x0a\x00\x00\xff\xff\x03\xb6\x66\x43\xa8\x00\x00\x00")

func _000011_add_retention_plugin_insecure_defaultDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000011_add_retention_plugin_insecure_defaultDownSql,
		"000011_add_retention_plugin_insecure_default.down.sql",
	)
}

func _000011_add_retention_plugin_insecure_defaultDownSql() (*asset, error) {
	bytes, err := _000011_add_retention_plugin_insecure_defaultDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000011_add_retention_plugin_insecure_default.down.sql", size: 168, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000011_add_retention_plugin_insecure_defaultUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x84\xcd\xb1\x0a\x42\x31\x0c\x46\xe1\xdd\xa7\xc8\x7b\x38\x55\xad\x53\x55\xd0\xde\x39\x04\xfd\xbd\x14\x42\x2b\x49\x2e\xbe\xbe\x83\x8b\x88\xe0\x7e\xf8\x4e\x2a\x35\x9f\xa9\xa6\x4d\xc9\x74\x93\x10\x36\x04\x7a\xb4\xd1\xf9\xa1\xcb\xdc\x3a\x1b\x14\xe2\x70\x7a\xa7\xdb\x53\x99\x0e\x47\x12\xd5\xf1\xe4\xd6\x1d\xd7\xc5\xc0\xa1\x4e\x97\x5c\x69\x97\xf7\x69\x2a\x95\xee\xa2\x8e\xf5\xea\x53\x1f\x36\xf3\xcf\xc3\x97\xfc\xcf\x7c\x05\x00\x00\xff\xff\x42\xea\x72\x0b\xb2\x00\x00\x00")

func _000011_add_retention_plugin_insecure_defaultUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000011_add_retention_plugin_insecure_defaultUpSql,
		"000011_add_retention_plugin_insecure_default.up.sql",
	)
}

func _000011_add_retention_plugin_insecure_defaultUpSql() (*asset, error) {
	bytes, err := _000011_add_retention_plugin_insecure_defaultUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000011_add_retention_plugin_insecure_default.up.sql", size: 178, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000012_drop_unique_name_constraintDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\xc8\x29\x4d\xcf\xcc\x8b\x2f\x4a\x2d\x49\xcd\x2b\xc9\xcc\xcf\x8b\x2f\x4e\x2e\xca\x2c\x28\x29\x56\x70\x74\x71\x51\x70\xf6\xf7\x0b\x0e\x09\x72\xf4\xf4\x0b\xc1\xa9\x2e\x3e\xbf\x28\x3d\x3e\x33\x05\xca\x8d\xcf\x4b\xcc\x4d\x8d\xcf\x4e\xad\x54\x08\xf5\xf3\x0c\x0c\x75\x55\xd0\x80\x48\xeb\x28\x20\xc9\x6b\x5a\x73\x01\x02\x00\x00\xff\xff\x0c\x33\xa4\xf9\x82\x00\x00\x00")

func _000012_drop_unique_name_constraintDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000012_drop_unique_name_constraintDownSql,
		"000012_drop_unique_name_constraint.down.sql",
	)
}

func _000012_drop_unique_name_constraintDownSql() (*asset, error) {
	bytes, err := _000012_drop_unique_name_constraintDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000012_drop_unique_name_constraint.down.sql", size: 130, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000012_drop_unique_name_constraintUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\xc8\x29\x4d\xcf\xcc\x8b\x2f\x4a\x2d\x49\xcd\x2b\xc9\xcc\xcf\x8b\x2f\x4e\x2e\xca\x2c\x28\x29\x56\x70\x09\xf2\x0f\x50\x70\xf6\xf7\x0b\x0e\x09\x72\xf4\xf4\x0b\xc1\xa9\x30\x3e\xbf\x28\x3d\x3e\x33\x05\xca\x8d\xcf\x4b\xcc\x4d\x8d\xcf\x4e\xad\xb4\xe6\x02\x04\x00\x00\xff\xff\x09\x7e\xec\xc2\x66\x00\x00\x00")

func _000012_drop_unique_name_constraintUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000012_drop_unique_name_constraintUpSql,
		"000012_drop_unique_name_constraint.up.sql",
	)
}

func _000012_drop_unique_name_constraintUpSql() (*asset, error) {
	bytes, err := _000012_drop_unique_name_constraintUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000012_drop_unique_name_constraint.up.sql", size: 102, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
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
	"000001_create_plugin_releases_table.down.sql":          _000001_create_plugin_releases_tableDownSql,
	"000001_create_plugin_releases_table.up.sql":            _000001_create_plugin_releases_tableUpSql,
	"000002_create_retention_releases_table.down.sql":       _000002_create_retention_releases_tableDownSql,
	"000002_create_retention_releases_table.up.sql":         _000002_create_retention_releases_tableUpSql,
	"000003_create_org_retention_table.down.sql":            _000003_create_org_retention_tableDownSql,
	"000003_create_org_retention_table.up.sql":              _000003_create_org_retention_tableUpSql,
	"000004_create_retention_scripts_table.down.sql":        _000004_create_retention_scripts_tableDownSql,
	"000004_create_retention_scripts_table.up.sql":          _000004_create_retention_scripts_tableUpSql,
	"000005_fix_preset_scripts_type.down.sql":               _000005_fix_preset_scripts_typeDownSql,
	"000005_fix_preset_scripts_type.up.sql":                 _000005_fix_preset_scripts_typeUpSql,
	"000006_add_pgcrypto.down.sql":                          _000006_add_pgcryptoDownSql,
	"000006_add_pgcrypto.up.sql":                            _000006_add_pgcryptoUpSql,
	"000007_update_retention_scripts_table.down.sql":        _000007_update_retention_scripts_tableDownSql,
	"000007_update_retention_scripts_table.up.sql":          _000007_update_retention_scripts_tableUpSql,
	"000008_drop_plugin_version_retention_scripts.down.sql": _000008_drop_plugin_version_retention_scriptsDownSql,
	"000008_drop_plugin_version_retention_scripts.up.sql":   _000008_drop_plugin_version_retention_scriptsUpSql,
	"000009_add_plugin_custom_export_url.down.sql":          _000009_add_plugin_custom_export_urlDownSql,
	"000009_add_plugin_custom_export_url.up.sql":            _000009_add_plugin_custom_export_urlUpSql,
	"000010_add_retention_plugin_insecure.down.sql":         _000010_add_retention_plugin_insecureDownSql,
	"000010_add_retention_plugin_insecure.up.sql":           _000010_add_retention_plugin_insecureUpSql,
	"000011_add_retention_plugin_insecure_default.down.sql": _000011_add_retention_plugin_insecure_defaultDownSql,
	"000011_add_retention_plugin_insecure_default.up.sql":   _000011_add_retention_plugin_insecure_defaultUpSql,
	"000012_drop_unique_name_constraint.down.sql":           _000012_drop_unique_name_constraintDownSql,
	"000012_drop_unique_name_constraint.up.sql":             _000012_drop_unique_name_constraintUpSql,
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
	"000001_create_plugin_releases_table.down.sql":          &bintree{_000001_create_plugin_releases_tableDownSql, map[string]*bintree{}},
	"000001_create_plugin_releases_table.up.sql":            &bintree{_000001_create_plugin_releases_tableUpSql, map[string]*bintree{}},
	"000002_create_retention_releases_table.down.sql":       &bintree{_000002_create_retention_releases_tableDownSql, map[string]*bintree{}},
	"000002_create_retention_releases_table.up.sql":         &bintree{_000002_create_retention_releases_tableUpSql, map[string]*bintree{}},
	"000003_create_org_retention_table.down.sql":            &bintree{_000003_create_org_retention_tableDownSql, map[string]*bintree{}},
	"000003_create_org_retention_table.up.sql":              &bintree{_000003_create_org_retention_tableUpSql, map[string]*bintree{}},
	"000004_create_retention_scripts_table.down.sql":        &bintree{_000004_create_retention_scripts_tableDownSql, map[string]*bintree{}},
	"000004_create_retention_scripts_table.up.sql":          &bintree{_000004_create_retention_scripts_tableUpSql, map[string]*bintree{}},
	"000005_fix_preset_scripts_type.down.sql":               &bintree{_000005_fix_preset_scripts_typeDownSql, map[string]*bintree{}},
	"000005_fix_preset_scripts_type.up.sql":                 &bintree{_000005_fix_preset_scripts_typeUpSql, map[string]*bintree{}},
	"000006_add_pgcrypto.down.sql":                          &bintree{_000006_add_pgcryptoDownSql, map[string]*bintree{}},
	"000006_add_pgcrypto.up.sql":                            &bintree{_000006_add_pgcryptoUpSql, map[string]*bintree{}},
	"000007_update_retention_scripts_table.down.sql":        &bintree{_000007_update_retention_scripts_tableDownSql, map[string]*bintree{}},
	"000007_update_retention_scripts_table.up.sql":          &bintree{_000007_update_retention_scripts_tableUpSql, map[string]*bintree{}},
	"000008_drop_plugin_version_retention_scripts.down.sql": &bintree{_000008_drop_plugin_version_retention_scriptsDownSql, map[string]*bintree{}},
	"000008_drop_plugin_version_retention_scripts.up.sql":   &bintree{_000008_drop_plugin_version_retention_scriptsUpSql, map[string]*bintree{}},
	"000009_add_plugin_custom_export_url.down.sql":          &bintree{_000009_add_plugin_custom_export_urlDownSql, map[string]*bintree{}},
	"000009_add_plugin_custom_export_url.up.sql":            &bintree{_000009_add_plugin_custom_export_urlUpSql, map[string]*bintree{}},
	"000010_add_retention_plugin_insecure.down.sql":         &bintree{_000010_add_retention_plugin_insecureDownSql, map[string]*bintree{}},
	"000010_add_retention_plugin_insecure.up.sql":           &bintree{_000010_add_retention_plugin_insecureUpSql, map[string]*bintree{}},
	"000011_add_retention_plugin_insecure_default.down.sql": &bintree{_000011_add_retention_plugin_insecure_defaultDownSql, map[string]*bintree{}},
	"000011_add_retention_plugin_insecure_default.up.sql":   &bintree{_000011_add_retention_plugin_insecure_defaultUpSql, map[string]*bintree{}},
	"000012_drop_unique_name_constraint.down.sql":           &bintree{_000012_drop_unique_name_constraintDownSql, map[string]*bintree{}},
	"000012_drop_unique_name_constraint.up.sql":             &bintree{_000012_drop_unique_name_constraintUpSql, map[string]*bintree{}},
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
