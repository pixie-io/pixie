// Code generated for package schema by go-bindata DO NOT EDIT. (@generated)
// sources:
// 000001_create_cron_scripts_table.down.sql
// 000001_create_cron_scripts_table.up.sql
// 000002_add_frequency_cron_scripts_table.down.sql
// 000002_add_frequency_cron_scripts_table.up.sql
// 000003_update_cluster_ids_cron_scripts_table.down.sql
// 000003_update_cluster_ids_cron_scripts_table.up.sql
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

var __000001_create_cron_scripts_tableDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\x08\x71\x74\xf2\x71\x55\xf0\x74\x53\x70\x8d\xf0\x0c\x0e\x09\x56\x48\x2e\xca\xcf\x8b\x2f\x4e\x2e\xca\x2c\x28\x29\xb6\xe6\x02\x04\x00\x00\xff\xff\x4e\x01\x55\x60\x23\x00\x00\x00")

func _000001_create_cron_scripts_tableDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000001_create_cron_scripts_tableDownSql,
		"000001_create_cron_scripts_table.down.sql",
	)
}

func _000001_create_cron_scripts_tableDownSql() (*asset, error) {
	bytes, err := _000001_create_cron_scripts_tableDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000001_create_cron_scripts_table.down.sql", size: 35, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000001_create_cron_scripts_tableUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x84\x92\x41\x6f\xda\x40\x10\x85\xef\xfe\x15\x4f\x39\x81\x04\xe9\xa5\x37\x4e\xb4\x38\x92\x55\x87\xa4\x60\x4b\x41\x55\x85\x16\x7b\xec\x5d\xd5\xec\x5a\x3b\x6b\x08\xff\xbe\x1a\x63\x37\x94\x1e\x7a\xf3\x7a\x66\xbe\xf7\xe6\x69\xbe\x6e\xe2\x65\x16\x23\x7e\xcb\xe2\xf5\x36\x79\x59\x23\x79\xc2\xfa\x25\x43\xfc\x96\x6c\xb3\x2d\x1e\xba\xce\x94\x73\xc7\xdc\x3e\x2c\xa2\xff\xf5\xb6\x75\xe1\x2f\x6d\x70\x0f\x8b\x68\xec\xcd\x96\x5f\xd2\x18\x85\x77\x76\xcf\x85\x37\x6d\x60\x4c\x22\x60\x3e\x47\xa6\x09\xc9\x0a\xae\x42\xd0\xd4\x77\xe0\xda\xf1\x18\x01\xa6\x44\x9e\x27\x2b\xe4\xeb\xe4\x7b\x1e\x63\x15\x3f\x2d\xf3\x34\x83\x98\xd9\xd7\x64\xc9\xab\x40\xfb\xd3\xe7\xc9\x74\x76\x85\x39\x5f\xef\x4d\x09\xc3\x3d\xcc\xf9\x1a\x67\xed\xa0\x95\xbc\x0d\x0f\x60\x90\x55\x87\x86\x4a\x11\x18\x06\x7a\x11\x59\x61\x9d\xa7\xe9\xc0\x2a\x9c\x0d\x64\x03\xf7\x1f\xca\xd8\x2b\x53\x15\xa1\x53\x0d\x5e\xdf\xd3\x1b\x9b\x03\xf7\xa4\x7c\xa1\x95\x1f\xe7\x65\x59\x7a\x6f\x3d\x31\x1b\xd9\xaa\xa5\xc2\x54\x86\x58\xec\x69\x77\x86\xab\x02\xd9\x1e\x3a\xcc\xb3\x76\x5d\x53\xc2\x77\x16\x1d\x1b\x5b\x43\x5d\xf3\xf8\x80\x88\xd8\x3d\xf7\x4e\xb5\xe9\x38\x90\xdf\x9b\x92\xc7\x18\x1a\xc3\x41\xf2\x1d\x4a\x8c\xb3\x36\x85\xfe\x2b\x91\x1b\x65\x67\x1f\x91\x54\xa0\x63\x1b\x2e\x33\x28\xe6\xee\x28\x96\x83\x14\x19\xce\x42\x35\xcd\x07\xca\xd8\x31\xe9\xde\xda\x8d\xb8\x44\xfa\xe3\xe7\xe0\x2a\xb6\x27\xe3\x9d\x3d\x92\xed\x53\x32\x92\xbf\x98\x53\x7f\xa4\x0f\x84\x8e\xa9\x44\x70\xa8\x4c\xd3\x8c\xe0\xab\xbf\x99\x3c\x15\x76\xcb\xe7\x14\x95\xf3\x47\xd5\xa7\x5e\x38\x5b\x99\x9a\x71\xb8\x04\x52\x83\x50\xe6\x7e\x91\x95\x9e\x7e\xba\x63\xf2\x9f\xe4\x0a\xe4\xc7\xbf\x5b\x1b\x1e\x36\xc6\x81\xb4\x6a\x2a\x81\x86\x1e\x70\x8b\x1c\xae\x45\xba\xcf\x9a\x82\x26\x7f\x7f\xa9\x52\xba\xb9\xa9\x71\xe0\xe0\x5c\x43\xca\xce\xa2\x08\x78\xdd\x24\xcf\xcb\xcd\x0e\xdf\xe2\x1d\x26\xa6\x9c\x46\xd3\x45\xf4\x3b\x00\x00\xff\xff\xbc\x82\xee\xfe\x73\x03\x00\x00")

func _000001_create_cron_scripts_tableUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000001_create_cron_scripts_tableUpSql,
		"000001_create_cron_scripts_table.up.sql",
	)
}

func _000001_create_cron_scripts_tableUpSql() (*asset, error) {
	bytes, err := _000001_create_cron_scripts_tableUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000001_create_cron_scripts_table.up.sql", size: 883, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000002_add_frequency_cron_scripts_tableDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x48\x2e\xca\xcf\x8b\x2f\x4e\x2e\xca\x2c\x28\x29\xe6\x52\x50\x70\x09\xf2\x0f\x50\x70\xf6\xf7\x09\xf5\xf5\x53\x48\x2b\x4a\x2d\x2c\x4d\xcd\x4b\xae\x8c\x2f\xb6\xe6\x02\x04\x00\x00\xff\xff\xe8\xf3\xcb\xac\x34\x00\x00\x00")

func _000002_add_frequency_cron_scripts_tableDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000002_add_frequency_cron_scripts_tableDownSql,
		"000002_add_frequency_cron_scripts_table.down.sql",
	)
}

func _000002_add_frequency_cron_scripts_tableDownSql() (*asset, error) {
	bytes, err := _000002_add_frequency_cron_scripts_tableDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000002_add_frequency_cron_scripts_table.down.sql", size: 52, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000002_add_frequency_cron_scripts_tableUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x48\x2e\xca\xcf\x8b\x2f\x4e\x2e\xca\x2c\x28\x29\xe6\x52\x50\x70\x74\x71\x51\x70\xf6\xf7\x09\xf5\xf5\x53\x48\x2b\x4a\x2d\x2c\x4d\xcd\x4b\xae\x8c\x2f\x56\xc8\xcc\x2b\x49\x4d\x4f\x2d\xb2\xe6\x02\x04\x00\x00\xff\xff\x6e\xae\x23\x51\x3b\x00\x00\x00")

func _000002_add_frequency_cron_scripts_tableUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000002_add_frequency_cron_scripts_tableUpSql,
		"000002_add_frequency_cron_scripts_table.up.sql",
	)
}

func _000002_add_frequency_cron_scripts_tableUpSql() (*asset, error) {
	bytes, err := _000002_add_frequency_cron_scripts_tableUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000002_add_frequency_cron_scripts_table.up.sql", size: 59, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000003_update_cluster_ids_cron_scripts_tableDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x48\x2e\xca\xcf\x8b\x2f\x4e\x2e\xca\x2c\x28\x29\x56\x70\x09\xf2\x0f\x50\x70\xf6\xf7\x09\xf5\xf5\x53\x48\xce\x29\x2d\x2e\x49\x2d\x8a\xcf\x4c\x29\xb6\xe6\xc2\xa9\xc3\xd1\xc5\x05\x8b\x06\x85\xd0\x50\x4f\x97\xe8\x58\x6b\x2e\x40\x00\x00\x00\xff\xff\xdb\xad\x01\x95\x6a\x00\x00\x00")

func _000003_update_cluster_ids_cron_scripts_tableDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000003_update_cluster_ids_cron_scripts_tableDownSql,
		"000003_update_cluster_ids_cron_scripts_table.down.sql",
	)
}

func _000003_update_cluster_ids_cron_scripts_tableDownSql() (*asset, error) {
	bytes, err := _000003_update_cluster_ids_cron_scripts_tableDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000003_update_cluster_ids_cron_scripts_table.down.sql", size: 106, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000003_update_cluster_ids_cron_scripts_tableUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x48\x2e\xca\xcf\x8b\x2f\x4e\x2e\xca\x2c\x28\x29\x56\x70\x09\xf2\x0f\x50\x70\xf6\xf7\x09\xf5\xf5\x53\x48\xce\x29\x2d\x2e\x49\x2d\x8a\xcf\x4c\x29\xb6\xe6\xc2\xa9\xc3\xd1\xc5\x05\x8b\x06\x85\xa4\xca\x92\xd4\x44\x6b\x2e\x40\x00\x00\x00\xff\xff\x44\x6f\x9d\xa6\x69\x00\x00\x00")

func _000003_update_cluster_ids_cron_scripts_tableUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000003_update_cluster_ids_cron_scripts_tableUpSql,
		"000003_update_cluster_ids_cron_scripts_table.up.sql",
	)
}

func _000003_update_cluster_ids_cron_scripts_tableUpSql() (*asset, error) {
	bytes, err := _000003_update_cluster_ids_cron_scripts_tableUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000003_update_cluster_ids_cron_scripts_table.up.sql", size: 105, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
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
	"000001_create_cron_scripts_table.down.sql":             _000001_create_cron_scripts_tableDownSql,
	"000001_create_cron_scripts_table.up.sql":               _000001_create_cron_scripts_tableUpSql,
	"000002_add_frequency_cron_scripts_table.down.sql":      _000002_add_frequency_cron_scripts_tableDownSql,
	"000002_add_frequency_cron_scripts_table.up.sql":        _000002_add_frequency_cron_scripts_tableUpSql,
	"000003_update_cluster_ids_cron_scripts_table.down.sql": _000003_update_cluster_ids_cron_scripts_tableDownSql,
	"000003_update_cluster_ids_cron_scripts_table.up.sql":   _000003_update_cluster_ids_cron_scripts_tableUpSql,
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
	"000001_create_cron_scripts_table.down.sql":             &bintree{_000001_create_cron_scripts_tableDownSql, map[string]*bintree{}},
	"000001_create_cron_scripts_table.up.sql":               &bintree{_000001_create_cron_scripts_tableUpSql, map[string]*bintree{}},
	"000002_add_frequency_cron_scripts_table.down.sql":      &bintree{_000002_add_frequency_cron_scripts_tableDownSql, map[string]*bintree{}},
	"000002_add_frequency_cron_scripts_table.up.sql":        &bintree{_000002_add_frequency_cron_scripts_tableUpSql, map[string]*bintree{}},
	"000003_update_cluster_ids_cron_scripts_table.down.sql": &bintree{_000003_update_cluster_ids_cron_scripts_tableDownSql, map[string]*bintree{}},
	"000003_update_cluster_ids_cron_scripts_table.up.sql":   &bintree{_000003_update_cluster_ids_cron_scripts_tableUpSql, map[string]*bintree{}},
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
