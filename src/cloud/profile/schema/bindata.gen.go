// Code generated for package schema by go-bindata DO NOT EDIT. (@generated)
// sources:
// 000001_create_org_user_tables.down.sql
// 000001_create_org_user_tables.up.sql
// 000002_add_unique_constraint_email.down.sql
// 000002_add_unique_constraint_email.up.sql
// 000003_add_profile_picture.down.sql
// 000003_add_profile_picture.up.sql
// 000004_add_updated_created_at.down.sql
// 000004_add_updated_created_at.up.sql
// 000005_create_user_settings_table.down.sql
// 000005_create_user_settings_table.up.sql
// 000006_add_approved_column.down.sql
// 000006_add_approved_column.up.sql
// 000007_add_enable_approvals_column.down.sql
// 000007_add_enable_approvals_column.up.sql
// 000008_insert_default_user_value.up.sql
// 000009_org_add_updated_created_at.down.sql
// 000009_org_add_updated_created_at.up.sql
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

var __000001_create_org_user_tablesDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\x08\x71\x74\xf2\x71\x55\xf0\x74\x53\x70\x8d\xf0\x0c\x0e\x09\x56\x28\x2d\x4e\x2d\x2a\xb6\xe6\xc2\x2a\x97\x5f\x94\x5e\x6c\xcd\x05\x08\x00\x00\xff\xff\x93\xee\xc5\x1a\x37\x00\x00\x00")

func _000001_create_org_user_tablesDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000001_create_org_user_tablesDownSql,
		"000001_create_org_user_tables.down.sql",
	)
}

func _000001_create_org_user_tablesDownSql() (*asset, error) {
	bytes, err := _000001_create_org_user_tablesDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000001_create_org_user_tables.down.sql", size: 55, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000001_create_org_user_tablesUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x9c\x90\x4f\x4b\xc3\x30\x18\xc6\xef\xf9\x14\x0f\x3b\x35\xa0\x30\x41\x4f\x9e\xea\xf6\x56\x82\x33\xd3\x34\x81\xed\x54\x82\x8d\x33\xb0\xb6\x92\xac\xfb\xfc\xf2\x6a\xf5\xb0\xb1\x8b\xc7\x3c\x7f\xc2\xef\x79\x17\x86\x4a\x4b\xa0\x8d\x25\x5d\xab\xb5\x86\xaa\xa0\xd7\x16\xb4\x51\xb5\xad\x31\x1b\xc7\xd8\x5e\x0f\x39\x7f\xce\xee\x85\x98\xc2\xb6\x7c\x58\x11\x86\xb4\xcb\x28\x04\x10\x5b\x38\xa7\x96\x70\x5a\xbd\x3a\xc2\x92\xaa\xd2\xad\x2c\xb8\xd9\xec\x42\x1f\x92\x3f\x84\xe6\x78\x5b\xc8\x2b\x01\x6e\x35\xbd\xef\x02\x8e\x3e\xbd\x7d\xf8\x54\xdc\xcd\xe5\xd4\x64\xbb\x1d\x3a\x1f\xfb\xcb\x09\x01\xbc\x18\xf5\x5c\x9a\x2d\x9e\x68\x5b\xc4\x56\x0a\x79\x0a\x36\xe6\x90\xfe\x49\x36\x15\xf8\xc9\xbf\x9c\x62\xb0\xfe\x1e\x53\x3e\x9c\x01\xb2\xb3\xf7\x17\x8c\xd0\xf9\xb8\xff\x13\x6f\xe6\xac\x9e\x0f\xe1\x64\xb5\x36\xa4\x1e\x35\x4b\x28\x7e\x80\x24\x0c\x55\x64\x48\x2f\xa8\xfe\xbe\xf9\xef\xe8\xaf\x00\x00\x00\xff\xff\xb4\x85\x91\x1a\xba\x01\x00\x00")

func _000001_create_org_user_tablesUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000001_create_org_user_tablesUpSql,
		"000001_create_org_user_tables.up.sql",
	)
}

func _000001_create_org_user_tablesUpSql() (*asset, error) {
	bytes, err := _000001_create_org_user_tablesUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000001_create_org_user_tables.up.sql", size: 442, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000002_add_unique_constraint_emailDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\x2d\x4e\x2d\x2a\xe6\x72\x09\xf2\x0f\x50\x70\xf6\xf7\x0b\x0e\x09\x72\xf4\xf4\x0b\x51\x48\xcd\x4d\xcc\xcc\x89\x2f\xcd\xcb\x2c\x2c\x4d\xb5\xe6\x02\x04\x00\x00\xff\xff\x1a\x07\x06\xa9\x30\x00\x00\x00")

func _000002_add_unique_constraint_emailDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000002_add_unique_constraint_emailDownSql,
		"000002_add_unique_constraint_email.down.sql",
	)
}

func _000002_add_unique_constraint_emailDownSql() (*asset, error) {
	bytes, err := _000002_add_unique_constraint_emailDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000002_add_unique_constraint_email.down.sql", size: 48, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000002_add_unique_constraint_emailUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\x2d\x4e\x2d\x2a\xe6\x72\x74\x71\x51\x70\xf6\xf7\x0b\x0e\x09\x72\xf4\xf4\x0b\x51\x48\xcd\x4d\xcc\xcc\x89\x2f\xcd\xcb\x2c\x2c\x4d\x55\x08\xf5\xf3\x0c\x0c\x75\x55\xd0\x00\x0b\x6a\x5a\x73\x01\x02\x00\x00\xff\xff\x0b\x8e\x6b\xf0\x3e\x00\x00\x00")

func _000002_add_unique_constraint_emailUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000002_add_unique_constraint_emailUpSql,
		"000002_add_unique_constraint_email.up.sql",
	)
}

func _000002_add_unique_constraint_emailUpSql() (*asset, error) {
	bytes, err := _000002_add_unique_constraint_emailUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000002_add_unique_constraint_email.up.sql", size: 62, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000003_add_profile_pictureDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\x2d\x4e\x2d\x2a\xe6\x72\x09\xf2\x0f\x50\x70\xf6\xf7\x09\xf5\xf5\x53\x28\x28\xca\x4f\xcb\xcc\x49\x8d\x2f\xc8\x4c\x2e\x29\x2d\x4a\xb5\xe6\x02\x04\x00\x00\xff\xff\x90\xf5\xcb\x8e\x2f\x00\x00\x00")

func _000003_add_profile_pictureDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000003_add_profile_pictureDownSql,
		"000003_add_profile_picture.down.sql",
	)
}

func _000003_add_profile_pictureDownSql() (*asset, error) {
	bytes, err := _000003_add_profile_pictureDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000003_add_profile_picture.down.sql", size: 47, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000003_add_profile_pictureUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\x2d\x4e\x2d\x2a\xe6\x72\x74\x71\x51\x70\xf6\xf7\x09\xf5\xf5\x53\x50\x28\x28\xca\x4f\xcb\xcc\x49\x8d\x2f\xc8\x4c\x2e\x29\x2d\x4a\x55\x08\x73\x0c\x72\xf6\x70\x0c\xd2\x30\x34\x30\xd0\xb4\xe6\x02\x04\x00\x00\xff\xff\x5c\xe5\xa7\x74\x3c\x00\x00\x00")

func _000003_add_profile_pictureUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000003_add_profile_pictureUpSql,
		"000003_add_profile_picture.up.sql",
	)
}

func _000003_add_profile_pictureUpSql() (*asset, error) {
	bytes, err := _000003_add_profile_pictureUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000003_add_profile_picture.up.sql", size: 60, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000004_add_updated_created_atDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\x2d\x4e\x2d\x2a\xe6\x72\x09\xf2\x0f\x50\x70\xf6\xf7\x09\xf5\xf5\x53\x48\x2e\x4a\x4d\x2c\x49\x4d\x89\x4f\x2c\xb1\xe6\xc2\xaf\xb2\xb4\x20\x05\xae\x12\x22\x11\x12\xe4\xe9\xee\xee\x1a\xa4\xe0\xe9\xa6\xe0\x1a\xe1\x19\x1c\x12\x0c\x55\x13\x0f\xd6\x1c\x8f\xd0\xa0\xe0\xef\x07\x31\xd0\x9a\x0b\x10\x00\x00\xff\xff\x7e\xc4\xa3\x01\x8e\x00\x00\x00")

func _000004_add_updated_created_atDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000004_add_updated_created_atDownSql,
		"000004_add_updated_created_at.down.sql",
	)
}

func _000004_add_updated_created_atDownSql() (*asset, error) {
	bytes, err := _000004_add_updated_created_atDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000004_add_updated_created_at.down.sql", size: 142, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000004_add_updated_created_atUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\xb4\x90\x41\xeb\x9b\x30\x18\xc6\xef\xf9\x14\xcf\x41\x68\x0b\x73\x5f\xa0\xec\x90\xea\x6b\x27\xd8\x44\x62\x42\x77\x2b\xa1\x4d\xad\xe0\xb4\xd3\xb8\x6e\xdf\x7e\x68\x91\x0a\xeb\x65\x8c\xbf\x17\xe1\xcd\x9b\xe7\xf7\xfc\x12\x86\x48\x9b\xca\x57\xb6\xae\x7f\xa3\x77\x1e\x4d\x8b\x8b\xbb\xda\xa1\xf6\x9f\xd0\xb7\xf0\x37\xeb\xe1\x7e\x55\xbd\xaf\x9a\x12\x5d\xfb\xe8\x71\xb3\x3f\x1d\x2c\x84\xc9\x32\x9c\x3b\x67\xbd\xbb\x9c\xac\xc7\xb9\xad\x87\xef\x0d\xe3\x99\x26\x05\xcd\x77\x19\x61\xe8\x5d\xd7\x83\xc7\x31\x22\x99\x99\x83\x58\xae\xeb\xf4\x40\x85\xe6\x87\x7c\xcb\xc2\x10\x85\xf3\xf0\x37\x37\xa3\xe1\x5b\x08\x79\x5c\x6f\x70\x6d\x3b\x5c\x07\x3f\x74\x6e\x82\xbf\x8b\x9f\x26\x7f\x03\x0a\xd2\x88\x29\xe1\x26\xd3\x68\xda\xc7\x7a\x33\x81\xfe\x47\x76\xb8\x5f\xfe\x45\x76\xb1\xfe\x31\xb2\x0b\xc0\x1b\x59\x16\x29\xe2\x9a\x20\x15\x14\xe5\x19\x8f\x08\x89\x11\x91\x4e\xe5\x7c\xf3\xf4\x0a\x58\x6f\x18\xa0\x48\x1b\x25\x0a\x68\x95\xee\xf7\xa4\xc0\x0b\x04\x01\x03\x76\xb4\x4f\x05\xc3\xf4\x09\x3a\x7e\x5e\x70\xbf\xcc\xb4\xe7\xe9\x33\x61\x5c\x1a\x27\x24\xe2\xf1\x17\x04\xa8\x6d\x53\x0e\xb6\x74\x58\xdd\xeb\x7b\xd9\xff\xa8\x57\xaf\x7e\x33\x6d\xee\x34\x7a\x2e\x9a\xb1\x1d\x25\x52\x11\x4c\x1e\x4f\x36\xe2\xf9\x12\x2c\x91\x0a\xc4\xa3\xaf\x50\xf2\x08\xfa\x46\x91\xd1\x84\x5c\xc9\x88\x62\xa3\xe8\x9d\xe1\x96\xfd\x09\x00\x00\xff\xff\xc3\xab\xd4\x78\xed\x02\x00\x00")

func _000004_add_updated_created_atUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000004_add_updated_created_atUpSql,
		"000004_add_updated_created_at.up.sql",
	)
}

func _000004_add_updated_created_atUpSql() (*asset, error) {
	bytes, err := _000004_add_updated_created_atUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000004_add_updated_created_at.up.sql", size: 749, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000005_create_user_settings_tableDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\x08\x71\x74\xf2\x71\x55\xf0\x74\x53\x70\x8d\xf0\x0c\x0e\x09\x56\x28\x2d\x4e\x2d\x8a\x2f\x4e\x2d\x29\xc9\xcc\x4b\x2f\xb6\xe6\x02\x04\x00\x00\xff\xff\xe4\x19\x53\xb1\x24\x00\x00\x00")

func _000005_create_user_settings_tableDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000005_create_user_settings_tableDownSql,
		"000005_create_user_settings_table.down.sql",
	)
}

func _000005_create_user_settings_tableDownSql() (*asset, error) {
	bytes, err := _000005_create_user_settings_tableDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000005_create_user_settings_table.down.sql", size: 36, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000005_create_user_settings_tableUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x0e\x72\x75\x0c\x71\x55\x08\x71\x74\xf2\x71\x55\x28\x2d\x4e\x2d\x8a\x2f\x4e\x2d\x29\xc9\xcc\x4b\x2f\x56\xd0\xe0\x52\x80\x88\x64\xa6\x28\x84\x86\x7a\xba\xe8\x70\x29\x28\x64\xa7\x56\x2a\x94\x25\x16\x25\x67\x24\x16\x69\x18\x1a\x18\x99\x68\x82\x04\xcb\x12\x73\x4a\x53\xd1\x85\xb9\x14\x14\x02\x82\x3c\x7d\x1d\x83\x22\x15\xbc\x5d\x23\x35\xa0\x06\xe9\x80\x4c\x00\x6b\x0a\xf5\xf3\x0c\x0c\x75\x55\xc0\x94\x70\xf3\x0f\x72\xf5\x74\xf7\x03\xe9\x82\xcb\x6a\x2a\x04\xb9\xba\xb9\x06\xb9\xfa\x39\xbb\x06\x83\xdd\x54\xac\x91\x99\xa2\xc9\xa5\x69\xcd\x05\x08\x00\x00\xff\xff\x98\x95\xd2\x01\xc0\x00\x00\x00")

func _000005_create_user_settings_tableUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000005_create_user_settings_tableUpSql,
		"000005_create_user_settings_table.up.sql",
	)
}

func _000005_create_user_settings_tableUpSql() (*asset, error) {
	bytes, err := _000005_create_user_settings_tableUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000005_create_user_settings_table.up.sql", size: 192, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000006_add_approved_columnDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\x2d\x4e\x2d\x2a\xe6\x72\x09\xf2\x0f\x50\x70\xf6\xf7\x09\xf5\xf5\x53\xc8\x2c\x8e\x4f\x2c\x28\x28\xca\x2f\x4b\x4d\xb1\xe6\x02\x04\x00\x00\xff\xff\x6e\xb4\x3a\x00\x2b\x00\x00\x00")

func _000006_add_approved_columnDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000006_add_approved_columnDownSql,
		"000006_add_approved_column.down.sql",
	)
}

func _000006_add_approved_columnDownSql() (*asset, error) {
	bytes, err := _000006_add_approved_columnDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000006_add_approved_column.down.sql", size: 43, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000006_add_approved_columnUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\x2d\x4e\x2d\x2a\xe6\x72\x74\x71\x51\x70\xf6\xf7\x09\xf5\xf5\x53\x50\xc8\x2c\x8e\x4f\x2c\x28\x28\xca\x2f\x4b\x4d\x51\x70\xf2\xf7\xf7\x71\x75\xf4\x53\x70\x71\x75\x73\x0c\xf5\x09\x51\x48\x4b\xcc\x29\x4e\xb5\xe6\x02\x04\x00\x00\xff\xff\xdf\xce\xd8\x56\x41\x00\x00\x00")

func _000006_add_approved_columnUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000006_add_approved_columnUpSql,
		"000006_add_approved_column.up.sql",
	)
}

func _000006_add_approved_columnUpSql() (*asset, error) {
	bytes, err := _000006_add_approved_columnUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000006_add_approved_column.up.sql", size: 65, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000007_add_enable_approvals_columnDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\xc8\x2f\x4a\x2f\xe6\x72\x09\xf2\x0f\x50\x70\xf6\xf7\x09\xf5\xf5\x53\x48\xcd\x4b\x4c\xca\x49\x8d\x4f\x2c\x28\x28\xca\x2f\x4b\xcc\x29\xb6\xe6\x02\x04\x00\x00\xff\xff\x21\x15\x36\x2a\x2f\x00\x00\x00")

func _000007_add_enable_approvals_columnDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000007_add_enable_approvals_columnDownSql,
		"000007_add_enable_approvals_column.down.sql",
	)
}

func _000007_add_enable_approvals_columnDownSql() (*asset, error) {
	bytes, err := _000007_add_enable_approvals_columnDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000007_add_enable_approvals_column.down.sql", size: 47, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000007_add_enable_approvals_columnUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\xc8\x2f\x4a\x2f\xe6\x72\x74\x71\x51\x70\xf6\xf7\x09\xf5\xf5\x53\x48\xcd\x4b\x4c\xca\x49\x8d\x4f\x2c\x28\x28\xca\x2f\x4b\xcc\x29\x56\x70\xf2\xf7\xf7\x71\x75\xf4\x53\x70\x71\x75\x73\x0c\xf5\x09\x51\x48\x4b\xcc\x29\x4e\xb5\xe6\x02\x04\x00\x00\xff\xff\x9a\x71\x64\xbc\x44\x00\x00\x00")

func _000007_add_enable_approvals_columnUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000007_add_enable_approvals_columnUpSql,
		"000007_add_enable_approvals_column.up.sql",
	)
}

func _000007_add_enable_approvals_columnUpSql() (*asset, error) {
	bytes, err := _000007_add_enable_approvals_columnUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000007_add_enable_approvals_column.up.sql", size: 68, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000008_insert_default_user_valueUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x0a\x0d\x70\x71\x0c\x71\x55\x28\x2d\x4e\x2d\x2a\x56\x08\x76\x0d\x51\xc8\x2c\x8e\x4f\x2c\x28\x28\xca\x2f\x4b\x4d\x51\xb0\x55\x28\x29\x2a\x4d\xb5\xe6\x02\x04\x00\x00\xff\xff\x83\xb9\x30\x99\x25\x00\x00\x00")

func _000008_insert_default_user_valueUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000008_insert_default_user_valueUpSql,
		"000008_insert_default_user_value.up.sql",
	)
}

func _000008_insert_default_user_valueUpSql() (*asset, error) {
	bytes, err := _000008_insert_default_user_valueUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000008_insert_default_user_value.up.sql", size: 37, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000009_org_add_updated_created_atDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\xc8\x2f\x4a\x2f\xe6\x72\x09\xf2\x0f\x50\x70\xf6\xf7\x09\xf5\xf5\x53\x48\x2e\x4a\x4d\x2c\x49\x4d\x89\x4f\x2c\xb1\xe6\xc2\xab\xb0\xb4\x20\x05\xae\x10\x22\x11\x12\xe4\xe9\xee\xee\x1a\xa4\xe0\xe9\xa6\xe0\x1a\xe1\x19\x1c\x12\x0c\x55\x13\x0f\xd2\x1b\x8f\x50\xaf\xe0\xef\x07\x36\xce\x9a\x0b\x10\x00\x00\xff\xff\xb8\x59\x22\x6c\x8a\x00\x00\x00")

func _000009_org_add_updated_created_atDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000009_org_add_updated_created_atDownSql,
		"000009_org_add_updated_created_at.down.sql",
	)
}

func _000009_org_add_updated_created_atDownSql() (*asset, error) {
	bytes, err := _000009_org_add_updated_created_atDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000009_org_add_updated_created_at.down.sql", size: 138, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000009_org_add_updated_created_atUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\xb4\x90\x51\xab\x9b\x30\x18\x86\xef\xf3\x2b\xde\x0b\xe1\xb4\x30\xf7\x07\x0e\xbb\x48\xf5\xb3\x13\x6c\x22\x31\xa1\xbb\x3b\x84\xd3\xd4\x0a\xce\x74\x1a\xd7\xed\xdf\x0f\xed\xa4\xc2\x7a\x33\xc6\xbc\x11\xbe\x7c\x79\x9f\xf7\x49\x1c\x23\xef\x9a\xd0\xd8\xb6\xfd\x89\xc1\x05\x74\x1e\x27\x77\xb6\x63\x1b\x3e\x60\xf0\x08\x17\x1b\xe0\x7e\x34\x43\x68\xba\x1a\xbd\xbf\x0d\xb8\xd8\xef\x0e\x16\xc2\x14\x05\xde\x7b\x67\x83\x3b\xbd\xd9\x80\x77\xdf\x8e\x5f\x3b\xc6\x0b\x4d\x0a\x9a\xef\x0a\x82\xef\xeb\x01\x3c\x4d\x91\xc8\xc2\x1c\xc4\x7a\x5b\xe7\x07\xaa\x34\x3f\x94\xaf\x2c\x8e\x51\xb9\x80\x70\x71\x0b\x19\xc1\x43\xc8\xe3\x66\x8b\xb3\xef\x71\x1e\xc3\xd8\xbb\x99\xfd\x24\x7d\x1e\xfc\x99\x5f\x91\x46\x4a\x19\x37\x85\x46\xe7\x6f\x9b\xed\xcc\xf9\x17\xd5\xf1\x7a\xfa\x0b\xd5\xd5\xf6\x7f\x51\x5d\xe5\x3f\x51\x65\x89\x22\xae\x09\x52\x41\x51\x59\xf0\x84\x90\x19\x91\xe8\x5c\x2e\x37\xdf\x1e\x01\x9b\x2d\x03\x14\x69\xa3\x44\x05\xad\xf2\xfd\x9e\x14\x78\x85\x28\x62\xc0\x8e\xf6\xb9\x60\x98\x3f\x41\xc7\x8f\x2b\xee\xa7\x85\x76\x3f\xbd\x27\x4c\x4b\xd3\x84\x44\x3a\xfd\xa2\x08\xad\xed\xea\xd1\xd6\x0e\x2f\xd7\xf6\x5a\x0f\xdf\xda\x97\x47\xbf\x85\xf6\xbb\xd3\xa4\xb9\x2a\xc6\x76\x94\x49\x45\x30\x65\x3a\xcb\x88\xf9\x1d\x58\x26\x15\x88\x27\x9f\xa1\xe4\x11\xf4\x85\x12\xa3\x09\xa5\x92\x09\xa5\x46\xd1\x33\xbf\x57\xf6\x2b\x00\x00\xff\xff\x3e\x0e\xdd\xff\xe7\x02\x00\x00")

func _000009_org_add_updated_created_atUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000009_org_add_updated_created_atUpSql,
		"000009_org_add_updated_created_at.up.sql",
	)
}

func _000009_org_add_updated_created_atUpSql() (*asset, error) {
	bytes, err := _000009_org_add_updated_created_atUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000009_org_add_updated_created_at.up.sql", size: 743, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
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
	"000001_create_org_user_tables.down.sql":      _000001_create_org_user_tablesDownSql,
	"000001_create_org_user_tables.up.sql":        _000001_create_org_user_tablesUpSql,
	"000002_add_unique_constraint_email.down.sql": _000002_add_unique_constraint_emailDownSql,
	"000002_add_unique_constraint_email.up.sql":   _000002_add_unique_constraint_emailUpSql,
	"000003_add_profile_picture.down.sql":         _000003_add_profile_pictureDownSql,
	"000003_add_profile_picture.up.sql":           _000003_add_profile_pictureUpSql,
	"000004_add_updated_created_at.down.sql":      _000004_add_updated_created_atDownSql,
	"000004_add_updated_created_at.up.sql":        _000004_add_updated_created_atUpSql,
	"000005_create_user_settings_table.down.sql":  _000005_create_user_settings_tableDownSql,
	"000005_create_user_settings_table.up.sql":    _000005_create_user_settings_tableUpSql,
	"000006_add_approved_column.down.sql":         _000006_add_approved_columnDownSql,
	"000006_add_approved_column.up.sql":           _000006_add_approved_columnUpSql,
	"000007_add_enable_approvals_column.down.sql": _000007_add_enable_approvals_columnDownSql,
	"000007_add_enable_approvals_column.up.sql":   _000007_add_enable_approvals_columnUpSql,
	"000008_insert_default_user_value.up.sql":     _000008_insert_default_user_valueUpSql,
	"000009_org_add_updated_created_at.down.sql":  _000009_org_add_updated_created_atDownSql,
	"000009_org_add_updated_created_at.up.sql":    _000009_org_add_updated_created_atUpSql,
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
	"000001_create_org_user_tables.down.sql":      &bintree{_000001_create_org_user_tablesDownSql, map[string]*bintree{}},
	"000001_create_org_user_tables.up.sql":        &bintree{_000001_create_org_user_tablesUpSql, map[string]*bintree{}},
	"000002_add_unique_constraint_email.down.sql": &bintree{_000002_add_unique_constraint_emailDownSql, map[string]*bintree{}},
	"000002_add_unique_constraint_email.up.sql":   &bintree{_000002_add_unique_constraint_emailUpSql, map[string]*bintree{}},
	"000003_add_profile_picture.down.sql":         &bintree{_000003_add_profile_pictureDownSql, map[string]*bintree{}},
	"000003_add_profile_picture.up.sql":           &bintree{_000003_add_profile_pictureUpSql, map[string]*bintree{}},
	"000004_add_updated_created_at.down.sql":      &bintree{_000004_add_updated_created_atDownSql, map[string]*bintree{}},
	"000004_add_updated_created_at.up.sql":        &bintree{_000004_add_updated_created_atUpSql, map[string]*bintree{}},
	"000005_create_user_settings_table.down.sql":  &bintree{_000005_create_user_settings_tableDownSql, map[string]*bintree{}},
	"000005_create_user_settings_table.up.sql":    &bintree{_000005_create_user_settings_tableUpSql, map[string]*bintree{}},
	"000006_add_approved_column.down.sql":         &bintree{_000006_add_approved_columnDownSql, map[string]*bintree{}},
	"000006_add_approved_column.up.sql":           &bintree{_000006_add_approved_columnUpSql, map[string]*bintree{}},
	"000007_add_enable_approvals_column.down.sql": &bintree{_000007_add_enable_approvals_columnDownSql, map[string]*bintree{}},
	"000007_add_enable_approvals_column.up.sql":   &bintree{_000007_add_enable_approvals_columnUpSql, map[string]*bintree{}},
	"000008_insert_default_user_value.up.sql":     &bintree{_000008_insert_default_user_valueUpSql, map[string]*bintree{}},
	"000009_org_add_updated_created_at.down.sql":  &bintree{_000009_org_add_updated_created_atDownSql, map[string]*bintree{}},
	"000009_org_add_updated_created_at.up.sql":    &bintree{_000009_org_add_updated_created_atUpSql, map[string]*bintree{}},
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
