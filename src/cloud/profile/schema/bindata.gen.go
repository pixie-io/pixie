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
// 000010_user_add_identity_provider.down.sql
// 000010_user_add_identity_provider.up.sql
// 000011_user_set_identity_provider_google_oauth2.up.sql
// 000012_user_add_auth_provider_id.down.sql
// 000012_user_add_auth_provider_id.up.sql
// 000013_lengthen_profile_email.down.sql
// 000013_lengthen_profile_email.up.sql
// 000014_lengthen_auth_provider_id.down.sql
// 000014_lengthen_auth_provider_id.up.sql
// 000015_create_user_attributes_table.down.sql
// 000015_create_user_attributes_table.up.sql
// 000016_move_user_settings.down.sql
// 000016_move_user_settings.up.sql
// 000017_reformat_user_settings.down.sql
// 000017_reformat_user_settings.up.sql
// 000018_add_unique_constraint_auth_provider_id.down.sql
// 000018_add_unique_constraint_auth_provider_id.up.sql
// 000019_create_org_ide_configs_tables.down.sql
// 000019_create_org_ide_configs_tables.up.sql
// 000020_empty_domain_name_col.down.sql
// 000020_empty_domain_name_col.up.sql
// 000021_org_add_invite_signing_key.down.sql
// 000021_org_add_invite_signing_key.up.sql
// 000022_users_drop_usernames.down.sql
// 000022_users_drop_usernames.up.sql
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

var __000010_user_add_identity_providerDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\x2d\x4e\x2d\x2a\xe6\x72\x09\xf2\x0f\x50\x70\xf6\xf7\x09\xf5\xf5\x53\xc8\x4c\x49\xcd\x2b\xc9\x2c\xa9\x8c\x2f\x28\xca\x2f\xcb\x4c\x49\x2d\xb2\xe6\x02\x04\x00\x00\xff\xff\xde\xb8\x87\x6f\x31\x00\x00\x00")

func _000010_user_add_identity_providerDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000010_user_add_identity_providerDownSql,
		"000010_user_add_identity_provider.down.sql",
	)
}

func _000010_user_add_identity_providerDownSql() (*asset, error) {
	bytes, err := _000010_user_add_identity_providerDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000010_user_add_identity_provider.down.sql", size: 49, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000010_user_add_identity_providerUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\x2d\x4e\x2d\x2a\xe6\x72\x74\x71\x51\x70\xf6\xf7\x09\xf5\xf5\x53\xc8\x4c\x49\xcd\x2b\xc9\x2c\xa9\x8c\x2f\x28\xca\x2f\xcb\x4c\x49\x2d\x52\x28\x4b\x2c\x4a\xce\x48\x2c\xd2\x30\x35\xd0\xb4\xe6\x02\x04\x00\x00\xff\xff\x02\x23\x6c\xc8\x3c\x00\x00\x00")

func _000010_user_add_identity_providerUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000010_user_add_identity_providerUpSql,
		"000010_user_add_identity_provider.up.sql",
	)
}

func _000010_user_add_identity_providerUpSql() (*asset, error) {
	bytes, err := _000010_user_add_identity_providerUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000010_user_add_identity_provider.up.sql", size: 60, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000011_user_set_identity_provider_google_oauth2UpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x5c\xd0\xb1\x6e\xf3\x30\x0c\x04\xe0\xdd\x4f\x71\x9b\xff\x7f\x70\x5a\x74\x2d\x3a\x04\x6d\xe6\x16\x70\x3a\x17\x4c\x44\x5b\x44\x25\xd1\x90\x28\xa7\x7e\xfb\xc2\x71\x90\xa1\x2b\x09\x1e\xbe\x63\xd7\xa1\x67\x83\xe3\x81\x6a\x30\xcc\x14\x2a\xc3\x14\xed\xa8\x3a\x06\xee\x94\xaa\xf9\xa7\x16\x27\x3e\x53\x2d\x0c\xf3\x52\x20\x05\xe6\x19\x91\x24\x35\x5d\x87\x52\xa7\x49\xb3\xb1\x83\x38\x4e\x26\xb6\x60\xca\x3a\x8b\xe3\x8c\x21\x6b\xc4\xbe\x9a\x7f\xdc\xe1\xbd\xef\xf1\x1a\xb4\x3a\xd4\xc2\xb9\xe0\x22\xe6\xaf\xc3\x75\x8f\x7f\x7e\x71\x99\x1e\xbe\x33\x99\x96\xff\x6b\xee\x45\x42\x80\xa7\xf9\x0a\x8a\x94\x2a\x85\xb0\x20\x73\xae\x69\x73\x9c\x35\x46\x4a\x6e\x0b\x6a\xb7\xcb\x16\x32\xac\xba\x05\x8e\xa7\xa0\x0b\xbb\x35\xea\xc4\x83\xe6\x9b\x3e\xca\x98\xc9\x44\x13\xf8\x47\x8a\xb1\xdb\x35\x9f\x1f\x6f\xfb\xe3\xe1\xc6\xea\x0f\xc7\x7b\x91\xaf\x7b\x91\x97\xbf\x2f\x79\x6e\x7e\x03\x00\x00\xff\xff\xb8\x08\xfb\x21\x3c\x01\x00\x00")

func _000011_user_set_identity_provider_google_oauth2UpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000011_user_set_identity_provider_google_oauth2UpSql,
		"000011_user_set_identity_provider_google_oauth2.up.sql",
	)
}

func _000011_user_set_identity_provider_google_oauth2UpSql() (*asset, error) {
	bytes, err := _000011_user_set_identity_provider_google_oauth2UpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000011_user_set_identity_provider_google_oauth2.up.sql", size: 316, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000012_user_add_auth_provider_idDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\x2d\x4e\x2d\x2a\xe6\x72\x09\xf2\x0f\x50\x70\xf6\xf7\x09\xf5\xf5\x53\x48\x2c\x2d\xc9\x88\x2f\x28\xca\x2f\xcb\x4c\x49\x2d\x8a\xcf\x4c\xb1\xe6\x02\x04\x00\x00\xff\xff\x24\x7e\xb4\x56\x30\x00\x00\x00")

func _000012_user_add_auth_provider_idDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000012_user_add_auth_provider_idDownSql,
		"000012_user_add_auth_provider_id.down.sql",
	)
}

func _000012_user_add_auth_provider_idDownSql() (*asset, error) {
	bytes, err := _000012_user_add_auth_provider_idDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000012_user_add_auth_provider_id.down.sql", size: 48, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000012_user_add_auth_provider_idUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x64\x8f\x41\x4b\xc4\x40\x0c\x85\xef\xfd\x15\xef\xb6\x0a\x76\xf1\xe2\xc9\x53\x75\x2a\x2c\x54\x05\xe9\x9e\x97\xb8\x9d\x3a\x61\xbb\xd3\x92\x64\x5a\xfc\xf7\x32\x2d\x1e\xc4\x6b\xde\xcb\x97\x2f\x65\x09\x4a\x16\x4e\x93\x8c\x33\x77\x5e\x4e\xdc\x81\x15\x16\x3c\x92\x7a\xc1\xc1\xc1\x02\x19\x28\xfe\xed\xe5\x54\xd1\x8f\x92\x93\x83\xdb\xa3\xcd\xad\x85\xbe\xc1\x3d\x68\xdb\x55\xfe\x8a\x0a\x8e\x58\xd8\x02\x08\x1d\xf7\xbd\x17\x1f\xad\x28\x4b\xf8\x2b\xf1\x80\xcf\x64\xeb\x2d\xa5\xab\xc7\x2f\xfb\x0e\x8b\xc7\x25\x8e\x0b\x6c\xc4\xc0\xf1\xb2\x29\x64\x1c\xd2\xb4\xd1\x2c\xb0\x62\x22\x31\x3e\xa7\x81\x56\x1d\xd9\x17\x55\xd3\xd6\x1f\x68\xab\xa7\xa6\x5e\x27\x5a\x54\xce\xe1\xf9\xbd\x39\xbe\xbe\xfd\xff\x73\x26\x39\x07\x92\x9b\x87\xfb\x5b\xb8\xfa\xa5\x3a\x36\x2d\x76\xbb\xc7\xe2\x27\x00\x00\xff\xff\x07\x62\x1a\xbb\x14\x01\x00\x00")

func _000012_user_add_auth_provider_idUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000012_user_add_auth_provider_idUpSql,
		"000012_user_add_auth_provider_id.up.sql",
	)
}

func _000012_user_add_auth_provider_idUpSql() (*asset, error) {
	bytes, err := _000012_user_add_auth_provider_idUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000012_user_add_auth_provider_id.up.sql", size: 276, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000013_lengthen_profile_emailDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\x2d\x4e\x2d\x2a\x56\x80\x88\x38\xfb\xfb\x84\xfa\xfa\x29\x14\x14\xe5\xa7\x65\xe6\xa4\xc6\x17\x64\x26\x97\x94\x16\xa5\x2a\x84\x44\x06\xb8\x2a\x84\x39\x06\x39\x7b\x38\x06\x29\x68\x18\x1a\x18\x68\x5a\x73\x11\x30\x22\x35\x37\x31\x33\x07\xab\x46\x40\x00\x00\x00\xff\xff\xad\x99\x4b\xee\x7c\x00\x00\x00")

func _000013_lengthen_profile_emailDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000013_lengthen_profile_emailDownSql,
		"000013_lengthen_profile_email.down.sql",
	)
}

func _000013_lengthen_profile_emailDownSql() (*asset, error) {
	bytes, err := _000013_lengthen_profile_emailDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000013_lengthen_profile_email.down.sql", size: 124, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000013_lengthen_profile_emailUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\x2d\x4e\x2d\x2a\x56\x80\x88\x38\xfb\xfb\x84\xfa\xfa\x29\x14\x14\xe5\xa7\x65\xe6\xa4\xc6\x17\x64\x26\x97\x94\x16\xa5\x2a\x84\x44\x06\xb8\x2a\x84\x39\x06\x39\x7b\x38\x06\x29\x68\x18\x1a\x18\x99\x68\x5a\x73\x11\x30\x23\x35\x37\x31\x33\x07\xbb\x4e\x40\x00\x00\x00\xff\xff\x1e\x45\x3e\x94\x7e\x00\x00\x00")

func _000013_lengthen_profile_emailUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000013_lengthen_profile_emailUpSql,
		"000013_lengthen_profile_email.up.sql",
	)
}

func _000013_lengthen_profile_emailUpSql() (*asset, error) {
	bytes, err := _000013_lengthen_profile_emailUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000013_lengthen_profile_email.up.sql", size: 126, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000014_lengthen_auth_provider_idDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\x2d\x4e\x2d\x2a\x56\x80\x88\x38\xfb\xfb\x84\xfa\xfa\x29\x24\x96\x96\x64\xc4\x17\x14\xe5\x97\x65\xa6\xa4\x16\xc5\x67\xa6\x28\x84\x44\x06\xb8\x2a\x84\x39\x06\x39\x7b\x38\x06\x29\x68\x98\x1a\x68\x5a\x73\x01\x02\x00\x00\xff\xff\xf0\x10\x4e\x1e\x43\x00\x00\x00")

func _000014_lengthen_auth_provider_idDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000014_lengthen_auth_provider_idDownSql,
		"000014_lengthen_auth_provider_id.down.sql",
	)
}

func _000014_lengthen_auth_provider_idDownSql() (*asset, error) {
	bytes, err := _000014_lengthen_auth_provider_idDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000014_lengthen_auth_provider_id.down.sql", size: 67, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000014_lengthen_auth_provider_idUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\x2d\x4e\x2d\x2a\x56\x80\x88\x38\xfb\xfb\x84\xfa\xfa\x29\x24\x96\x96\x64\xc4\x17\x14\xe5\x97\x65\xa6\xa4\x16\xc5\x67\xa6\x28\x84\x44\x06\xb8\x2a\x84\x39\x06\x39\x7b\x38\x06\x29\x68\x18\x1a\x18\x99\x68\x5a\x73\x01\x02\x00\x00\xff\xff\x21\x91\xa2\x15\x45\x00\x00\x00")

func _000014_lengthen_auth_provider_idUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000014_lengthen_auth_provider_idUpSql,
		"000014_lengthen_auth_provider_id.up.sql",
	)
}

func _000014_lengthen_auth_provider_idUpSql() (*asset, error) {
	bytes, err := _000014_lengthen_auth_provider_idUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000014_lengthen_auth_provider_id.up.sql", size: 69, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000015_create_user_attributes_tableDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\x08\x71\x74\xf2\x71\x55\xf0\x74\x53\x70\x8d\xf0\x0c\x0e\x09\x56\x28\x2d\x4e\x2d\x8a\x4f\x2c\x29\x29\xca\x4c\x2a\x2d\x49\x2d\xb6\xe6\x02\x04\x00\x00\xff\xff\xfa\x14\xea\x74\x26\x00\x00\x00")

func _000015_create_user_attributes_tableDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000015_create_user_attributes_tableDownSql,
		"000015_create_user_attributes_table.down.sql",
	)
}

func _000015_create_user_attributes_tableDownSql() (*asset, error) {
	bytes, err := _000015_create_user_attributes_tableDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000015_create_user_attributes_table.down.sql", size: 38, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000015_create_user_attributes_tableUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x3c\xcd\xcd\x0a\x82\x40\x14\xc5\xf1\xfd\x7d\x8a\xb3\x54\xf0\x0d\x5a\x4d\x7a\x0d\xc9\x2c\x6e\xba\x70\x25\x8a\x37\x18\x10\x07\xe6\xe3\xfd\xc3\x8a\xb6\xff\xc3\x8f\x53\x0a\x9b\x9e\xd1\x9b\x73\xcb\x48\x41\xfd\x34\xc7\xe8\xed\x92\xa2\x06\x64\x84\x6f\xb3\x2b\x86\xa1\xa9\x0a\x02\xa2\x4b\x7e\x0a\xaa\x3b\x16\xe7\x36\x9d\x77\x54\x5c\x9b\xa1\xed\xf1\x9a\xb7\xa0\x05\x11\xf0\x90\xe6\x66\x64\xc4\x95\xc7\xec\xe7\xf3\xc3\xd6\x77\xe1\xe6\xd2\x1d\x1d\xff\x01\xc2\x35\x0b\x77\x25\x3f\x3f\x67\x21\xb3\x6b\x4e\xf9\x89\xde\x01\x00\x00\xff\xff\xb0\x47\xbc\x5c\x9b\x00\x00\x00")

func _000015_create_user_attributes_tableUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000015_create_user_attributes_tableUpSql,
		"000015_create_user_attributes_table.up.sql",
	)
}

func _000015_create_user_attributes_tableUpSql() (*asset, error) {
	bytes, err := _000015_create_user_attributes_tableUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000015_create_user_attributes_table.up.sql", size: 155, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000016_move_user_settingsDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\xf2\xf4\x0b\x76\x0d\x0a\x51\xf0\xf4\x0b\xf1\x57\x28\x2d\x4e\x2d\x8a\x2f\x4e\x2d\x29\xc9\xcc\x4b\x2f\x56\xd0\x00\x73\x33\x53\x74\x14\xb2\x53\x2b\x75\x14\xca\x12\x73\x4a\x53\x35\xb9\x82\x5d\x7d\x5c\x9d\x43\x14\xe0\x72\xea\x25\xf9\xa5\x20\x4d\xa9\x79\xea\x3a\x0a\x70\xb6\x95\x55\x59\x62\x51\x72\x46\x62\x91\x86\xa9\x81\xa6\x82\x5b\x90\xbf\x2f\x44\x47\x62\x49\x49\x51\x66\x52\x69\x49\x6a\xb1\x35\x17\x97\x8b\xab\x8f\x6b\x88\x2b\x0e\x59\x40\x00\x00\x00\xff\xff\x7e\x6c\xc4\x0d\x98\x00\x00\x00")

func _000016_move_user_settingsDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000016_move_user_settingsDownSql,
		"000016_move_user_settings.down.sql",
	)
}

func _000016_move_user_settingsDownSql() (*asset, error) {
	bytes, err := _000016_move_user_settingsDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000016_move_user_settings.down.sql", size: 152, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000016_move_user_settingsUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x6c\xce\xc1\xca\x82\x50\x10\x05\xe0\xfd\x3c\xc5\x59\xfe\xff\xc6\x07\x50\x5c\x44\x4e\x24\x98\x86\xde\x68\x29\x57\x1c\xe2\x82\x28\x38\x73\x7b\xfe\x40\xa2\x16\xb6\x1b\x86\x73\x3e\x4e\x59\x77\xdc\x3a\x94\xb5\x6b\x10\x55\xd6\xde\x9b\xad\x61\x88\x26\x8a\xbf\xed\x11\xc6\x7f\xea\xb8\xe2\xa3\x43\x18\x71\x6a\x9b\xcb\x16\xd4\x8c\xe8\x76\x2d\x0e\x8e\x77\xbd\x8e\x1d\x6c\x89\x6b\xaf\x22\x73\xae\xc9\xd3\x4f\x51\xd2\x74\x58\x96\x49\xfc\xfc\x25\x7a\x15\xb3\x30\x3f\x14\x5e\xa1\x04\xdc\xcf\xdc\xee\xb8\xe4\xbd\x02\x39\x3e\x77\x46\x54\x70\xc5\x8e\x7f\x60\x19\xbd\x02\x00\x00\xff\xff\x2c\x0a\xca\xc5\xd4\x00\x00\x00")

func _000016_move_user_settingsUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000016_move_user_settingsUpSql,
		"000016_move_user_settings.up.sql",
	)
}

func _000016_move_user_settingsUpSql() (*asset, error) {
	bytes, err := _000016_move_user_settingsUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000016_move_user_settings.up.sql", size: 212, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000017_reformat_user_settingsDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x6c\x8e\xb1\x8e\x82\x40\x14\x45\xfb\xf7\x15\xb7\x9c\x49\x28\x76\x37\xdb\x51\x21\x3c\xcc\x44\x05\x7c\x32\x05\x95\x21\x32\x51\xa2\xb1\x60\x80\xc4\xbf\x37\xa0\xb1\x50\xdb\x73\x72\x4f\x6e\x22\x79\x81\x32\x5a\xac\x19\x83\x77\xdd\xde\xbb\xbe\x6f\xaf\x47\x1f\x12\xc5\xc2\x51\xc9\xdf\x24\x14\xe1\x41\xda\x06\xd6\x9a\x24\x20\xe0\xec\x6e\x18\xeb\xee\x70\xaa\x3b\xf5\xfb\xf3\xf7\xaf\x27\x38\xd6\x97\xc1\xbd\x63\x02\x0a\x31\x9b\x48\x2a\xac\xb8\x52\xcf\x50\x30\x15\xe6\x91\xcd\xcc\xd6\x32\x3e\x45\x9a\x0b\x9b\x65\x36\xad\x5e\x56\x43\x38\x65\xe1\x2c\xe6\xdd\xfc\xc9\xab\xb6\xd1\xa4\x43\xba\x07\x00\x00\xff\xff\xd8\xde\x58\x07\xdb\x00\x00\x00")

func _000017_reformat_user_settingsDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000017_reformat_user_settingsDownSql,
		"000017_reformat_user_settings.down.sql",
	)
}

func _000017_reformat_user_settingsDownSql() (*asset, error) {
	bytes, err := _000017_reformat_user_settingsDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000017_reformat_user_settings.down.sql", size: 219, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000017_reformat_user_settingsUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x6c\x8e\xb1\x4e\xc3\x30\x10\x86\xf7\x7b\x8a\x7f\x4c\xa4\xbe\x41\x26\x93\x9c\x91\x45\x6a\x57\x17\x67\xe8\x54\x19\x62\x90\xa5\x28\x46\xd8\x1d\x78\x7b\x54\x02\x0c\xa8\xeb\xf7\xe9\xbe\xfb\x07\x71\x27\x78\xf5\x30\x32\xae\x25\x7e\x5c\x4a\xac\x35\x6d\x6f\xa5\x23\xea\x85\x95\xe7\x7b\x12\x0d\x61\x27\x69\xc1\x3c\x9b\xe1\x40\x40\xd8\xc2\xfa\x59\xd3\x4b\xb9\xe4\xf7\x9a\xaf\x15\xcf\x39\xaf\x31\x6c\x18\x58\xab\x79\xf4\x78\x0d\x6b\x89\x07\x22\xe0\x24\xe6\xa8\xe4\x8c\x27\x3e\x37\x3f\x99\xf6\x96\xd0\x4e\xd8\x3c\xda\x1b\xc7\x9f\x80\xb0\x66\x61\xdb\xf3\xf4\xfd\xb3\x34\x69\x69\xa9\xed\x88\x8c\x9d\x58\x3c\x8c\xf5\xee\xff\xbe\xdf\x63\x9a\x78\xe4\xde\x23\x2d\xd0\xe2\x8e\x7b\xa0\xa3\xaf\x00\x00\x00\xff\xff\x76\x36\x50\x0c\xf6\x00\x00\x00")

func _000017_reformat_user_settingsUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000017_reformat_user_settingsUpSql,
		"000017_reformat_user_settings.up.sql",
	)
}

func _000017_reformat_user_settingsUpSql() (*asset, error) {
	bytes, err := _000017_reformat_user_settingsUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000017_reformat_user_settings.up.sql", size: 246, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000018_add_unique_constraint_auth_provider_idDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\x2d\x4e\x2d\x2a\xe6\x72\x09\xf2\x0f\x50\x70\xf6\xf7\x0b\x0e\x09\x72\xf4\xf4\x0b\x51\x48\x2c\x2d\xc9\x88\x2f\x28\xca\x2f\xcb\x4c\x49\x2d\x8a\xcf\x4c\x89\x2f\xcd\xcb\x2c\x2c\x4d\xb5\xe6\x02\x04\x00\x00\xff\xff\x60\xa0\xa3\xdf\x3b\x00\x00\x00")

func _000018_add_unique_constraint_auth_provider_idDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000018_add_unique_constraint_auth_provider_idDownSql,
		"000018_add_unique_constraint_auth_provider_id.down.sql",
	)
}

func _000018_add_unique_constraint_auth_provider_idDownSql() (*asset, error) {
	bytes, err := _000018_add_unique_constraint_auth_provider_idDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000018_add_unique_constraint_auth_provider_id.down.sql", size: 59, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000018_add_unique_constraint_auth_provider_idUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\x2d\x4e\x2d\x2a\xe6\x72\x74\x71\x51\x70\xf6\xf7\x0b\x0e\x09\x72\xf4\xf4\x0b\x51\x48\x2c\x2d\xc9\x88\x2f\x28\xca\x2f\xcb\x4c\x49\x2d\x8a\xcf\x4c\x89\x2f\xcd\xcb\x2c\x2c\x4d\x55\x08\xf5\xf3\x0c\x0c\x75\x55\xd0\x40\x97\xd7\xb4\xe6\x02\x04\x00\x00\xff\xff\xd0\x9f\x74\x6b\x54\x00\x00\x00")

func _000018_add_unique_constraint_auth_provider_idUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000018_add_unique_constraint_auth_provider_idUpSql,
		"000018_add_unique_constraint_auth_provider_id.up.sql",
	)
}

func _000018_add_unique_constraint_auth_provider_idUpSql() (*asset, error) {
	bytes, err := _000018_add_unique_constraint_auth_provider_idUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000018_add_unique_constraint_auth_provider_id.up.sql", size: 84, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000019_create_org_ide_configs_tablesDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\x08\x71\x74\xf2\x71\x55\xf0\x74\x53\x70\x8d\xf0\x0c\x0e\x09\x56\xc8\x2f\x4a\x8f\xcf\x4c\x49\x8d\x4f\xce\xcf\x4b\xcb\x4c\x2f\xb6\xe6\x02\x04\x00\x00\xff\xff\xdf\x02\x52\xc7\x26\x00\x00\x00")

func _000019_create_org_ide_configs_tablesDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000019_create_org_ide_configs_tablesDownSql,
		"000019_create_org_ide_configs_tables.down.sql",
	)
}

func _000019_create_org_ide_configs_tablesDownSql() (*asset, error) {
	bytes, err := _000019_create_org_ide_configs_tablesDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000019_create_org_ide_configs_tables.down.sql", size: 38, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000019_create_org_ide_configs_tablesUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x6c\x8e\xbd\x0e\x82\x40\x10\x06\xfb\x7d\x8a\xaf\xbc\x4b\x28\xd4\xd8\x59\x9d\xb8\xe8\x45\x45\x5d\xef\x8c\x54\x84\x08\x22\x85\x60\xc4\xf7\x8f\x39\xff\x2a\xdb\x9d\x99\xcd\x17\x0b\x1b\xc7\x70\x66\xba\x62\x74\xf7\x3a\x6f\xca\x2a\x3f\x75\xed\xb9\xa9\x7b\x28\xc2\xe7\x06\xef\xed\x2c\x22\x20\xe0\xb6\xb8\x56\x38\x18\x89\x17\x46\xa0\x86\x83\xd1\x58\x07\x74\x2b\x1e\x17\x38\x3e\xba\x88\x08\xd8\x8a\x5d\x1b\xc9\xb0\xe4\x4c\xbd\x7f\x44\xbf\xf8\xa5\xfb\xd4\xee\x3c\xe3\x3f\x4c\x36\xc2\x76\x9e\x86\xfa\x6b\x68\x08\x27\x2c\x9c\xc6\xbc\x0f\xab\x7a\xd5\x94\x9a\xf4\x84\x9e\x01\x00\x00\xff\xff\xdb\x55\x31\xd8\xc3\x00\x00\x00")

func _000019_create_org_ide_configs_tablesUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000019_create_org_ide_configs_tablesUpSql,
		"000019_create_org_ide_configs_tables.up.sql",
	)
}

func _000019_create_org_ide_configs_tablesUpSql() (*asset, error) {
	bytes, err := _000019_create_org_ide_configs_tablesUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000019_create_org_ide_configs_tables.up.sql", size: 195, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000020_empty_domain_name_colDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x0a\x0d\x70\x71\x0c\x71\x55\xc8\x2f\x4a\x2f\x56\x08\x76\x0d\x51\x48\xc9\xcf\x4d\xcc\xcc\x8b\xcf\x4b\xcc\x4d\x55\xb0\x05\x09\x83\x99\xd6\x5c\x5c\x2e\x41\xfe\x01\x0a\x9e\x7e\x2e\xae\x11\x0a\x99\x29\x15\xf1\x20\x0d\xf1\x48\x8a\xad\xb9\xb8\x1c\x7d\x42\x5c\x83\x14\x42\x1c\x9d\x7c\xa0\xe6\x39\xba\xb8\x28\x38\xfb\xfb\x05\x87\x04\x39\x7a\xfa\x85\x28\xa0\x6b\x89\xcf\x4e\xad\x54\x08\xf5\xf3\x0c\x0c\x75\x55\xd0\x40\x12\xd7\xb4\xe6\x02\x04\x00\x00\xff\xff\xd2\x6d\x74\xec\x96\x00\x00\x00")

func _000020_empty_domain_name_colDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000020_empty_domain_name_colDownSql,
		"000020_empty_domain_name_col.down.sql",
	)
}

func _000020_empty_domain_name_colDownSql() (*asset, error) {
	bytes, err := _000020_empty_domain_name_colDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000020_empty_domain_name_col.down.sql", size: 150, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000020_empty_domain_name_colUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\xc8\x2f\x4a\x2f\x56\x70\x09\xf2\x0f\x50\x70\xf6\xf7\x0b\x0e\x09\x72\xf4\xf4\x0b\x01\x0b\xc6\xa7\xe4\xe7\x26\x66\xe6\xc5\xe7\x25\xe6\xa6\xc6\x67\xa7\x56\x5a\x73\x71\x39\x07\xb9\x3a\x86\xb8\x2a\x78\xfa\xb9\xb8\x46\x28\x64\xa6\x54\xc4\xa3\xab\x53\xf0\xf7\x03\xeb\xd5\x40\x12\xd3\xb4\xe6\xe2\x0a\x0d\x70\x01\x69\x04\xdb\x15\xec\x1a\xa2\x80\xac\xc5\x56\xc1\x2f\xd4\xc7\xc7\x9a\x0b\x10\x00\x00\xff\xff\x4b\x81\xec\xba\x95\x00\x00\x00")

func _000020_empty_domain_name_colUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000020_empty_domain_name_colUpSql,
		"000020_empty_domain_name_col.up.sql",
	)
}

func _000020_empty_domain_name_colUpSql() (*asset, error) {
	bytes, err := _000020_empty_domain_name_colUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000020_empty_domain_name_col.up.sql", size: 149, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000021_org_add_invite_signing_keyDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\x70\x8d\x08\x71\xf5\x0b\xf6\xf4\xf7\x53\xf0\x74\x53\x70\x8d\xf0\x0c\x0e\x09\x56\x50\x2a\x48\x4f\x2e\xaa\x2c\x28\xc9\x57\xb2\xe6\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\xc8\x2f\x4a\x2f\x56\x00\xeb\x72\xf6\xf7\x09\xf5\xf5\x53\xc8\xcc\x2b\xcb\x2c\x49\x8d\x2f\xce\x4c\xcf\xcb\xcc\x4b\x8f\xcf\x4e\xad\xb4\xe6\x02\x04\x00\x00\xff\xff\x06\x0e\x35\x73\x56\x00\x00\x00")

func _000021_org_add_invite_signing_keyDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000021_org_add_invite_signing_keyDownSql,
		"000021_org_add_invite_signing_key.down.sql",
	)
}

func _000021_org_add_invite_signing_keyDownSql() (*asset, error) {
	bytes, err := _000021_org_add_invite_signing_keyDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000021_org_add_invite_signing_key.down.sql", size: 86, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000021_org_add_invite_signing_keyUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x04\xc0\x41\x0a\x83\x30\x14\x04\xd0\xbd\xa7\x18\xbc\x86\xab\x54\xa7\x10\x48\x13\x30\x53\xc8\xce\x45\x91\xf0\x29\x44\x51\x29\x78\xfb\xbe\x71\xa6\x13\xc1\x22\xc6\xec\x53\x84\x7f\x22\x26\x81\xc5\x67\x65\xf4\x7b\xfd\x1c\xf7\x7e\x6d\xfd\xd0\xb9\x20\xce\x90\x7b\x04\x62\x3b\xea\x09\x37\x4d\x18\x53\x78\xbf\x22\xac\xfd\xec\x5a\x97\xd3\x6a\xb3\x56\x97\xef\x7a\x43\x2c\x1a\xba\x7f\x00\x00\x00\xff\xff\x34\xfd\x8d\xf9\x60\x00\x00\x00")

func _000021_org_add_invite_signing_keyUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000021_org_add_invite_signing_keyUpSql,
		"000021_org_add_invite_signing_key.up.sql",
	)
}

func _000021_org_add_invite_signing_keyUpSql() (*asset, error) {
	bytes, err := _000021_org_add_invite_signing_keyUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000021_org_add_invite_signing_key.up.sql", size: 96, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000022_users_drop_usernamesDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\xc8\x2f\x4a\x2f\x56\x70\x74\x71\x51\x70\xf6\xf7\x09\xf5\xf5\x53\x28\x2d\x4e\x2d\xca\x4b\xcc\x4d\x55\x28\x4b\x2c\x4a\xce\x48\x2c\xd2\x30\x35\xd0\xb4\xe6\x02\x04\x00\x00\xff\xff\xd4\x8b\xdd\x3a\x32\x00\x00\x00")

func _000022_users_drop_usernamesDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000022_users_drop_usernamesDownSql,
		"000022_users_drop_usernames.down.sql",
	)
}

func _000022_users_drop_usernamesDownSql() (*asset, error) {
	bytes, err := _000022_users_drop_usernamesDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000022_users_drop_usernames.down.sql", size: 50, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000022_users_drop_usernamesUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\x2d\x4e\x2d\x2a\x56\x70\x09\xf2\x0f\x50\x70\xf6\xf7\x09\xf5\xf5\x03\x8b\xe4\x25\xe6\xa6\x5a\x73\x01\x02\x00\x00\xff\xff\xcb\xe3\x6c\xd1\x28\x00\x00\x00")

func _000022_users_drop_usernamesUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000022_users_drop_usernamesUpSql,
		"000022_users_drop_usernames.up.sql",
	)
}

func _000022_users_drop_usernamesUpSql() (*asset, error) {
	bytes, err := _000022_users_drop_usernamesUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000022_users_drop_usernames.up.sql", size: 40, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
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
	"000001_create_org_user_tables.down.sql":                 _000001_create_org_user_tablesDownSql,
	"000001_create_org_user_tables.up.sql":                   _000001_create_org_user_tablesUpSql,
	"000002_add_unique_constraint_email.down.sql":            _000002_add_unique_constraint_emailDownSql,
	"000002_add_unique_constraint_email.up.sql":              _000002_add_unique_constraint_emailUpSql,
	"000003_add_profile_picture.down.sql":                    _000003_add_profile_pictureDownSql,
	"000003_add_profile_picture.up.sql":                      _000003_add_profile_pictureUpSql,
	"000004_add_updated_created_at.down.sql":                 _000004_add_updated_created_atDownSql,
	"000004_add_updated_created_at.up.sql":                   _000004_add_updated_created_atUpSql,
	"000005_create_user_settings_table.down.sql":             _000005_create_user_settings_tableDownSql,
	"000005_create_user_settings_table.up.sql":               _000005_create_user_settings_tableUpSql,
	"000006_add_approved_column.down.sql":                    _000006_add_approved_columnDownSql,
	"000006_add_approved_column.up.sql":                      _000006_add_approved_columnUpSql,
	"000007_add_enable_approvals_column.down.sql":            _000007_add_enable_approvals_columnDownSql,
	"000007_add_enable_approvals_column.up.sql":              _000007_add_enable_approvals_columnUpSql,
	"000008_insert_default_user_value.up.sql":                _000008_insert_default_user_valueUpSql,
	"000009_org_add_updated_created_at.down.sql":             _000009_org_add_updated_created_atDownSql,
	"000009_org_add_updated_created_at.up.sql":               _000009_org_add_updated_created_atUpSql,
	"000010_user_add_identity_provider.down.sql":             _000010_user_add_identity_providerDownSql,
	"000010_user_add_identity_provider.up.sql":               _000010_user_add_identity_providerUpSql,
	"000011_user_set_identity_provider_google_oauth2.up.sql": _000011_user_set_identity_provider_google_oauth2UpSql,
	"000012_user_add_auth_provider_id.down.sql":              _000012_user_add_auth_provider_idDownSql,
	"000012_user_add_auth_provider_id.up.sql":                _000012_user_add_auth_provider_idUpSql,
	"000013_lengthen_profile_email.down.sql":                 _000013_lengthen_profile_emailDownSql,
	"000013_lengthen_profile_email.up.sql":                   _000013_lengthen_profile_emailUpSql,
	"000014_lengthen_auth_provider_id.down.sql":              _000014_lengthen_auth_provider_idDownSql,
	"000014_lengthen_auth_provider_id.up.sql":                _000014_lengthen_auth_provider_idUpSql,
	"000015_create_user_attributes_table.down.sql":           _000015_create_user_attributes_tableDownSql,
	"000015_create_user_attributes_table.up.sql":             _000015_create_user_attributes_tableUpSql,
	"000016_move_user_settings.down.sql":                     _000016_move_user_settingsDownSql,
	"000016_move_user_settings.up.sql":                       _000016_move_user_settingsUpSql,
	"000017_reformat_user_settings.down.sql":                 _000017_reformat_user_settingsDownSql,
	"000017_reformat_user_settings.up.sql":                   _000017_reformat_user_settingsUpSql,
	"000018_add_unique_constraint_auth_provider_id.down.sql": _000018_add_unique_constraint_auth_provider_idDownSql,
	"000018_add_unique_constraint_auth_provider_id.up.sql":   _000018_add_unique_constraint_auth_provider_idUpSql,
	"000019_create_org_ide_configs_tables.down.sql":          _000019_create_org_ide_configs_tablesDownSql,
	"000019_create_org_ide_configs_tables.up.sql":            _000019_create_org_ide_configs_tablesUpSql,
	"000020_empty_domain_name_col.down.sql":                  _000020_empty_domain_name_colDownSql,
	"000020_empty_domain_name_col.up.sql":                    _000020_empty_domain_name_colUpSql,
	"000021_org_add_invite_signing_key.down.sql":             _000021_org_add_invite_signing_keyDownSql,
	"000021_org_add_invite_signing_key.up.sql":               _000021_org_add_invite_signing_keyUpSql,
	"000022_users_drop_usernames.down.sql":                   _000022_users_drop_usernamesDownSql,
	"000022_users_drop_usernames.up.sql":                     _000022_users_drop_usernamesUpSql,
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
	"000001_create_org_user_tables.down.sql":                 &bintree{_000001_create_org_user_tablesDownSql, map[string]*bintree{}},
	"000001_create_org_user_tables.up.sql":                   &bintree{_000001_create_org_user_tablesUpSql, map[string]*bintree{}},
	"000002_add_unique_constraint_email.down.sql":            &bintree{_000002_add_unique_constraint_emailDownSql, map[string]*bintree{}},
	"000002_add_unique_constraint_email.up.sql":              &bintree{_000002_add_unique_constraint_emailUpSql, map[string]*bintree{}},
	"000003_add_profile_picture.down.sql":                    &bintree{_000003_add_profile_pictureDownSql, map[string]*bintree{}},
	"000003_add_profile_picture.up.sql":                      &bintree{_000003_add_profile_pictureUpSql, map[string]*bintree{}},
	"000004_add_updated_created_at.down.sql":                 &bintree{_000004_add_updated_created_atDownSql, map[string]*bintree{}},
	"000004_add_updated_created_at.up.sql":                   &bintree{_000004_add_updated_created_atUpSql, map[string]*bintree{}},
	"000005_create_user_settings_table.down.sql":             &bintree{_000005_create_user_settings_tableDownSql, map[string]*bintree{}},
	"000005_create_user_settings_table.up.sql":               &bintree{_000005_create_user_settings_tableUpSql, map[string]*bintree{}},
	"000006_add_approved_column.down.sql":                    &bintree{_000006_add_approved_columnDownSql, map[string]*bintree{}},
	"000006_add_approved_column.up.sql":                      &bintree{_000006_add_approved_columnUpSql, map[string]*bintree{}},
	"000007_add_enable_approvals_column.down.sql":            &bintree{_000007_add_enable_approvals_columnDownSql, map[string]*bintree{}},
	"000007_add_enable_approvals_column.up.sql":              &bintree{_000007_add_enable_approvals_columnUpSql, map[string]*bintree{}},
	"000008_insert_default_user_value.up.sql":                &bintree{_000008_insert_default_user_valueUpSql, map[string]*bintree{}},
	"000009_org_add_updated_created_at.down.sql":             &bintree{_000009_org_add_updated_created_atDownSql, map[string]*bintree{}},
	"000009_org_add_updated_created_at.up.sql":               &bintree{_000009_org_add_updated_created_atUpSql, map[string]*bintree{}},
	"000010_user_add_identity_provider.down.sql":             &bintree{_000010_user_add_identity_providerDownSql, map[string]*bintree{}},
	"000010_user_add_identity_provider.up.sql":               &bintree{_000010_user_add_identity_providerUpSql, map[string]*bintree{}},
	"000011_user_set_identity_provider_google_oauth2.up.sql": &bintree{_000011_user_set_identity_provider_google_oauth2UpSql, map[string]*bintree{}},
	"000012_user_add_auth_provider_id.down.sql":              &bintree{_000012_user_add_auth_provider_idDownSql, map[string]*bintree{}},
	"000012_user_add_auth_provider_id.up.sql":                &bintree{_000012_user_add_auth_provider_idUpSql, map[string]*bintree{}},
	"000013_lengthen_profile_email.down.sql":                 &bintree{_000013_lengthen_profile_emailDownSql, map[string]*bintree{}},
	"000013_lengthen_profile_email.up.sql":                   &bintree{_000013_lengthen_profile_emailUpSql, map[string]*bintree{}},
	"000014_lengthen_auth_provider_id.down.sql":              &bintree{_000014_lengthen_auth_provider_idDownSql, map[string]*bintree{}},
	"000014_lengthen_auth_provider_id.up.sql":                &bintree{_000014_lengthen_auth_provider_idUpSql, map[string]*bintree{}},
	"000015_create_user_attributes_table.down.sql":           &bintree{_000015_create_user_attributes_tableDownSql, map[string]*bintree{}},
	"000015_create_user_attributes_table.up.sql":             &bintree{_000015_create_user_attributes_tableUpSql, map[string]*bintree{}},
	"000016_move_user_settings.down.sql":                     &bintree{_000016_move_user_settingsDownSql, map[string]*bintree{}},
	"000016_move_user_settings.up.sql":                       &bintree{_000016_move_user_settingsUpSql, map[string]*bintree{}},
	"000017_reformat_user_settings.down.sql":                 &bintree{_000017_reformat_user_settingsDownSql, map[string]*bintree{}},
	"000017_reformat_user_settings.up.sql":                   &bintree{_000017_reformat_user_settingsUpSql, map[string]*bintree{}},
	"000018_add_unique_constraint_auth_provider_id.down.sql": &bintree{_000018_add_unique_constraint_auth_provider_idDownSql, map[string]*bintree{}},
	"000018_add_unique_constraint_auth_provider_id.up.sql":   &bintree{_000018_add_unique_constraint_auth_provider_idUpSql, map[string]*bintree{}},
	"000019_create_org_ide_configs_tables.down.sql":          &bintree{_000019_create_org_ide_configs_tablesDownSql, map[string]*bintree{}},
	"000019_create_org_ide_configs_tables.up.sql":            &bintree{_000019_create_org_ide_configs_tablesUpSql, map[string]*bintree{}},
	"000020_empty_domain_name_col.down.sql":                  &bintree{_000020_empty_domain_name_colDownSql, map[string]*bintree{}},
	"000020_empty_domain_name_col.up.sql":                    &bintree{_000020_empty_domain_name_colUpSql, map[string]*bintree{}},
	"000021_org_add_invite_signing_key.down.sql":             &bintree{_000021_org_add_invite_signing_keyDownSql, map[string]*bintree{}},
	"000021_org_add_invite_signing_key.up.sql":               &bintree{_000021_org_add_invite_signing_keyUpSql, map[string]*bintree{}},
	"000022_users_drop_usernames.down.sql":                   &bintree{_000022_users_drop_usernamesDownSql, map[string]*bintree{}},
	"000022_users_drop_usernames.up.sql":                     &bintree{_000022_users_drop_usernamesUpSql, map[string]*bintree{}},
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
