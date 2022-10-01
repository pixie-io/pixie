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
// 000008_connected_cluster_status_enum.down.sql
// 000008_connected_cluster_status_enum.up.sql
// 000009_update_failed_cluster_status_enum.down.sql
// 000009_update_failed_cluster_status_enum.up.sql
// 000010_add_vizier_info_to_cluster_info.down.sql
// 000010_add_vizier_info_to_cluster_info.up.sql
// 000011_add_updated_at_to_vizier_cluster.down.sql
// 000011_add_updated_at_to_vizier_cluster.up.sql
// 000012_add_deployment_keys.down.sql
// 000012_add_deployment_keys.up.sql
// 000013_add_desc_to_deploy_key.down.sql
// 000013_add_desc_to_deploy_key.up.sql
// 000014_add_unique_org_name_constraint.down.sql
// 000014_add_unique_org_name_constraint.up.sql
// 000015_move_cluster_info_columns.down.sql
// 000015_move_cluster_info_columns.up.sql
// 000016_trim_cluster_name.down.sql
// 000016_trim_cluster_name.up.sql
// 000017_add_pod_status_to_vizier_cluster.down.sql
// 000017_add_pod_status_to_vizier_cluster.up.sql
// 000018_add_num_nodes_to_cluster_table.down.sql
// 000018_add_num_nodes_to_cluster_table.up.sql
// 000019_drop_index_table.down.sql
// 000019_drop_index_table.up.sql
// 000020_add_auto_update_to_cluster_table.down.sql
// 000020_add_auto_update_to_cluster_table.up.sql
// 000021_add_status_message_to_cluster_info.down.sql
// 000021_add_status_message_to_cluster_info.up.sql
// 000022_add_data_plane_and_past_status_columns.down.sql
// 000022_add_data_plane_and_past_status_columns.up.sql
// 000023_add_triggers_for_prev.down.sql
// 000023_add_triggers_for_prev.up.sql
// 000024_add_encrypted_deployment_key.down.sql
// 000024_add_encrypted_deployment_key.up.sql
// 000025_drop_deployment_key_old.down.sql
// 000025_drop_deployment_key_old.up.sql
// 000026_remove_extraneous_columns.down.sql
// 000026_remove_extraneous_columns.up.sql
// 000027_add_vizier_cluster_indices.down.sql
// 000027_add_vizier_cluster_indices.up.sql
// 000028_remove_previous_status_cols.down.sql
// 000028_remove_previous_status_cols.up.sql
// 000029_remove_first_seen.down.sql
// 000029_remove_first_seen.up.sql
// 000030_add_degraded_cluster_status_enum.down.sql
// 000030_add_degraded_cluster_status_enum.up.sql
// 000031_add_operator_version_column.down.sql
// 000031_add_operator_version_column.up.sql
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

	info := bindataFileInfo{name: "000001_create_cluster_tables.down.sql", size: 114, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
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

	info := bindataFileInfo{name: "000001_create_cluster_tables.up.sql", size: 1024, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
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

	info := bindataFileInfo{name: "000003_create_index_table.down.sql", size: 41, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
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

	info := bindataFileInfo{name: "000003_create_index_table.up.sql", size: 371, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
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

	info := bindataFileInfo{name: "000004_create_shard_index.down.sql", size: 39, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
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

	info := bindataFileInfo{name: "000004_create_shard_index.up.sql", size: 166, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
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

	info := bindataFileInfo{name: "000005_add_passthrough_to_cluster_table.down.sql", size: 65, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
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

	info := bindataFileInfo{name: "000005_add_passthrough_to_cluster_table.up.sql", size: 85, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
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

	info := bindataFileInfo{name: "000006_add_project_id_to_cluster_table.down.sql", size: 108, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
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

	info := bindataFileInfo{name: "000006_add_project_id_to_cluster_table.up.sql", size: 163, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
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

	info := bindataFileInfo{name: "000007_update_cluster_status_enum.down.sql", size: 112, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000007_update_cluster_status_enumUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x89\x0c\x70\x55\x28\xcb\xac\xca\x4c\x2d\x8a\x2f\x2e\x49\x2c\x29\x2d\x56\x70\x74\x71\x51\x08\x73\xf4\x09\x75\x55\x50\x0f\x0d\x70\x71\x0c\xf1\xf4\x73\x57\x57\x70\x74\x03\xa9\x56\x77\xf1\x0c\x76\xf6\xf7\xf3\x73\x75\x0e\x71\x75\x51\xb7\xe6\x02\x04\x00\x00\xff\xff\x2c\x73\x19\x04\x44\x00\x00\x00")

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

	info := bindataFileInfo{name: "000007_update_cluster_status_enum.up.sql", size: 68, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000008_connected_cluster_status_enumDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\x08\x89\x0c\x70\x55\x28\xcb\xac\xca\x4c\x2d\x8a\x2f\x2e\x49\x2c\x29\x2d\xb6\xe6\x72\x0e\x72\x75\x0c\x71\xc5\x22\xa5\xe0\x18\xac\xe0\xea\x17\xea\xab\xa0\xa1\x1e\xea\xe7\xed\xe7\x1f\xee\xa7\xae\xa3\xa0\xee\xe1\xea\xe8\x13\xe2\x11\x09\x62\x86\xfa\x21\x71\x5c\x3c\x83\x9d\xfd\xfd\xfc\x5c\x9d\x43\x5c\x5d\xc0\x92\x01\x2e\x8e\x21\x9e\x7e\xee\xea\x9a\xd6\x5c\x80\x00\x00\x00\xff\xff\x08\x85\xa3\x9f\x7c\x00\x00\x00")

func _000008_connected_cluster_status_enumDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000008_connected_cluster_status_enumDownSql,
		"000008_connected_cluster_status_enum.down.sql",
	)
}

func _000008_connected_cluster_status_enumDownSql() (*asset, error) {
	bytes, err := _000008_connected_cluster_status_enumDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000008_connected_cluster_status_enum.down.sql", size: 124, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000008_connected_cluster_status_enumUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x89\x0c\x70\x55\x28\xcb\xac\xca\x4c\x2d\x8a\x2f\x2e\x49\x2c\x29\x2d\x56\x70\x74\x71\x51\x08\x73\xf4\x09\x75\x55\x50\x77\xf6\xf7\xf3\x73\x75\x0e\x71\x75\x51\x57\x70\x74\x03\x29\x57\x0f\x0d\x70\x71\x0c\xf1\xf4\x73\x57\xb7\xe6\x02\x04\x00\x00\xff\xff\x25\xaa\x03\x86\x41\x00\x00\x00")

func _000008_connected_cluster_status_enumUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000008_connected_cluster_status_enumUpSql,
		"000008_connected_cluster_status_enum.up.sql",
	)
}

func _000008_connected_cluster_status_enumUpSql() (*asset, error) {
	bytes, err := _000008_connected_cluster_status_enumUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000008_connected_cluster_status_enum.up.sql", size: 65, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000009_update_failed_cluster_status_enumDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\x08\x89\x0c\x70\x55\x28\xcb\xac\xca\x4c\x2d\x8a\x2f\x2e\x49\x2c\x29\x2d\xb6\xe6\x72\x0e\x72\x75\x0c\x71\xc5\x22\xa5\xe0\x18\xac\xe0\xea\x17\xea\xab\xa0\xa1\x1e\xea\xe7\xed\xe7\x1f\xee\xa7\xae\xa3\xa0\xee\xe1\xea\xe8\x13\xe2\x11\x09\x62\x86\xfa\x21\x71\x5c\x3c\x83\x9d\xfd\xfd\xfc\x5c\x9d\x43\x5c\x5d\xc0\x92\x01\x2e\x8e\x21\x9e\x7e\xee\x20\x36\x42\x42\xd3\x9a\x0b\x10\x00\x00\xff\xff\x72\x79\xf5\xe9\x89\x00\x00\x00")

func _000009_update_failed_cluster_status_enumDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000009_update_failed_cluster_status_enumDownSql,
		"000009_update_failed_cluster_status_enum.down.sql",
	)
}

func _000009_update_failed_cluster_status_enumDownSql() (*asset, error) {
	bytes, err := _000009_update_failed_cluster_status_enumDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000009_update_failed_cluster_status_enum.down.sql", size: 137, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000009_update_failed_cluster_status_enumUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x89\x0c\x70\x55\x28\xcb\xac\xca\x4c\x2d\x8a\x2f\x2e\x49\x2c\x29\x2d\x56\x70\x74\x71\x51\x08\x73\xf4\x09\x75\x55\x50\x0f\x0d\x70\x71\x0c\x71\x8d\x77\x73\xf4\xf4\x71\x75\x51\x57\x70\x74\x03\x69\x51\x77\xf6\xf7\xf3\x73\x75\x0e\x71\x75\x51\xb7\xe6\x02\x04\x00\x00\xff\xff\x75\x34\x49\x0e\x46\x00\x00\x00")

func _000009_update_failed_cluster_status_enumUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000009_update_failed_cluster_status_enumUpSql,
		"000009_update_failed_cluster_status_enum.up.sql",
	)
}

func _000009_update_failed_cluster_status_enumUpSql() (*asset, error) {
	bytes, err := _000009_update_failed_cluster_status_enumUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000009_update_failed_cluster_status_enum.up.sql", size: 70, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000010_add_vizier_info_to_cluster_infoDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\xcb\xac\xca\x4c\x2d\x8a\x4f\xce\x29\x2d\x2e\x49\x2d\x8a\xcf\xcc\x4b\xcb\xe7\x72\x09\xf2\x0f\x50\x70\xf6\xf7\x09\xf5\xf5\x83\xc9\x97\xa5\x16\x15\x67\xe6\xe7\xe9\xa0\xc8\xc1\x34\xe5\x25\xe6\xa6\x62\x97\xc1\xab\xad\x34\x33\xc5\x9a\x0b\x10\x00\x00\xff\xff\x1b\x7c\x41\x1a\x8c\x00\x00\x00")

func _000010_add_vizier_info_to_cluster_infoDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000010_add_vizier_info_to_cluster_infoDownSql,
		"000010_add_vizier_info_to_cluster_info.down.sql",
	)
}

func _000010_add_vizier_info_to_cluster_infoDownSql() (*asset, error) {
	bytes, err := _000010_add_vizier_info_to_cluster_infoDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000010_add_vizier_info_to_cluster_info.down.sql", size: 140, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000010_add_vizier_info_to_cluster_infoUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\xcb\xac\xca\x4c\x2d\x8a\x4f\xce\x29\x2d\x2e\x49\x2d\x8a\xcf\xcc\x4b\xcb\xe7\x72\x74\x71\x51\x70\xf6\xf7\x09\xf5\xf5\x83\x49\x97\xa5\x16\x15\x67\xe6\xe7\x29\x94\x25\x16\x25\x67\x24\x16\x69\x18\x1a\x18\x18\x68\xea\x20\x2b\x84\x19\x90\x97\x98\x9b\x4a\x84\x32\xe2\x0d\x2c\xcd\x4c\x41\x55\x65\xcd\x05\x08\x00\x00\xff\xff\xfe\xe4\x5a\xc6\xc0\x00\x00\x00")

func _000010_add_vizier_info_to_cluster_infoUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000010_add_vizier_info_to_cluster_infoUpSql,
		"000010_add_vizier_info_to_cluster_info.up.sql",
	)
}

func _000010_add_vizier_info_to_cluster_infoUpSql() (*asset, error) {
	bytes, err := _000010_add_vizier_info_to_cluster_infoUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000010_add_vizier_info_to_cluster_info.up.sql", size: 192, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000011_add_updated_at_to_vizier_clusterDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\xcb\xac\xca\x4c\x2d\x8a\x4f\xce\x29\x2d\x2e\x49\x2d\xe2\x72\x09\xf2\x0f\x50\x70\xf6\xf7\x09\xf5\xf5\x53\x28\x2d\x48\x49\x2c\x49\x4d\x89\x4f\x2c\xb1\xe6\x82\x48\x84\x04\x79\xba\xbb\xbb\x06\x29\x78\xba\x29\xb8\x46\x78\x06\x87\x04\x43\xd5\xc4\xa3\x9a\x12\x8f\xd0\xa9\xe0\xef\x87\x66\x85\x35\x17\x20\x00\x00\xff\xff\x13\xad\x96\x33\x7f\x00\x00\x00")

func _000011_add_updated_at_to_vizier_clusterDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000011_add_updated_at_to_vizier_clusterDownSql,
		"000011_add_updated_at_to_vizier_cluster.down.sql",
	)
}

func _000011_add_updated_at_to_vizier_clusterDownSql() (*asset, error) {
	bytes, err := _000011_add_updated_at_to_vizier_clusterDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000011_add_updated_at_to_vizier_cluster.down.sql", size: 127, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000011_add_updated_at_to_vizier_clusterUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x7c\x90\xcd\xae\xda\x30\x10\x85\xf7\x7e\x8a\xb3\x88\x74\x41\x6a\xfa\x02\xa8\x0b\x93\x4c\x68\xa4\x60\x47\x8e\x2d\xba\x43\x16\x98\x10\xc9\x4d\x68\xe2\x40\xdb\xa7\xaf\x08\x8d\xa0\x3f\xba\xde\x58\xf2\x78\xe6\xfb\xce\xc4\x31\xf2\xb6\x09\x8d\xf5\xfe\x07\x06\x17\xd0\x76\x38\xba\x93\x1d\x7d\xf8\x80\xa1\x43\x38\xdb\x00\xf7\xbd\x19\x42\xd3\xd6\xe8\xbb\xdb\x80\xb3\xbd\x3a\x58\x08\x53\x14\x18\x2f\x47\x1b\xdc\x71\x6f\x03\x0e\x9d\x1f\xbf\xb6\x8c\x17\x9a\x14\x34\x5f\x17\x84\x6b\xf3\xb3\x71\xfd\xfe\xe0\xc7\x21\xb8\x1e\x3c\x4d\x91\xc8\xc2\x6c\xc5\x6b\x9f\xce\xb7\x54\x69\xbe\x2d\x57\x2c\x8e\x51\xb9\x80\x70\x76\xb3\x03\x42\x07\x21\x77\x8b\x25\x4e\x5d\x8f\xd3\x18\xc6\xde\x4d\x16\xef\x72\xa6\xd2\xbf\xa4\x8a\x34\x52\xca\xb8\x29\x34\xda\xee\xb6\x58\xae\x18\x4b\x14\x71\x4d\x90\x0a\x8a\xca\x82\x27\x84\xcc\x88\x44\xe7\x72\xee\xdc\x3f\x07\x2c\x96\x0c\x50\xa4\x8d\x12\x15\xb4\xca\x37\x1b\x52\xe0\x15\xa2\x88\x01\x6b\xda\xe4\x82\x61\x3a\x82\x76\x1f\x5f\xb8\x9f\x66\xda\xa3\xfa\x98\x70\xff\x74\x7f\x21\x91\xde\xaf\x28\x82\xb7\x6d\x3d\xda\xda\xe1\xed\xe2\x2f\xf5\xf0\xcd\xbf\x3d\xfd\x66\xda\x6f\xa7\x3f\x03\xbf\x28\xb2\x35\x65\x52\x11\x4c\x99\x4e\xb1\xc4\x5f\xbb\x61\x99\x54\x20\x9e\x7c\x86\x92\x3b\xd0\x17\x4a\x8c\x26\x94\x4a\x26\x94\x1a\x45\xff\xcb\xbc\x62\xbf\x02\x00\x00\xff\xff\xe3\x2b\x46\x12\x24\x02\x00\x00")

func _000011_add_updated_at_to_vizier_clusterUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000011_add_updated_at_to_vizier_clusterUpSql,
		"000011_add_updated_at_to_vizier_cluster.up.sql",
	)
}

func _000011_add_updated_at_to_vizier_clusterUpSql() (*asset, error) {
	bytes, err := _000011_add_updated_at_to_vizier_clusterUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000011_add_updated_at_to_vizier_cluster.up.sql", size: 548, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000012_add_deployment_keysDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\x08\x71\x74\xf2\x71\x55\xf0\x74\x53\x70\x8d\xf0\x0c\x0e\x09\x56\x28\xcb\xac\xca\x4c\x2d\x8a\x4f\x49\x2d\xc8\xc9\xaf\xcc\x4d\xcd\x2b\x89\xcf\x4e\xad\x2c\xb6\xe6\x02\x04\x00\x00\xff\xff\x79\x3e\x94\xa4\x2d\x00\x00\x00")

func _000012_add_deployment_keysDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000012_add_deployment_keysDownSql,
		"000012_add_deployment_keys.down.sql",
	)
}

func _000012_add_deployment_keysDownSql() (*asset, error) {
	bytes, err := _000012_add_deployment_keysDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000012_add_deployment_keys.down.sql", size: 45, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000012_add_deployment_keysUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x7c\x90\x31\xaf\x9b\x30\x1c\xc4\x77\x3e\xc5\xe9\x4d\x44\x6a\xaa\x54\xea\xf6\x26\xda\x38\x92\x55\x42\xd2\x60\xd4\x64\x42\x0e\xfc\x13\xac\x10\x3b\xb2\x4d\x10\xfd\xf4\x95\x81\xa6\x4b\xf5\x36\xb0\xef\x7e\xe7\xbb\xef\x07\x96\x08\x06\x76\x14\x2c\xcb\xf9\x2e\x03\xdf\x20\xdb\x09\xb0\x23\xcf\x45\x8e\xb7\xae\x53\xf5\xd2\x38\xf7\x78\x7b\x8f\xa2\xe5\x12\xa2\x51\x0e\x5e\x9e\x5b\x42\x65\xb4\x97\x4a\x3b\xdc\x68\x70\xf0\x8d\xf4\xa8\xa4\xc6\x99\xd0\x39\xaa\xe1\x0d\x6a\x7a\xb4\x66\x80\x84\xa6\x1e\x4f\xf5\x5b\x91\xfd\x1c\xcd\x89\x22\xf9\x96\xb2\xf9\xb0\x9c\x84\x77\xd2\xbe\x1c\x61\x71\x04\x8c\x61\x04\xbe\x0e\xa4\xce\x11\x2e\xc6\xc2\x87\xf8\xaa\xed\x9c\x0f\x28\x40\xd5\x28\x0a\xbe\x46\x91\xf1\x9f\x05\xc3\x9a\x6d\x92\x22\x15\x08\xaf\x2e\xaf\xa4\xc9\x4a\x4f\xe5\xf3\x6b\xbc\xf8\x34\x11\x8d\xbd\x96\xaa\x46\xe8\xd0\x10\x4c\xaf\xc9\xc2\x5c\x26\xec\x8d\x86\x80\x9c\x25\x23\x36\x2c\x91\x15\x69\x3a\xbb\x3b\x47\x36\xdc\x8d\x5d\x3f\x62\xfc\x15\xfe\x0f\x22\xd4\x9d\x9c\x97\xf7\x07\xfa\x86\xf4\xcb\x86\x5e\x3a\x54\x96\xa4\xa7\x3a\x20\xe6\xcf\x52\x7a\x08\xbe\x65\xb9\x48\xb6\xfb\x57\xbf\x6c\xf7\x2b\x54\xfa\xb7\x92\xac\x7c\x27\xdb\x91\xe3\x64\x3b\x23\xc2\xdf\x53\xda\xaa\x91\x36\xfe\xb2\x5a\xad\x26\xc7\x34\x55\x7c\xa3\x61\x1c\x65\x7f\xe0\xdb\xe4\x70\xc2\x0f\x76\x8a\x55\xbd\x88\x16\xef\xd1\x9f\x00\x00\x00\xff\xff\xda\x80\xcc\x35\x14\x02\x00\x00")

func _000012_add_deployment_keysUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000012_add_deployment_keysUpSql,
		"000012_add_deployment_keys.up.sql",
	)
}

func _000012_add_deployment_keysUpSql() (*asset, error) {
	bytes, err := _000012_add_deployment_keysUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000012_add_deployment_keys.up.sql", size: 532, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000013_add_desc_to_deploy_keyDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\xcb\xac\xca\x4c\x2d\x8a\x4f\x49\x2d\xc8\xc9\xaf\xcc\x4d\xcd\x2b\x89\xcf\x4e\xad\x2c\x56\x70\x09\xf2\x0f\x50\x70\xf6\xf7\x09\xf5\xf5\x53\x48\x49\x2d\x4e\x2e\xca\x2c\x28\xc9\xcc\xcf\xb3\xe6\x02\x04\x00\x00\xff\xff\xa8\xfd\xa0\xf5\x3c\x00\x00\x00")

func _000013_add_desc_to_deploy_keyDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000013_add_desc_to_deploy_keyDownSql,
		"000013_add_desc_to_deploy_key.down.sql",
	)
}

func _000013_add_desc_to_deploy_keyDownSql() (*asset, error) {
	bytes, err := _000013_add_desc_to_deploy_keyDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000013_add_desc_to_deploy_key.down.sql", size: 60, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000013_add_desc_to_deploy_keyUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\xcb\xac\xca\x4c\x2d\x8a\x4f\x49\x2d\xc8\xc9\xaf\xcc\x4d\xcd\x2b\x89\xcf\x4e\xad\x2c\x56\x70\x74\x71\x51\x70\xf6\xf7\x09\xf5\xf5\x53\x48\x49\x2d\x4e\x2e\xca\x2c\x28\xc9\xcc\xcf\x53\x28\x4b\x2c\x4a\xce\x48\x2c\xd2\x30\x34\x30\x30\xd0\xb4\xe6\x02\x04\x00\x00\xff\xff\xfc\x5c\x48\x7b\x49\x00\x00\x00")

func _000013_add_desc_to_deploy_keyUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000013_add_desc_to_deploy_keyUpSql,
		"000013_add_desc_to_deploy_key.up.sql",
	)
}

func _000013_add_desc_to_deploy_keyUpSql() (*asset, error) {
	bytes, err := _000013_add_desc_to_deploy_keyUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000013_add_desc_to_deploy_key.up.sql", size: 73, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000014_add_unique_org_name_constraintDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\xcb\xac\xca\x4c\x2d\x8a\x4f\xce\x29\x2d\x2e\x49\x2d\x8a\xcf\xcc\x4b\xcb\x57\x70\x09\xf2\x0f\x50\x70\xf6\xf7\x0b\x0e\x09\x72\xf4\xf4\x0b\x51\x28\xcd\xcb\x2c\x8c\xcf\x2f\x4a\x87\xab\xca\x4b\xcc\x4d\xb5\xe6\x02\x04\x00\x00\xff\xff\x75\x44\x92\x0f\x47\x00\x00\x00")

func _000014_add_unique_org_name_constraintDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000014_add_unique_org_name_constraintDownSql,
		"000014_add_unique_org_name_constraint.down.sql",
	)
}

func _000014_add_unique_org_name_constraintDownSql() (*asset, error) {
	bytes, err := _000014_add_unique_org_name_constraintDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000014_add_unique_org_name_constraint.down.sql", size: 71, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000014_add_unique_org_name_constraintUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x4c\x8d\xc1\xca\x82\x40\x1c\xc4\xef\x3e\xc5\x1c\xbf\xef\x60\x2f\xd0\x69\x4b\x0f\x82\x18\xd9\x4a\x47\x31\x1b\x73\x41\x77\x69\xfd\xbb\x41\x4f\x1f\x4a\x48\xb7\x99\xe1\xc7\xfc\xe2\x18\x57\xc2\x92\x77\x88\x43\xeb\x6c\xa0\x17\x48\x6f\xa6\xa5\xcf\xd6\x3c\x67\xc2\xf9\x07\xda\x61\x9e\x84\x1e\xb6\x19\x89\xa6\x5b\xe2\x8b\x18\x5d\xe0\x46\x4b\x4f\x38\xe9\xe9\x21\xcd\x6d\xe0\x2e\x52\xb9\x4e\x4b\x68\x75\xc8\x53\x04\xf3\x36\xf4\xf5\xf7\xa6\x36\xb6\x73\x50\x49\x82\xe3\xa9\xb8\xe8\x52\x65\x85\x5e\x6d\x1b\xb0\x7a\xaa\x22\x3b\x57\x29\xfe\x7e\xc7\xff\x7d\xf4\x09\x00\x00\xff\xff\x43\xda\x4a\xaf\xb5\x00\x00\x00")

func _000014_add_unique_org_name_constraintUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000014_add_unique_org_name_constraintUpSql,
		"000014_add_unique_org_name_constraint.up.sql",
	)
}

func _000014_add_unique_org_name_constraintUpSql() (*asset, error) {
	bytes, err := _000014_add_unique_org_name_constraintUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000014_add_unique_org_name_constraint.up.sql", size: 181, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000015_move_cluster_info_columnsDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\xcb\xac\xca\x4c\x2d\x8a\x4f\xce\x29\x2d\x2e\x49\x2d\xe2\x72\x09\xf2\x0f\x50\x70\xf6\xf7\x09\xf5\xf5\x53\x80\x8a\xc5\xe7\x25\xe6\xa6\xea\x60\x95\x29\x4b\x2d\x2a\xce\xcc\xcf\xc3\x2e\x59\x9a\x99\x62\xcd\x05\x08\x00\x00\xff\xff\x3c\x29\xe9\xb7\x6b\x00\x00\x00")

func _000015_move_cluster_info_columnsDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000015_move_cluster_info_columnsDownSql,
		"000015_move_cluster_info_columns.down.sql",
	)
}

func _000015_move_cluster_info_columnsDownSql() (*asset, error) {
	bytes, err := _000015_move_cluster_info_columnsDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000015_move_cluster_info_columns.down.sql", size: 107, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000015_move_cluster_info_columnsUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x8c\x8f\xbd\x0a\xc2\x30\x14\x46\xf7\x3c\xc5\x1d\x2b\x84\x52\xe7\xe2\x10\x6d\x44\xa1\x3f\x5a\x13\x1c\x43\x69\xa2\x5e\xd0\x16\x52\xdb\xc1\xa7\x17\xa1\x43\xff\xac\xae\x97\x73\x2e\xdf\x61\xa1\xe0\x29\x08\xb6\x0e\x39\x34\xf8\x42\x63\x55\x7e\xaf\xab\xa7\xb1\x84\x05\x01\x6c\x92\x50\x46\x31\xb4\x27\x55\x64\x0f\x03\x4d\x66\xf3\x5b\x66\x9d\xa5\xe7\x79\x0b\x3a\x85\x35\xc6\x56\x58\x16\x7f\x90\x35\xea\x3e\xe5\x13\xf2\x63\x92\x8c\xf7\x47\xc9\xc1\x29\xed\x55\xa1\xa6\xbd\x6d\x1f\x5d\x1e\x02\x26\x46\xe6\x89\x8b\x7e\xc5\x6a\x40\x28\x2c\x2e\xa5\xdb\x45\xe8\xa8\x67\xde\x69\x29\x4a\xba\x71\xf3\x4a\x8d\x9a\x6c\xd3\x24\x9a\x82\xe0\xbc\xe3\xe9\xb0\xc3\xfd\xfa\x72\x78\xd3\x3e\x79\x07\x00\x00\xff\xff\x4d\x21\xcc\x94\xdc\x01\x00\x00")

func _000015_move_cluster_info_columnsUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000015_move_cluster_info_columnsUpSql,
		"000015_move_cluster_info_columns.up.sql",
	)
}

func _000015_move_cluster_info_columnsUpSql() (*asset, error) {
	bytes, err := _000015_move_cluster_info_columnsUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000015_move_cluster_info_columns.up.sql", size: 476, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000016_trim_cluster_nameDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\xd2\xd5\x55\x08\xc9\xc8\x2c\x56\xc8\x2c\x56\x48\xcc\x53\xc8\x2c\x2a\x4a\x2d\x4b\x2d\x2a\xce\x4c\xca\x49\x55\xc8\xcd\x4c\x2f\x4a\x2c\xc9\xcc\xcf\xd3\xe3\x02\x04\x00\x00\xff\xff\xf0\x8b\xeb\xff\x26\x00\x00\x00")

func _000016_trim_cluster_nameDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000016_trim_cluster_nameDownSql,
		"000016_trim_cluster_name.down.sql",
	)
}

func _000016_trim_cluster_nameDownSql() (*asset, error) {
	bytes, err := _000016_trim_cluster_nameDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000016_trim_cluster_name.down.sql", size: 38, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000016_trim_cluster_nameUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x0a\x0d\x70\x71\x0c\x71\x55\x28\xcb\xac\xca\x4c\x2d\x8a\x4f\xce\x29\x2d\x2e\x49\x2d\xe2\x0a\x76\x0d\x51\x80\xb2\xe3\xf3\x12\x73\x53\x15\x6c\x15\x8a\x52\xd3\x53\x2b\x0a\xe2\x8b\x52\x0b\x72\x12\x93\x53\x35\x90\x65\x75\x14\x5c\xd5\xa3\x63\x62\xf2\x62\x62\x8a\x62\xb5\xd5\x75\x14\xd4\x15\x40\x44\xba\xba\x82\xa6\x35\x17\x20\x00\x00\xff\xff\x5c\xaa\xb8\xed\x60\x00\x00\x00")

func _000016_trim_cluster_nameUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000016_trim_cluster_nameUpSql,
		"000016_trim_cluster_name.up.sql",
	)
}

func _000016_trim_cluster_nameUpSql() (*asset, error) {
	bytes, err := _000016_trim_cluster_nameUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000016_trim_cluster_name.up.sql", size: 96, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000017_add_pod_status_to_vizier_clusterDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\xcb\xac\xca\x4c\x2d\x8a\x4f\xce\x29\x2d\x2e\x49\x2d\x8a\xcf\xcc\x4b\xcb\x57\x70\x09\xf2\x0f\x50\x70\xf6\xf7\x09\xf5\xf5\x53\x48\xce\xcf\x2b\x29\xca\xcf\x89\x2f\xc8\x49\xcc\x4b\x8d\x2f\xc8\x4f\x89\x2f\x2e\x49\x2c\x29\x2d\x4e\x2d\x56\xc8\x2a\xce\xcf\xb3\xe6\x02\x04\x00\x00\xff\xff\xf5\x01\x1e\xf5\x4d\x00\x00\x00")

func _000017_add_pod_status_to_vizier_clusterDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000017_add_pod_status_to_vizier_clusterDownSql,
		"000017_add_pod_status_to_vizier_cluster.down.sql",
	)
}

func _000017_add_pod_status_to_vizier_clusterDownSql() (*asset, error) {
	bytes, err := _000017_add_pod_status_to_vizier_clusterDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000017_add_pod_status_to_vizier_cluster.down.sql", size: 77, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000017_add_pod_status_to_vizier_clusterUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x04\xc0\xb1\x0e\xc2\x20\x10\x06\xe0\xbd\x4f\xf1\x6f\x7d\x08\x27\x14\x9c\x4e\x9a\x18\x98\x2f\x4d\xc5\x04\x43\xb8\x86\xbb\x3a\x68\x7c\x77\x3f\x47\x29\xdc\x91\xdc\x99\x02\xde\xf5\x53\xcb\xe0\xad\x1d\x6a\x65\x70\xed\x4f\x99\x9c\xf7\xb8\x2c\x94\x6f\x11\x9b\x74\x1b\xd2\x78\x6f\x6b\x2f\xbc\xcb\x83\xd5\x56\x3b\xb4\x28\x5e\x2a\x1d\x71\x49\x88\x99\x08\x3e\x5c\x5d\xa6\x84\xf9\xfb\x9b\x4f\xd3\x3f\x00\x00\xff\xff\x21\x8b\x54\x3b\x62\x00\x00\x00")

func _000017_add_pod_status_to_vizier_clusterUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000017_add_pod_status_to_vizier_clusterUpSql,
		"000017_add_pod_status_to_vizier_cluster.up.sql",
	)
}

func _000017_add_pod_status_to_vizier_clusterUpSql() (*asset, error) {
	bytes, err := _000017_add_pod_status_to_vizier_clusterUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000017_add_pod_status_to_vizier_cluster.up.sql", size: 98, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000018_add_num_nodes_to_cluster_tableDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\xcb\xac\xca\x4c\x2d\x8a\x4f\xce\x29\x2d\x2e\x49\x2d\x8a\xcf\xcc\x4b\xcb\xe7\x72\x09\xf2\x0f\x50\x70\xf6\xf7\x09\xf5\xf5\x53\xc8\x2b\xcd\x8d\xcf\xcb\x4f\x49\x2d\xb6\xe6\xe2\x22\x55\x5f\x66\x5e\x71\x49\x51\x69\x6e\x6a\x5e\x49\x6a\x0a\xcc\x10\x40\x00\x00\x00\xff\xff\x77\x03\x68\x45\x7c\x00\x00\x00")

func _000018_add_num_nodes_to_cluster_tableDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000018_add_num_nodes_to_cluster_tableDownSql,
		"000018_add_num_nodes_to_cluster_table.down.sql",
	)
}

func _000018_add_num_nodes_to_cluster_tableDownSql() (*asset, error) {
	bytes, err := _000018_add_num_nodes_to_cluster_tableDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000018_add_num_nodes_to_cluster_table.down.sql", size: 124, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000018_add_num_nodes_to_cluster_tableUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\xcb\xac\xca\x4c\x2d\x8a\x4f\xce\x29\x2d\x2e\x49\x2d\x8a\xcf\xcc\x4b\xcb\xe7\x72\x74\x71\x51\x70\xf6\xf7\x09\xf5\xf5\x53\xc8\x2b\xcd\x8d\xcf\xcb\x4f\x49\x2d\x56\xf0\xf4\x0b\x51\xf0\xf3\x0f\x51\xf0\x0b\xf5\xf1\x51\x70\x71\x75\x73\x0c\xf5\x09\x51\x30\xb0\xe6\xe2\x22\xd1\xb4\xcc\xbc\xe2\x92\xa2\xd2\xdc\xd4\xbc\x92\xd4\x14\x02\x46\x03\x02\x00\x00\xff\xff\x68\x07\xd7\x8c\xa8\x00\x00\x00")

func _000018_add_num_nodes_to_cluster_tableUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000018_add_num_nodes_to_cluster_tableUpSql,
		"000018_add_num_nodes_to_cluster_table.up.sql",
	)
}

func _000018_add_num_nodes_to_cluster_tableUpSql() (*asset, error) {
	bytes, err := _000018_add_num_nodes_to_cluster_tableUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000018_add_num_nodes_to_cluster_table.up.sql", size: 168, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000019_drop_index_tableDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x6c\x8f\xc1\x6a\xeb\x30\x10\x45\xf7\xfa\x8a\xbb\x8b\x0d\x76\x78\x8b\xd7\x55\x56\xae\xad\x80\xa8\xe3\xb4\x8e\x5c\xc8\x4a\xa8\xf6\x24\x16\x04\x19\x24\xd9\x84\x7e\x7d\xb1\x9b\xd0\x42\xba\x1b\x66\x38\x77\xee\x49\x53\xc8\xde\x78\x04\xfd\x71\x21\xb4\x83\x0d\xda\x58\x0f\x63\x3b\xba\xc2\x07\x1d\x08\xa7\xc1\x81\x74\xdb\x63\x32\x9f\x86\xdc\x9a\xe5\x35\xcf\x24\x87\xcc\x9e\x4b\x7e\x5b\xaa\x05\x50\xdf\x40\xc4\x80\x25\x97\x20\x0a\x0c\x27\x84\x9e\xd0\x5e\x46\x1f\x66\x1a\xf7\x51\x99\x0e\x4d\x23\x0a\x34\x95\x78\x6b\x38\x0a\xbe\xcd\x9a\x52\x62\x1c\x4d\xa7\xce\x64\xc9\xe9\x40\x6a\xfa\x1f\xc5\x09\x03\x1c\xf9\x61\x74\x2d\xa9\x89\x9c\x37\x83\xc5\xa4\x5d\xdb\x6b\x17\x3d\xfd\x8b\x13\xc6\x80\xd7\x5a\xec\xb2\xfa\x88\x17\x7e\x8c\x7e\x3e\xc4\x2c\xde\x30\x96\xa6\x29\x72\x47\x73\xb9\x07\xb3\xab\xf1\xc1\xd8\x33\xde\x17\x11\xbf\x66\xa2\x3a\xf0\x5a\x42\x54\x72\xff\x87\xdd\xaf\xec\xe4\xa1\x54\x8c\x03\x2f\x79\x2e\x31\x1f\x57\x2b\x6c\xeb\xfd\xee\x9e\x71\xe3\x36\xec\x2b\x00\x00\xff\xff\x0d\xe1\xbc\xbe\x73\x01\x00\x00")

func _000019_drop_index_tableDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000019_drop_index_tableDownSql,
		"000019_drop_index_table.down.sql",
	)
}

func _000019_drop_index_tableDownSql() (*asset, error) {
	bytes, err := _000019_drop_index_tableDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000019_drop_index_table.down.sql", size: 371, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000019_drop_index_tableUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\xf0\xf4\x73\x71\x8d\x50\x28\xcb\xac\xca\x4c\x2d\x8a\x4f\xce\x29\x2d\x2e\x49\x2d\x8a\x2f\xce\x48\x2c\x4a\x89\xcf\xcc\x4b\x49\xad\xb0\xe6\x02\x04\x00\x00\xff\xff\x7b\x8a\xfd\xde\x27\x00\x00\x00")

func _000019_drop_index_tableUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000019_drop_index_tableUpSql,
		"000019_drop_index_table.up.sql",
	)
}

func _000019_drop_index_tableUpSql() (*asset, error) {
	bytes, err := _000019_drop_index_tableUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000019_drop_index_table.up.sql", size: 39, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000020_add_auto_update_to_cluster_tableDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\xcb\xac\xca\x4c\x2d\x8a\x4f\xce\x29\x2d\x2e\x49\x2d\x8a\xcf\xcc\x4b\xcb\x57\x70\x09\xf2\x0f\x50\x70\xf6\xf7\x09\xf5\xf5\x53\x48\x2c\x2d\xc9\x8f\x2f\x2d\x48\x49\x2c\x49\x8d\x4f\xcd\x4b\x4c\xca\x49\x4d\xb1\xe6\x02\x04\x00\x00\xff\xff\x0d\x05\xfa\xc6\x41\x00\x00\x00")

func _000020_add_auto_update_to_cluster_tableDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000020_add_auto_update_to_cluster_tableDownSql,
		"000020_add_auto_update_to_cluster_table.down.sql",
	)
}

func _000020_add_auto_update_to_cluster_tableDownSql() (*asset, error) {
	bytes, err := _000020_add_auto_update_to_cluster_tableDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000020_add_auto_update_to_cluster_table.down.sql", size: 65, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000020_add_auto_update_to_cluster_tableUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x04\xc0\xb1\x0a\x02\x31\x0c\x06\xe0\xdd\xa7\xf8\xdf\xc3\x29\x67\xe3\x14\xaf\x20\xbd\x39\x54\x2f\x42\xe1\x68\xa5\x26\x0e\x3e\xbd\x1f\x49\xe1\x3b\x0a\x2d\xc2\xf8\xb6\x5f\xb3\xa9\xcf\x23\x3e\x6e\x53\x5b\x7f\x0d\x50\x4a\xb8\x64\xd9\x6e\x2b\x6a\xf8\xd0\x78\xef\xd5\x4d\xad\xd7\xc7\x61\x3b\x96\x9c\x85\x69\x45\xe2\x2b\x6d\x52\xe0\x33\xec\x7c\xfa\x07\x00\x00\xff\xff\x54\xbb\x1d\x40\x55\x00\x00\x00")

func _000020_add_auto_update_to_cluster_tableUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000020_add_auto_update_to_cluster_tableUpSql,
		"000020_add_auto_update_to_cluster_table.up.sql",
	)
}

func _000020_add_auto_update_to_cluster_tableUpSql() (*asset, error) {
	bytes, err := _000020_add_auto_update_to_cluster_tableUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000020_add_auto_update_to_cluster_table.up.sql", size: 85, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000021_add_status_message_to_cluster_infoDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\xcb\xac\xca\x4c\x2d\x8a\x4f\xce\x29\x2d\x2e\x49\x2d\x8a\xcf\xcc\x4b\xcb\x57\x70\x09\xf2\x0f\x50\x70\xf6\xf7\x09\xf5\xf5\x53\x28\x2e\x49\x2c\x29\x2d\x8e\xcf\x4d\x2d\x2e\x4e\x4c\x4f\xb5\xe6\x02\x04\x00\x00\xff\xff\x75\xde\x86\xcf\x3c\x00\x00\x00")

func _000021_add_status_message_to_cluster_infoDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000021_add_status_message_to_cluster_infoDownSql,
		"000021_add_status_message_to_cluster_info.down.sql",
	)
}

func _000021_add_status_message_to_cluster_infoDownSql() (*asset, error) {
	bytes, err := _000021_add_status_message_to_cluster_infoDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000021_add_status_message_to_cluster_info.down.sql", size: 60, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000021_add_status_message_to_cluster_infoUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\xcb\xac\xca\x4c\x2d\x8a\x4f\xce\x29\x2d\x2e\x49\x2d\x8a\xcf\xcc\x4b\xcb\xe7\x72\x74\x71\x51\x70\xf6\xf7\x09\xf5\xf5\x53\x28\x2e\x49\x2c\x29\x2d\x8e\xcf\x4d\x2d\x2e\x4e\x4c\x4f\x55\x28\x49\xad\x28\xb1\xe6\x02\x04\x00\x00\xff\xff\x02\x3f\xe2\x84\x40\x00\x00\x00")

func _000021_add_status_message_to_cluster_infoUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000021_add_status_message_to_cluster_infoUpSql,
		"000021_add_status_message_to_cluster_info.up.sql",
	)
}

func _000021_add_status_message_to_cluster_infoUpSql() (*asset, error) {
	bytes, err := _000021_add_status_message_to_cluster_infoUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000021_add_status_message_to_cluster_info.up.sql", size: 64, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000022_add_data_plane_and_past_status_columnsDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\xac\xcc\xb1\x0a\xc2\x30\x10\x80\xe1\xbd\x4f\x71\xef\xd1\x29\x6a\x06\x21\xb1\xa5\xc6\xf9\x08\xf6\xa4\x91\x98\x84\xdc\xa5\xa0\x4f\xef\xa0\x8b\xa3\xd0\xf1\x87\x9f\x4f\x19\xa7\x27\x70\x6a\x67\x34\xac\xe1\x15\xa8\xe2\x35\x36\x16\xaa\x18\xd2\x2d\xc3\x61\x1a\x46\xd8\x0f\xe6\x62\x4f\xd0\xd2\x42\x3e\xca\xf2\xc4\xd9\x8b\xc7\x12\x7d\x22\x2c\x79\x46\x16\x2f\x8d\x89\xe1\xce\x39\xf5\xdd\x3f\x66\xa9\xb4\x86\xdc\x18\xbf\xe3\x87\x82\x9f\xda\x42\x44\x09\x0f\x02\x77\xb4\xfa\xec\x94\x1d\xfb\xee\x1d\x00\x00\xff\xff\x6d\x3c\x0e\x94\xf9\x00\x00\x00")

func _000022_add_data_plane_and_past_status_columnsDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000022_add_data_plane_and_past_status_columnsDownSql,
		"000022_add_data_plane_and_past_status_columns.down.sql",
	)
}

func _000022_add_data_plane_and_past_status_columnsDownSql() (*asset, error) {
	bytes, err := _000022_add_data_plane_and_past_status_columnsDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000022_add_data_plane_and_past_status_columns.down.sql", size: 249, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000022_add_data_plane_and_past_status_columnsUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\xac\xcd\xc1\xaa\xc2\x30\x10\x85\xe1\x7d\x9e\x62\x76\x7d\x88\xae\x72\x6f\x23\x08\x69\x2b\x9a\xae\x87\x60\x47\x1a\xa9\x49\xc9\x4c\x0a\x2a\xbe\xbb\x0b\xdd\xb8\x14\x5c\x1e\x38\xfc\x9f\xb6\xce\xec\xc1\xe9\x3f\x6b\x60\x0d\xb7\x40\x19\x8f\x73\x61\xa1\x8c\x21\x9e\x92\xd2\x4d\x03\xff\xbd\x1d\xda\x0e\x4a\x9c\xc8\xcf\x32\x5d\x71\xf4\xe2\x71\x99\x7d\x24\x5c\xd2\x88\x2c\x5e\x0a\x13\xc3\x99\x53\x84\xae\x77\xd0\x0d\xd6\x42\x63\x36\x7a\xb0\x0e\xaa\xfb\xa3\xaa\x95\xfa\x42\x5a\x32\xad\x21\x15\xc6\xf7\xef\x05\xc0\xc7\xfa\x45\x11\x25\x5c\x08\xdc\xb6\x35\x07\xa7\xdb\x5d\xad\x9e\x01\x00\x00\xff\xff\x7b\x1f\x97\x8a\x0e\x01\x00\x00")

func _000022_add_data_plane_and_past_status_columnsUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000022_add_data_plane_and_past_status_columnsUpSql,
		"000022_add_data_plane_and_past_status_columns.up.sql",
	)
}

func _000022_add_data_plane_and_past_status_columnsUpSql() (*asset, error) {
	bytes, err := _000022_add_data_plane_and_past_status_columnsUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000022_add_data_plane_and_past_status_columns.up.sql", size: 270, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000023_add_triggers_for_prevDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\x08\x09\xf2\x74\x77\x77\x0d\x52\xf0\x74\x53\x70\x8d\xf0\x0c\x0e\x09\x56\x28\x2d\x48\x49\x2c\x49\x8d\x2f\xcb\xac\xca\x4c\x2d\x8a\x4f\xce\x29\x2d\x2e\x49\x2d\x8a\xcf\xcc\x4b\xcb\x8f\x2f\x28\x4a\x2d\x8b\x2f\x2e\x49\x2c\x49\xe5\x52\x50\xf0\xf7\x53\xc0\xa2\xc6\x9a\x8b\xcb\xd1\x27\xc4\x35\x48\x21\xc4\xd1\xc9\xc7\x15\x9b\x0a\x05\xb0\xbd\xce\xfe\x3e\xa1\xbe\x7e\x0a\x70\x13\x4b\x8b\xad\xc9\xd5\x18\x5f\x92\x99\x9b\x4a\xbe\xee\xdc\xd4\xe2\xe2\xc4\xf4\x54\x6b\x2e\x40\x00\x00\x00\xff\xff\x6e\x0a\x0d\x1d\x10\x01\x00\x00")

func _000023_add_triggers_for_prevDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000023_add_triggers_for_prevDownSql,
		"000023_add_triggers_for_prev.down.sql",
	)
}

func _000023_add_triggers_for_prevDownSql() (*asset, error) {
	bytes, err := _000023_add_triggers_for_prevDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000023_add_triggers_for_prev.down.sql", size: 272, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000023_add_triggers_for_prevUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\xa4\x91\xcd\x6a\xdc\x30\x14\x85\xf7\x7a\x8a\xc3\x60\x48\xbb\x48\x5f\xc0\x6d\x41\xb1\xaf\x27\x06\x8f\x64\x34\x32\xc9\xce\xa8\x13\x75\x62\x50\x66\x5c\x4b\x4e\xa1\x4f\x5f\xfc\xd7\xba\x10\x6f\x9a\x95\x91\xef\x39\x1f\xd2\x77\x6f\x6f\xa1\x9f\xad\xb7\x38\x5d\x9d\xc7\xcf\xc6\x39\x74\xb6\x75\xe6\x64\xb1\x6b\x3b\xfb\xda\x5c\x7b\x5f\xbf\x36\xbf\x1a\xdb\xd5\x3e\x98\xd0\xfb\x1d\xcc\xe5\x69\x6b\x58\x87\xe6\xc5\xee\x18\x2f\x34\x29\x68\x7e\x57\x10\xe6\xf9\xc9\xf5\x3e\xd8\xae\x6e\x2e\xdf\xaf\x0c\xe0\x69\x8a\x44\x16\xd5\x41\x60\x20\xcd\x75\xfc\x03\x8b\xd9\x7f\x83\xc6\x7b\x40\xe7\x07\x3a\x6a\x7e\x28\xdf\x43\x7a\xb1\xde\x9b\xb3\x85\xa6\x47\x1d\x33\x96\x28\xe2\x9a\x20\x15\x14\x95\x05\x4f\x08\x59\x25\x12\x9d\x4b\x81\xbe\x7d\x32\xc1\xd6\x7f\xca\xf6\xc3\x47\x06\x28\xd2\x95\x12\x47\x68\x95\xef\xf7\xa4\xc0\x8f\x88\x22\x06\xdc\xd1\x3e\x17\x0c\x00\xf2\x0c\x82\x1e\x3e\xcd\x0a\x3e\x7f\x85\x2c\xd2\xe5\xa4\xef\x69\x0a\x01\x63\x68\x2d\xeb\xcb\x2a\x18\x6f\x84\x26\x11\x53\xd2\x19\x1f\xea\x67\x6b\xba\xf0\xcd\x9a\xb0\xd9\x58\x1e\xbc\xc6\x2f\x3f\xa7\x12\x89\x14\x79\x16\xb3\xf1\x30\xbd\x6f\xa0\x0c\x43\x12\xe9\xf0\x89\x22\x38\x73\x39\xf7\x03\xe7\xa6\x75\xed\xd9\xff\x70\x37\x7f\xed\x2d\x2e\x66\x63\x6f\xec\x63\x65\x71\x74\x95\x49\x45\xa8\xca\x74\x74\x2f\x36\x36\x98\x49\x05\xe2\xc9\x3d\x94\x7c\x00\x3d\x52\x52\x69\x42\xa9\x64\x42\x69\xa5\xe8\xad\xfd\xc4\xec\x77\x00\x00\x00\xff\xff\x3e\x1f\xb7\x5e\x01\x03\x00\x00")

func _000023_add_triggers_for_prevUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000023_add_triggers_for_prevUpSql,
		"000023_add_triggers_for_prev.up.sql",
	)
}

func _000023_add_triggers_for_prevUpSql() (*asset, error) {
	bytes, err := _000023_add_triggers_for_prevUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000023_add_triggers_for_prev.up.sql", size: 769, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000024_add_encrypted_deployment_keyDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\xf0\xf4\x73\x71\x8d\x50\xc8\x4c\xa9\x88\x2f\xcb\xac\xca\x4c\x2d\x8a\x4f\x49\x2d\xc8\xc9\xaf\xcc\x4d\xcd\x2b\x89\xcf\x4e\xad\x2c\x8e\xcf\x48\x2c\xce\x48\x4d\x01\xb1\xad\xb9\xb8\x1c\x7d\x42\x5c\x83\x14\x42\x1c\x9d\x7c\x5c\x15\xb0\xab\xe7\x52\x50\x00\x9b\xeb\xec\xef\x13\xea\xeb\xa7\x40\x99\xee\xd4\xbc\xe4\xa2\xca\x82\x12\x98\x01\x80\x00\x00\x00\xff\xff\xa6\xe5\x49\x3f\xb1\x00\x00\x00")

func _000024_add_encrypted_deployment_keyDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000024_add_encrypted_deployment_keyDownSql,
		"000024_add_encrypted_deployment_key.down.sql",
	)
}

func _000024_add_encrypted_deployment_keyDownSql() (*asset, error) {
	bytes, err := _000024_add_encrypted_deployment_keyDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000024_add_encrypted_deployment_key.down.sql", size: 177, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000024_add_encrypted_deployment_keyUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x94\x8e\xc1\x4a\xc4\x30\x10\x86\xef\x79\x8a\xff\xa8\x87\xf5\x05\xf6\x14\xb7\x01\x85\xda\x85\xa5\x82\xb7\x30\x36\x23\x0d\x5b\x93\x92\x99\xad\xc6\xa7\x97\xe2\xa1\x08\x7a\xd8\xeb\x7c\x33\xf3\x7d\xb6\xed\xdd\x09\xbd\xbd\x6f\x1d\x96\xf8\x15\xb9\xf8\xc0\xf3\x94\xeb\x3b\x27\xf5\x67\xae\x62\x00\xdb\x34\x38\x1c\xdb\xe7\xa7\x0e\x9c\x86\x52\x67\xe5\xb0\x32\xbc\x56\x65\xda\x1b\xb3\xdb\xe1\x81\x64\xe4\x80\x75\x2a\x9a\x0b\x0b\x08\x42\x93\x72\x00\xa5\x80\x71\xc3\x3a\x92\xe2\x83\x31\x50\xc2\x45\x18\x6f\xb9\x80\x44\xf2\x10\x49\xe3\xc2\x98\x72\x3e\x5f\xe6\x3b\x73\x6d\xda\x8f\xe2\x57\xd7\xe1\xe4\x6c\xef\xf0\xd8\x35\xee\x05\x31\x7c\xfa\xbf\xff\xf8\xed\xd4\x00\xc7\xee\x1f\xdd\xcd\xb6\x76\xbb\x37\xdf\x01\x00\x00\xff\xff\xfa\x0f\x00\x58\x3a\x01\x00\x00")

func _000024_add_encrypted_deployment_keyUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000024_add_encrypted_deployment_keyUpSql,
		"000024_add_encrypted_deployment_key.up.sql",
	)
}

func _000024_add_encrypted_deployment_keyUpSql() (*asset, error) {
	bytes, err := _000024_add_encrypted_deployment_keyUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000024_add_encrypted_deployment_key.up.sql", size: 314, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000025_drop_deployment_key_oldDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\xcb\xac\xca\x4c\x2d\x8a\x4f\x49\x2d\xc8\xc9\xaf\xcc\x4d\xcd\x2b\x89\xcf\x4e\xad\x2c\xe6\x52\x50\x70\x74\x71\x51\x70\xf6\xf7\x09\xf5\xf5\x53\x50\xcf\x4e\xad\x54\x57\x28\x4b\x2c\x4a\xce\x48\x2c\xb2\xe6\x02\x04\x00\x00\xff\xff\xfb\xc3\xd3\x12\x3f\x00\x00\x00")

func _000025_drop_deployment_key_oldDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000025_drop_deployment_key_oldDownSql,
		"000025_drop_deployment_key_old.down.sql",
	)
}

func _000025_drop_deployment_key_oldDownSql() (*asset, error) {
	bytes, err := _000025_drop_deployment_key_oldDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000025_drop_deployment_key_old.down.sql", size: 63, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000025_drop_deployment_key_oldUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\xcb\xac\xca\x4c\x2d\x8a\x4f\x49\x2d\xc8\xc9\xaf\xcc\x4d\xcd\x2b\x89\xcf\x4e\xad\x2c\xe6\x52\x50\x70\x09\xf2\x0f\x50\x70\xf6\xf7\x09\xf5\xf5\x53\xc8\x4e\xad\xb4\xe6\x02\x04\x00\x00\xff\xff\x16\xc3\x47\xe1\x36\x00\x00\x00")

func _000025_drop_deployment_key_oldUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000025_drop_deployment_key_oldUpSql,
		"000025_drop_deployment_key_old.up.sql",
	)
}

func _000025_drop_deployment_key_oldUpSql() (*asset, error) {
	bytes, err := _000025_drop_deployment_key_oldUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000025_drop_deployment_key_old.up.sql", size: 54, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000026_remove_extraneous_columnsDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\xcb\xac\xca\x4c\x2d\x8a\x4f\xce\x29\x2d\x2e\x49\x2d\x8a\xcf\xcc\x4b\xcb\xe7\x72\x74\x71\x51\x70\xf6\xf7\x09\xf5\xf5\x53\x80\x89\xe7\x25\xe6\xa6\x2a\x94\x25\x16\x25\x67\x24\x16\x69\x18\x1a\x18\x18\x68\xea\x60\x53\x56\x9a\x99\x82\xaa\xca\x9a\x8b\x0b\xb7\x6d\xd8\x4c\x28\x4b\x2d\x2a\xce\xcc\xcf\x43\x37\x05\x10\x00\x00\xff\xff\xb0\x08\xa7\x04\xb3\x00\x00\x00")

func _000026_remove_extraneous_columnsDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000026_remove_extraneous_columnsDownSql,
		"000026_remove_extraneous_columns.down.sql",
	)
}

func _000026_remove_extraneous_columnsDownSql() (*asset, error) {
	bytes, err := _000026_remove_extraneous_columnsDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000026_remove_extraneous_columns.down.sql", size: 179, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000026_remove_extraneous_columnsUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\xcb\xac\xca\x4c\x2d\x8a\x4f\xce\x29\x2d\x2e\x49\x2d\x8a\xcf\xcc\x4b\xcb\xe7\x72\x09\xf2\x0f\x50\x70\xf6\xf7\x09\xf5\xf5\x53\x80\x49\xe4\x25\xe6\xa6\xea\x60\x95\x29\xcd\x4c\xb1\xe6\xe2\xc2\x6d\x26\x56\x4d\x65\xa9\x45\xc5\x99\xf9\x79\xd6\x5c\x80\x00\x00\x00\xff\xff\xcc\x45\xb0\xd9\x8c\x00\x00\x00")

func _000026_remove_extraneous_columnsUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000026_remove_extraneous_columnsUpSql,
		"000026_remove_extraneous_columns.up.sql",
	)
}

func _000026_remove_extraneous_columnsUpSql() (*asset, error) {
	bytes, err := _000026_remove_extraneous_columnsUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000026_remove_extraneous_columns.up.sql", size: 140, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000027_add_vizier_cluster_indicesDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\xf0\xf4\x73\x71\x8d\x50\xc8\x4c\xa9\x88\x2f\xcb\xac\xca\x4c\x2d\x8a\x4f\xce\x29\x2d\x2e\x41\xa2\x4b\x33\x53\xac\xb9\x88\x53\x9a\x97\x98\x9b\x4a\x48\x6d\x7e\x51\x7a\x3c\xc8\x44\x02\xca\x32\xf3\xd2\xf2\xe3\x8b\x4b\x12\x4b\x4a\x8b\x09\x99\x08\x56\x9a\x93\x58\x5c\x12\x9f\x91\x9a\x58\x54\x92\x94\x9a\x58\x62\xcd\x05\x08\x00\x00\xff\xff\x57\x0a\x51\xf1\xdc\x00\x00\x00")

func _000027_add_vizier_cluster_indicesDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000027_add_vizier_cluster_indicesDownSql,
		"000027_add_vizier_cluster_indices.down.sql",
	)
}

func _000027_add_vizier_cluster_indicesDownSql() (*asset, error) {
	bytes, err := _000027_add_vizier_cluster_indicesDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000027_add_vizier_cluster_indices.down.sql", size: 220, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000027_add_vizier_cluster_indicesUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x0e\x72\x75\x0c\x71\x55\xf0\xf4\x73\x71\x8d\x50\xc8\x4c\xa9\x88\x2f\xcb\xac\xca\x4c\x2d\x8a\x4f\xce\x29\x2d\x2e\x41\xa2\x4b\x33\x53\x14\xfc\xfd\x14\x50\x65\x35\x90\x64\x35\xad\xb9\x88\x35\x2b\x2f\x31\x37\x15\x8f\x61\x20\x69\x22\x4c\xcb\x2f\x4a\x8f\xc7\xea\x28\x88\x84\xa6\x35\x17\x41\x23\x32\xf3\xd2\xf2\xe3\x8b\x4b\x12\x4b\x4a\x8b\x31\xcd\x01\xcb\x6a\x40\x64\x89\x70\x0e\xd8\xac\x9c\xc4\xe2\x92\xf8\x8c\xd4\xc4\xa2\x92\xa4\xd4\xc4\x12\x5c\x66\xa2\xaa\xd2\xb4\xe6\x02\x04\x00\x00\xff\xff\xb7\x74\xdd\xfd\x85\x01\x00\x00")

func _000027_add_vizier_cluster_indicesUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000027_add_vizier_cluster_indicesUpSql,
		"000027_add_vizier_cluster_indices.up.sql",
	)
}

func _000027_add_vizier_cluster_indicesUpSql() (*asset, error) {
	bytes, err := _000027_add_vizier_cluster_indicesUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000027_add_vizier_cluster_indices.up.sql", size: 389, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000028_remove_previous_status_colsDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\xcb\xac\xca\x4c\x2d\x8a\x4f\xce\x29\x2d\x2e\x49\x2d\x8a\xcf\xcc\x4b\xcb\xe7\x52\x50\x70\x74\x71\x51\x70\xf6\xf7\x09\xf5\xf5\x53\x28\x28\x4a\x2d\xcb\xcc\x2f\x2d\x8e\x87\xaa\x2c\x2e\x49\x2c\x29\x2d\x56\x40\xe1\x59\x73\x71\x51\xc1\xcc\xf8\x92\xcc\xdc\x54\x85\x10\x4f\x5f\xd7\xe0\x10\x47\xdf\x00\x6b\x2e\x40\x00\x00\x00\xff\xff\x79\x45\x5a\x09\xa8\x00\x00\x00")

func _000028_remove_previous_status_colsDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000028_remove_previous_status_colsDownSql,
		"000028_remove_previous_status_cols.down.sql",
	)
}

func _000028_remove_previous_status_colsDownSql() (*asset, error) {
	bytes, err := _000028_remove_previous_status_colsDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000028_remove_previous_status_cols.down.sql", size: 168, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000028_remove_previous_status_colsUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\xcb\xac\xca\x4c\x2d\x8a\x4f\xce\x29\x2d\x2e\x49\x2d\x8a\xcf\xcc\x4b\xcb\xe7\x52\x50\x70\x09\xf2\x0f\x50\x70\xf6\xf7\x09\xf5\xf5\x53\x28\x28\x4a\x2d\xcb\xcc\x2f\x2d\x8e\x87\x2a\x2d\x2e\x49\x2c\x29\x2d\xb6\xe6\xe2\xa2\x86\x31\xf1\x25\x99\xb9\xa9\xd6\x5c\x80\x00\x00\x00\xff\xff\xb8\xcf\x71\xc8\x92\x00\x00\x00")

func _000028_remove_previous_status_colsUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000028_remove_previous_status_colsUpSql,
		"000028_remove_previous_status_cols.up.sql",
	)
}

func _000028_remove_previous_status_colsUpSql() (*asset, error) {
	bytes, err := _000028_remove_previous_status_colsUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000028_remove_previous_status_cols.up.sql", size: 146, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000029_remove_first_seenDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\xcb\xac\xca\x4c\x2d\x8a\x4f\xce\x29\x2d\x2e\x49\x2d\xe2\x52\x50\x70\x74\x71\x51\x70\xf6\xf7\x09\xf5\xf5\x53\x48\xcb\x2c\x2a\x2e\x89\x2f\x4e\x4d\xcd\x8b\x4f\x2c\x51\x08\xf1\xf4\x75\x0d\x0e\x71\xf4\x0d\xb0\xe6\x02\x04\x00\x00\xff\xff\xee\x41\x97\xf8\x41\x00\x00\x00")

func _000029_remove_first_seenDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000029_remove_first_seenDownSql,
		"000029_remove_first_seen.down.sql",
	)
}

func _000029_remove_first_seenDownSql() (*asset, error) {
	bytes, err := _000029_remove_first_seenDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000029_remove_first_seen.down.sql", size: 65, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000029_remove_first_seenUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\xcb\xac\xca\x4c\x2d\x8a\x4f\xce\x29\x2d\x2e\x49\x2d\xe2\x52\x50\x70\x09\xf2\x0f\x50\x70\xf6\xf7\x09\xf5\xf5\x53\x48\xcb\x2c\x2a\x2e\x89\x2f\x4e\x4d\xcd\x8b\x4f\x2c\xb1\xe6\x02\x04\x00\x00\xff\xff\xc3\x6a\xb9\xb0\x38\x00\x00\x00")

func _000029_remove_first_seenUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000029_remove_first_seenUpSql,
		"000029_remove_first_seen.up.sql",
	)
}

func _000029_remove_first_seenUpSql() (*asset, error) {
	bytes, err := _000029_remove_first_seenUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000029_remove_first_seen.up.sql", size: 56, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000030_add_degraded_cluster_status_enumDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\x08\x89\x0c\x70\x55\x28\xcb\xac\xca\x4c\x2d\x8a\x2f\x2e\x49\x2c\x29\x2d\xb6\xe6\x72\x0e\x72\x75\x0c\x71\xc5\x22\xa5\xe0\x18\xac\xe0\xea\x17\xea\xab\xa0\xa1\x1e\xea\xe7\xed\xe7\x1f\xee\xa7\xae\xa3\xa0\xee\xe1\xea\xe8\x13\xe2\x11\x09\x62\x86\xfa\x21\x71\x5c\x3c\x83\x9d\xfd\xfd\xfc\x5c\x9d\x43\x5c\x5d\xc0\x92\x01\x2e\x8e\x21\x9e\x7e\xee\x20\x36\xa6\x84\x6b\xbc\x9b\xa3\xa7\x8f\xab\x8b\xba\xa6\x35\x17\x20\x00\x00\xff\xff\xd6\x55\xa4\x36\x9a\x00\x00\x00")

func _000030_add_degraded_cluster_status_enumDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000030_add_degraded_cluster_status_enumDownSql,
		"000030_add_degraded_cluster_status_enum.down.sql",
	)
}

func _000030_add_degraded_cluster_status_enumDownSql() (*asset, error) {
	bytes, err := _000030_add_degraded_cluster_status_enumDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000030_add_degraded_cluster_status_enum.down.sql", size: 154, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000030_add_degraded_cluster_status_enumUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x89\x0c\x70\x55\x28\xcb\xac\xca\x4c\x2d\x8a\x2f\x2e\x49\x2c\x29\x2d\x56\x70\x74\x71\x51\x08\x73\xf4\x09\x75\x55\x50\x77\x71\x75\x0f\x72\x74\x71\x75\x51\x57\x70\x74\x03\xa9\x56\x0f\x0d\x70\x71\x0c\x71\x8d\x77\x73\xf4\xf4\x71\x75\x51\xb7\xe6\x02\x04\x00\x00\xff\xff\xaf\xeb\xf4\x7c\x45\x00\x00\x00")

func _000030_add_degraded_cluster_status_enumUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000030_add_degraded_cluster_status_enumUpSql,
		"000030_add_degraded_cluster_status_enum.up.sql",
	)
}

func _000030_add_degraded_cluster_status_enumUpSql() (*asset, error) {
	bytes, err := _000030_add_degraded_cluster_status_enumUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000030_add_degraded_cluster_status_enum.up.sql", size: 69, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000031_add_operator_version_columnDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\xcb\xac\xca\x4c\x2d\x8a\x4f\xce\x29\x2d\x2e\x49\x2d\x8a\xcf\xcc\x4b\xcb\xe7\x52\x50\x70\x09\xf2\x0f\x50\x70\xf6\xf7\x09\xf5\xf5\x53\xc8\x2f\x48\x2d\x4a\x2c\xc9\x2f\x8a\x2f\x4b\x2d\x2a\xce\xcc\xcf\xb3\xe6\x02\x04\x00\x00\xff\xff\x51\x18\x6d\x17\x40\x00\x00\x00")

func _000031_add_operator_version_columnDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000031_add_operator_version_columnDownSql,
		"000031_add_operator_version_column.down.sql",
	)
}

func _000031_add_operator_version_columnDownSql() (*asset, error) {
	bytes, err := _000031_add_operator_version_columnDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000031_add_operator_version_column.down.sql", size: 64, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000031_add_operator_version_columnUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\xcb\xac\xca\x4c\x2d\x8a\x4f\xce\x29\x2d\x2e\x49\x2d\x8a\xcf\xcc\x4b\xcb\xe7\x52\x50\x70\x74\x71\x51\x70\xf6\xf7\x09\xf5\xf5\x53\xc8\x2f\x48\x2d\x4a\x2c\xc9\x2f\x8a\x2f\x4b\x2d\x2a\xce\xcc\xcf\x53\x28\x4b\x2c\x4a\xce\x48\x2c\xd2\x30\x34\x30\x30\xd0\xb4\xe6\x02\x04\x00\x00\xff\xff\xc8\x89\x3d\xf8\x4d\x00\x00\x00")

func _000031_add_operator_version_columnUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000031_add_operator_version_columnUpSql,
		"000031_add_operator_version_column.up.sql",
	)
}

func _000031_add_operator_version_columnUpSql() (*asset, error) {
	bytes, err := _000031_add_operator_version_columnUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000031_add_operator_version_column.up.sql", size: 77, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
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
	"000001_create_cluster_tables.down.sql":                  _000001_create_cluster_tablesDownSql,
	"000001_create_cluster_tables.up.sql":                    _000001_create_cluster_tablesUpSql,
	"000002_create_pgcrypto_extension.down.sql":              _000002_create_pgcrypto_extensionDownSql,
	"000002_create_pgcrypto_extension.up.sql":                _000002_create_pgcrypto_extensionUpSql,
	"000003_create_index_table.down.sql":                     _000003_create_index_tableDownSql,
	"000003_create_index_table.up.sql":                       _000003_create_index_tableUpSql,
	"000004_create_shard_index.down.sql":                     _000004_create_shard_indexDownSql,
	"000004_create_shard_index.up.sql":                       _000004_create_shard_indexUpSql,
	"000005_add_passthrough_to_cluster_table.down.sql":       _000005_add_passthrough_to_cluster_tableDownSql,
	"000005_add_passthrough_to_cluster_table.up.sql":         _000005_add_passthrough_to_cluster_tableUpSql,
	"000006_add_project_id_to_cluster_table.down.sql":        _000006_add_project_id_to_cluster_tableDownSql,
	"000006_add_project_id_to_cluster_table.up.sql":          _000006_add_project_id_to_cluster_tableUpSql,
	"000007_update_cluster_status_enum.down.sql":             _000007_update_cluster_status_enumDownSql,
	"000007_update_cluster_status_enum.up.sql":               _000007_update_cluster_status_enumUpSql,
	"000008_connected_cluster_status_enum.down.sql":          _000008_connected_cluster_status_enumDownSql,
	"000008_connected_cluster_status_enum.up.sql":            _000008_connected_cluster_status_enumUpSql,
	"000009_update_failed_cluster_status_enum.down.sql":      _000009_update_failed_cluster_status_enumDownSql,
	"000009_update_failed_cluster_status_enum.up.sql":        _000009_update_failed_cluster_status_enumUpSql,
	"000010_add_vizier_info_to_cluster_info.down.sql":        _000010_add_vizier_info_to_cluster_infoDownSql,
	"000010_add_vizier_info_to_cluster_info.up.sql":          _000010_add_vizier_info_to_cluster_infoUpSql,
	"000011_add_updated_at_to_vizier_cluster.down.sql":       _000011_add_updated_at_to_vizier_clusterDownSql,
	"000011_add_updated_at_to_vizier_cluster.up.sql":         _000011_add_updated_at_to_vizier_clusterUpSql,
	"000012_add_deployment_keys.down.sql":                    _000012_add_deployment_keysDownSql,
	"000012_add_deployment_keys.up.sql":                      _000012_add_deployment_keysUpSql,
	"000013_add_desc_to_deploy_key.down.sql":                 _000013_add_desc_to_deploy_keyDownSql,
	"000013_add_desc_to_deploy_key.up.sql":                   _000013_add_desc_to_deploy_keyUpSql,
	"000014_add_unique_org_name_constraint.down.sql":         _000014_add_unique_org_name_constraintDownSql,
	"000014_add_unique_org_name_constraint.up.sql":           _000014_add_unique_org_name_constraintUpSql,
	"000015_move_cluster_info_columns.down.sql":              _000015_move_cluster_info_columnsDownSql,
	"000015_move_cluster_info_columns.up.sql":                _000015_move_cluster_info_columnsUpSql,
	"000016_trim_cluster_name.down.sql":                      _000016_trim_cluster_nameDownSql,
	"000016_trim_cluster_name.up.sql":                        _000016_trim_cluster_nameUpSql,
	"000017_add_pod_status_to_vizier_cluster.down.sql":       _000017_add_pod_status_to_vizier_clusterDownSql,
	"000017_add_pod_status_to_vizier_cluster.up.sql":         _000017_add_pod_status_to_vizier_clusterUpSql,
	"000018_add_num_nodes_to_cluster_table.down.sql":         _000018_add_num_nodes_to_cluster_tableDownSql,
	"000018_add_num_nodes_to_cluster_table.up.sql":           _000018_add_num_nodes_to_cluster_tableUpSql,
	"000019_drop_index_table.down.sql":                       _000019_drop_index_tableDownSql,
	"000019_drop_index_table.up.sql":                         _000019_drop_index_tableUpSql,
	"000020_add_auto_update_to_cluster_table.down.sql":       _000020_add_auto_update_to_cluster_tableDownSql,
	"000020_add_auto_update_to_cluster_table.up.sql":         _000020_add_auto_update_to_cluster_tableUpSql,
	"000021_add_status_message_to_cluster_info.down.sql":     _000021_add_status_message_to_cluster_infoDownSql,
	"000021_add_status_message_to_cluster_info.up.sql":       _000021_add_status_message_to_cluster_infoUpSql,
	"000022_add_data_plane_and_past_status_columns.down.sql": _000022_add_data_plane_and_past_status_columnsDownSql,
	"000022_add_data_plane_and_past_status_columns.up.sql":   _000022_add_data_plane_and_past_status_columnsUpSql,
	"000023_add_triggers_for_prev.down.sql":                  _000023_add_triggers_for_prevDownSql,
	"000023_add_triggers_for_prev.up.sql":                    _000023_add_triggers_for_prevUpSql,
	"000024_add_encrypted_deployment_key.down.sql":           _000024_add_encrypted_deployment_keyDownSql,
	"000024_add_encrypted_deployment_key.up.sql":             _000024_add_encrypted_deployment_keyUpSql,
	"000025_drop_deployment_key_old.down.sql":                _000025_drop_deployment_key_oldDownSql,
	"000025_drop_deployment_key_old.up.sql":                  _000025_drop_deployment_key_oldUpSql,
	"000026_remove_extraneous_columns.down.sql":              _000026_remove_extraneous_columnsDownSql,
	"000026_remove_extraneous_columns.up.sql":                _000026_remove_extraneous_columnsUpSql,
	"000027_add_vizier_cluster_indices.down.sql":             _000027_add_vizier_cluster_indicesDownSql,
	"000027_add_vizier_cluster_indices.up.sql":               _000027_add_vizier_cluster_indicesUpSql,
	"000028_remove_previous_status_cols.down.sql":            _000028_remove_previous_status_colsDownSql,
	"000028_remove_previous_status_cols.up.sql":              _000028_remove_previous_status_colsUpSql,
	"000029_remove_first_seen.down.sql":                      _000029_remove_first_seenDownSql,
	"000029_remove_first_seen.up.sql":                        _000029_remove_first_seenUpSql,
	"000030_add_degraded_cluster_status_enum.down.sql":       _000030_add_degraded_cluster_status_enumDownSql,
	"000030_add_degraded_cluster_status_enum.up.sql":         _000030_add_degraded_cluster_status_enumUpSql,
	"000031_add_operator_version_column.down.sql":            _000031_add_operator_version_columnDownSql,
	"000031_add_operator_version_column.up.sql":              _000031_add_operator_version_columnUpSql,
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
	"000001_create_cluster_tables.down.sql":                  &bintree{_000001_create_cluster_tablesDownSql, map[string]*bintree{}},
	"000001_create_cluster_tables.up.sql":                    &bintree{_000001_create_cluster_tablesUpSql, map[string]*bintree{}},
	"000002_create_pgcrypto_extension.down.sql":              &bintree{_000002_create_pgcrypto_extensionDownSql, map[string]*bintree{}},
	"000002_create_pgcrypto_extension.up.sql":                &bintree{_000002_create_pgcrypto_extensionUpSql, map[string]*bintree{}},
	"000003_create_index_table.down.sql":                     &bintree{_000003_create_index_tableDownSql, map[string]*bintree{}},
	"000003_create_index_table.up.sql":                       &bintree{_000003_create_index_tableUpSql, map[string]*bintree{}},
	"000004_create_shard_index.down.sql":                     &bintree{_000004_create_shard_indexDownSql, map[string]*bintree{}},
	"000004_create_shard_index.up.sql":                       &bintree{_000004_create_shard_indexUpSql, map[string]*bintree{}},
	"000005_add_passthrough_to_cluster_table.down.sql":       &bintree{_000005_add_passthrough_to_cluster_tableDownSql, map[string]*bintree{}},
	"000005_add_passthrough_to_cluster_table.up.sql":         &bintree{_000005_add_passthrough_to_cluster_tableUpSql, map[string]*bintree{}},
	"000006_add_project_id_to_cluster_table.down.sql":        &bintree{_000006_add_project_id_to_cluster_tableDownSql, map[string]*bintree{}},
	"000006_add_project_id_to_cluster_table.up.sql":          &bintree{_000006_add_project_id_to_cluster_tableUpSql, map[string]*bintree{}},
	"000007_update_cluster_status_enum.down.sql":             &bintree{_000007_update_cluster_status_enumDownSql, map[string]*bintree{}},
	"000007_update_cluster_status_enum.up.sql":               &bintree{_000007_update_cluster_status_enumUpSql, map[string]*bintree{}},
	"000008_connected_cluster_status_enum.down.sql":          &bintree{_000008_connected_cluster_status_enumDownSql, map[string]*bintree{}},
	"000008_connected_cluster_status_enum.up.sql":            &bintree{_000008_connected_cluster_status_enumUpSql, map[string]*bintree{}},
	"000009_update_failed_cluster_status_enum.down.sql":      &bintree{_000009_update_failed_cluster_status_enumDownSql, map[string]*bintree{}},
	"000009_update_failed_cluster_status_enum.up.sql":        &bintree{_000009_update_failed_cluster_status_enumUpSql, map[string]*bintree{}},
	"000010_add_vizier_info_to_cluster_info.down.sql":        &bintree{_000010_add_vizier_info_to_cluster_infoDownSql, map[string]*bintree{}},
	"000010_add_vizier_info_to_cluster_info.up.sql":          &bintree{_000010_add_vizier_info_to_cluster_infoUpSql, map[string]*bintree{}},
	"000011_add_updated_at_to_vizier_cluster.down.sql":       &bintree{_000011_add_updated_at_to_vizier_clusterDownSql, map[string]*bintree{}},
	"000011_add_updated_at_to_vizier_cluster.up.sql":         &bintree{_000011_add_updated_at_to_vizier_clusterUpSql, map[string]*bintree{}},
	"000012_add_deployment_keys.down.sql":                    &bintree{_000012_add_deployment_keysDownSql, map[string]*bintree{}},
	"000012_add_deployment_keys.up.sql":                      &bintree{_000012_add_deployment_keysUpSql, map[string]*bintree{}},
	"000013_add_desc_to_deploy_key.down.sql":                 &bintree{_000013_add_desc_to_deploy_keyDownSql, map[string]*bintree{}},
	"000013_add_desc_to_deploy_key.up.sql":                   &bintree{_000013_add_desc_to_deploy_keyUpSql, map[string]*bintree{}},
	"000014_add_unique_org_name_constraint.down.sql":         &bintree{_000014_add_unique_org_name_constraintDownSql, map[string]*bintree{}},
	"000014_add_unique_org_name_constraint.up.sql":           &bintree{_000014_add_unique_org_name_constraintUpSql, map[string]*bintree{}},
	"000015_move_cluster_info_columns.down.sql":              &bintree{_000015_move_cluster_info_columnsDownSql, map[string]*bintree{}},
	"000015_move_cluster_info_columns.up.sql":                &bintree{_000015_move_cluster_info_columnsUpSql, map[string]*bintree{}},
	"000016_trim_cluster_name.down.sql":                      &bintree{_000016_trim_cluster_nameDownSql, map[string]*bintree{}},
	"000016_trim_cluster_name.up.sql":                        &bintree{_000016_trim_cluster_nameUpSql, map[string]*bintree{}},
	"000017_add_pod_status_to_vizier_cluster.down.sql":       &bintree{_000017_add_pod_status_to_vizier_clusterDownSql, map[string]*bintree{}},
	"000017_add_pod_status_to_vizier_cluster.up.sql":         &bintree{_000017_add_pod_status_to_vizier_clusterUpSql, map[string]*bintree{}},
	"000018_add_num_nodes_to_cluster_table.down.sql":         &bintree{_000018_add_num_nodes_to_cluster_tableDownSql, map[string]*bintree{}},
	"000018_add_num_nodes_to_cluster_table.up.sql":           &bintree{_000018_add_num_nodes_to_cluster_tableUpSql, map[string]*bintree{}},
	"000019_drop_index_table.down.sql":                       &bintree{_000019_drop_index_tableDownSql, map[string]*bintree{}},
	"000019_drop_index_table.up.sql":                         &bintree{_000019_drop_index_tableUpSql, map[string]*bintree{}},
	"000020_add_auto_update_to_cluster_table.down.sql":       &bintree{_000020_add_auto_update_to_cluster_tableDownSql, map[string]*bintree{}},
	"000020_add_auto_update_to_cluster_table.up.sql":         &bintree{_000020_add_auto_update_to_cluster_tableUpSql, map[string]*bintree{}},
	"000021_add_status_message_to_cluster_info.down.sql":     &bintree{_000021_add_status_message_to_cluster_infoDownSql, map[string]*bintree{}},
	"000021_add_status_message_to_cluster_info.up.sql":       &bintree{_000021_add_status_message_to_cluster_infoUpSql, map[string]*bintree{}},
	"000022_add_data_plane_and_past_status_columns.down.sql": &bintree{_000022_add_data_plane_and_past_status_columnsDownSql, map[string]*bintree{}},
	"000022_add_data_plane_and_past_status_columns.up.sql":   &bintree{_000022_add_data_plane_and_past_status_columnsUpSql, map[string]*bintree{}},
	"000023_add_triggers_for_prev.down.sql":                  &bintree{_000023_add_triggers_for_prevDownSql, map[string]*bintree{}},
	"000023_add_triggers_for_prev.up.sql":                    &bintree{_000023_add_triggers_for_prevUpSql, map[string]*bintree{}},
	"000024_add_encrypted_deployment_key.down.sql":           &bintree{_000024_add_encrypted_deployment_keyDownSql, map[string]*bintree{}},
	"000024_add_encrypted_deployment_key.up.sql":             &bintree{_000024_add_encrypted_deployment_keyUpSql, map[string]*bintree{}},
	"000025_drop_deployment_key_old.down.sql":                &bintree{_000025_drop_deployment_key_oldDownSql, map[string]*bintree{}},
	"000025_drop_deployment_key_old.up.sql":                  &bintree{_000025_drop_deployment_key_oldUpSql, map[string]*bintree{}},
	"000026_remove_extraneous_columns.down.sql":              &bintree{_000026_remove_extraneous_columnsDownSql, map[string]*bintree{}},
	"000026_remove_extraneous_columns.up.sql":                &bintree{_000026_remove_extraneous_columnsUpSql, map[string]*bintree{}},
	"000027_add_vizier_cluster_indices.down.sql":             &bintree{_000027_add_vizier_cluster_indicesDownSql, map[string]*bintree{}},
	"000027_add_vizier_cluster_indices.up.sql":               &bintree{_000027_add_vizier_cluster_indicesUpSql, map[string]*bintree{}},
	"000028_remove_previous_status_cols.down.sql":            &bintree{_000028_remove_previous_status_colsDownSql, map[string]*bintree{}},
	"000028_remove_previous_status_cols.up.sql":              &bintree{_000028_remove_previous_status_colsUpSql, map[string]*bintree{}},
	"000029_remove_first_seen.down.sql":                      &bintree{_000029_remove_first_seenDownSql, map[string]*bintree{}},
	"000029_remove_first_seen.up.sql":                        &bintree{_000029_remove_first_seenUpSql, map[string]*bintree{}},
	"000030_add_degraded_cluster_status_enum.down.sql":       &bintree{_000030_add_degraded_cluster_status_enumDownSql, map[string]*bintree{}},
	"000030_add_degraded_cluster_status_enum.up.sql":         &bintree{_000030_add_degraded_cluster_status_enumUpSql, map[string]*bintree{}},
	"000031_add_operator_version_column.down.sql":            &bintree{_000031_add_operator_version_columnDownSql, map[string]*bintree{}},
	"000031_add_operator_version_column.up.sql":              &bintree{_000031_add_operator_version_columnUpSql, map[string]*bintree{}},
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
