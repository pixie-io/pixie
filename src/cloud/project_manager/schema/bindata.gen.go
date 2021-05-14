// Code generated for package schema by go-bindata DO NOT EDIT. (@generated)
// sources:
// 000001_create_sites_table.down.sql
// 000001_create_sites_table.up.sql
// 000002_create_site_name_column.down.sql
// 000002_create_site_name_column.up.sql
// 000003_rename_sites_to_projects.down.sql
// 000003_rename_sites_to_projects.up.sql
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

var __000001_create_sites_tableDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\x08\x71\x74\xf2\x71\x55\xf0\x74\x53\x70\x8d\xf0\x0c\x0e\x09\x56\x28\xce\x2c\x49\x2d\xb6\xe6\x02\x04\x00\x00\xff\xff\xff\xc7\xee\xf6\x1c\x00\x00\x00")

func _000001_create_sites_tableDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000001_create_sites_tableDownSql,
		"000001_create_sites_table.down.sql",
	)
}

func _000001_create_sites_tableDownSql() (*asset, error) {
	bytes, err := _000001_create_sites_tableDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000001_create_sites_table.down.sql", size: 28, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000001_create_sites_tableUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x4c\xcc\x31\xcb\xc2\x30\x10\x87\xf1\x3d\x9f\xe2\x3f\xb6\xf0\xf6\xc5\xc5\xc9\xa9\x6a\x86\x82\x08\x4a\x33\x97\xc3\x9e\xe6\x86\x26\x7a\x49\xf5\xeb\x4b\xad\x05\xf7\xdf\xf3\xec\xce\xb6\x6e\x2d\xda\x7a\x7b\xb0\x48\x92\x39\xa1\x30\x40\x55\x21\xea\xad\x93\x1e\x92\x90\x3d\x23\xbe\x02\x2b\xe2\x15\xd9\x4b\xfa\xc0\x7f\x83\xc5\x38\xd7\xec\xff\xe6\xaa\x8f\x03\x49\xe8\x02\x0d\xbc\xa4\x63\x90\xc7\xc8\xb8\x93\xe6\x79\xc0\x5f\x85\x49\x4d\x9b\xdf\xe8\x49\x7a\xf1\xa4\xc5\x7a\x55\xc2\x1d\x9b\x93\xb3\xa6\xdc\x98\x77\x00\x00\x00\xff\xff\xe9\x89\x3b\xd2\xa7\x00\x00\x00")

func _000001_create_sites_tableUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000001_create_sites_tableUpSql,
		"000001_create_sites_table.up.sql",
	)
}

func _000001_create_sites_tableUpSql() (*asset, error) {
	bytes, err := _000001_create_sites_tableUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000001_create_sites_table.up.sql", size: 167, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000002_create_site_name_columnDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\xce\x2c\x49\x2d\xe6\x72\x74\x71\x51\x70\xf6\xf7\x09\xf5\xf5\x53\x48\xc9\xcf\x4d\xcc\xcc\x8b\xcf\x4b\xcc\x4d\x55\x08\x73\x0c\x72\xf6\x70\x0c\xd2\x30\x35\xd0\x54\x08\xf5\xf3\x0c\x0c\x75\xb5\xe6\xe2\x0a\x0d\x70\x71\x0c\x81\x69\x0c\x76\x0d\x41\xd1\x61\x0b\x16\x07\xb3\xad\xb9\xb8\x30\x2d\x72\x09\xf2\x0f\x50\x48\xce\xcf\x29\xcd\xcd\x43\x56\x09\x08\x00\x00\xff\xff\x5a\x48\xab\x8d\x92\x00\x00\x00")

func _000002_create_site_name_columnDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000002_create_site_name_columnDownSql,
		"000002_create_site_name_column.down.sql",
	)
}

func _000002_create_site_name_columnDownSql() (*asset, error) {
	bytes, err := _000002_create_site_name_columnDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000002_create_site_name_column.down.sql", size: 146, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000002_create_site_name_columnUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\xce\x2c\x49\x2d\xe6\x72\x74\x71\x51\x70\xf6\xf7\x09\xf5\xf5\x53\x00\x8b\xc4\xe7\x25\xe6\xa6\x2a\x84\x39\x06\x39\x7b\x38\x06\x69\x98\x1a\x68\x2a\x84\xfa\x79\x06\x86\xba\x5a\x73\x71\x85\x06\xb8\x38\x86\xc0\xf4\x05\xbb\x86\x20\xa9\xb7\x55\x48\xc9\xcf\x4d\xcc\xcc\x03\xf3\xac\xb9\x00\x01\x00\x00\xff\xff\x01\x56\xff\xed\x67\x00\x00\x00")

func _000002_create_site_name_columnUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000002_create_site_name_columnUpSql,
		"000002_create_site_name_column.up.sql",
	)
}

func _000002_create_site_name_columnUpSql() (*asset, error) {
	bytes, err := _000002_create_site_name_columnUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000002_create_site_name_column.up.sql", size: 103, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000003_rename_sites_to_projectsDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x34\xcc\xbd\xce\x82\x30\x18\xc5\xf1\xbd\x57\x71\x46\x48\x5e\xde\xb8\x38\x31\xa1\x74\x20\x31\x7e\x10\x3a\x93\x06\x1e\xa5\x26\xb4\xf8\xb4\xe8\xed\x1b\x2c\xac\x27\xbf\xf3\x2f\xeb\xcb\x15\x4d\x71\x38\x49\x4c\xec\x9e\xd4\x05\x9f\x0b\x71\xac\x65\xd1\xc8\x75\xf7\x26\x90\x47\x22\x80\x2c\x83\xe3\x47\x6b\x7a\x18\x8f\x30\x10\xdc\xc7\x12\xc3\xdd\x11\x06\xe3\x7f\xf0\x5f\x60\x33\x4a\x55\xe5\x5f\x7c\xf5\x6e\xd4\xc6\xb6\x56\x8f\xb4\x5d\x67\x6b\x5e\x33\x61\xd2\x1c\x62\x80\x56\x85\x45\x2d\x99\x25\x17\x2f\x6f\xcd\xdd\xa0\x39\xd9\xef\x52\xa8\x73\x75\x53\x52\xa4\xb9\xf8\x06\x00\x00\xff\xff\x82\x63\x67\x8a\xbb\x00\x00\x00")

func _000003_rename_sites_to_projectsDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000003_rename_sites_to_projectsDownSql,
		"000003_rename_sites_to_projects.down.sql",
	)
}

func _000003_rename_sites_to_projectsDownSql() (*asset, error) {
	bytes, err := _000003_rename_sites_to_projectsDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000003_rename_sites_to_projects.down.sql", size: 187, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000003_rename_sites_to_projectsUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\x08\x71\x74\xf2\x71\x55\x28\xce\x2c\x49\x2d\xb6\xe6\xe2\x72\x0e\x72\x75\x0c\x71\x85\x0a\x16\x14\xe5\x67\xa5\x26\x97\x14\x2b\x68\x70\x29\x28\xe8\xea\x2a\xe4\x17\xa5\xc7\x67\xa6\x28\x64\x16\x2b\x94\x64\xa4\x2a\xe4\x97\xe7\xa5\x16\x29\xe4\xa7\x29\x94\x64\x64\x16\xc3\xd4\xea\x71\x29\xc0\x94\x85\x86\x7a\xba\xe8\x40\x34\x42\x25\xe3\xf3\x12\x73\x53\x61\xda\xc1\x6c\xb0\xee\x54\x64\xcd\x28\x4a\xcb\x12\x8b\x92\x33\x12\x8b\x34\x4c\x0d\x34\x41\x26\x85\xfa\x79\x06\x86\xba\x2a\x68\x40\x2c\xd0\x41\x51\xab\xc9\xa5\x69\xcd\x05\x08\x00\x00\xff\xff\xf1\xad\x88\x01\xd1\x00\x00\x00")

func _000003_rename_sites_to_projectsUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000003_rename_sites_to_projectsUpSql,
		"000003_rename_sites_to_projects.up.sql",
	)
}

func _000003_rename_sites_to_projectsUpSql() (*asset, error) {
	bytes, err := _000003_rename_sites_to_projectsUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000003_rename_sites_to_projects.up.sql", size: 209, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
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
	"000001_create_sites_table.down.sql":       _000001_create_sites_tableDownSql,
	"000001_create_sites_table.up.sql":         _000001_create_sites_tableUpSql,
	"000002_create_site_name_column.down.sql":  _000002_create_site_name_columnDownSql,
	"000002_create_site_name_column.up.sql":    _000002_create_site_name_columnUpSql,
	"000003_rename_sites_to_projects.down.sql": _000003_rename_sites_to_projectsDownSql,
	"000003_rename_sites_to_projects.up.sql":   _000003_rename_sites_to_projectsUpSql,
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
	"000001_create_sites_table.down.sql":       &bintree{_000001_create_sites_tableDownSql, map[string]*bintree{}},
	"000001_create_sites_table.up.sql":         &bintree{_000001_create_sites_tableUpSql, map[string]*bintree{}},
	"000002_create_site_name_column.down.sql":  &bintree{_000002_create_site_name_columnDownSql, map[string]*bintree{}},
	"000002_create_site_name_column.up.sql":    &bintree{_000002_create_site_name_columnUpSql, map[string]*bintree{}},
	"000003_rename_sites_to_projects.down.sql": &bintree{_000003_rename_sites_to_projectsDownSql, map[string]*bintree{}},
	"000003_rename_sites_to_projects.up.sql":   &bintree{_000003_rename_sites_to_projectsUpSql, map[string]*bintree{}},
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
