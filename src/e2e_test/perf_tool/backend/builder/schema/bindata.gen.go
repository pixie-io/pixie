// Code generated for package schema by go-bindata DO NOT EDIT. (@generated)
// sources:
// 000001_create_build_queue_table.down.sql
// 000001_create_build_queue_table.up.sql
// 000002_create_build_artifacts_table.down.sql
// 000002_create_build_artifacts_table.up.sql
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

var __000001_create_build_queue_tableDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\x08\x71\x74\xf2\x71\x55\xf0\x74\x53\x70\x8d\xf0\x0c\x0e\x09\x56\x48\x2a\xcd\xcc\x49\x89\x2f\x2c\x4d\x2d\x4d\xe5\x02\x04\x00\x00\xff\xff\x2c\x0f\x29\xb4\x21\x00\x00\x00")

func _000001_create_build_queue_tableDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000001_create_build_queue_tableDownSql,
		"000001_create_build_queue_table.down.sql",
	)
}

func _000001_create_build_queue_tableDownSql() (*asset, error) {
	bytes, err := _000001_create_build_queue_tableDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000001_create_build_queue_table.down.sql", size: 33, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000001_create_build_queue_tableUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x64\x91\xc1\x8e\xa2\x40\x10\x86\xef\xfd\x14\xff\x0d\x48\x94\x17\xf0\xc4\xee\x76\x0c\x59\x75\x0d\x68\x36\x9e\x08\xd8\xe5\x50\x13\x04\x6c\xaa\x33\x3a\x4f\x3f\x11\x74\x64\xc2\xb1\xab\xfe\xaf\x3e\xa8\xfa\x9d\xe8\x68\xa7\xb1\x3b\x6c\x35\x0a\xc7\x95\xc9\x3a\xc9\x85\x10\xa5\xd0\x9b\xfd\x1a\xbe\xf7\x3f\x8a\x77\xf1\x66\xe9\xcd\xe0\xc5\x9b\x6c\x9b\xfc\x5b\x26\x3a\x4d\xbd\x60\xa1\xd4\x13\x8e\x7e\xad\x9e\xf4\xc5\x91\x23\xf8\x0a\x98\xcf\xc1\xb5\x90\xad\xf3\x0a\x6c\xe0\x3a\x32\x90\x06\x8d\x35\x64\x21\x25\xa1\x8f\x86\x0a\xf7\x6e\xaa\x93\x38\x5a\xcd\x06\x8c\xae\x2d\x59\x3e\x53\x2d\x99\x73\x6c\xd0\x9c\x20\x25\x77\x83\x01\xef\x4d\xa1\x30\xc9\xec\xf7\xf1\x9f\x29\x6e\x49\xec\x2d\x63\x73\x85\x34\x6f\x24\x25\x59\x7c\xb0\x94\x13\xda\xd5\x7c\x71\x54\xdd\xc0\x86\x6a\xe1\x13\x53\x37\x28\x5f\x41\xe4\x22\x74\x6e\x25\xfc\x29\x7f\x19\xb8\x96\xc7\x07\x74\x64\x39\xaf\xf8\x93\x4c\xd6\xb5\x74\x04\x77\xfd\xff\xb6\xb6\x91\xa6\x70\xa7\x51\x7f\x3c\xff\x1e\x0d\xe1\xa7\x44\xa3\x6a\x5b\x84\xfa\xfb\x91\xb6\x74\x0c\x14\x26\xf3\x8b\x9b\x50\xfe\x74\xf7\xd7\x7b\x18\x87\x47\xbf\x3e\x1a\xb6\x77\xa7\xfb\xe2\xe8\xd6\x33\xa5\x80\x6d\x12\xaf\xa3\xe4\x80\xbf\xfa\xe0\xb3\x09\x54\xb0\x50\x5f\x01\x00\x00\xff\xff\x45\xf3\x39\xdf\x1b\x02\x00\x00")

func _000001_create_build_queue_tableUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000001_create_build_queue_tableUpSql,
		"000001_create_build_queue_table.up.sql",
	)
}

func _000001_create_build_queue_tableUpSql() (*asset, error) {
	bytes, err := _000001_create_build_queue_tableUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000001_create_build_queue_table.up.sql", size: 539, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000002_create_build_artifacts_tableDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\x08\x71\x74\xf2\x71\x55\xf0\x74\x53\x70\x8d\xf0\x0c\x0e\x09\x56\x48\x2a\xcd\xcc\x49\x89\x4f\x2c\x2a\xc9\x4c\x4b\x4c\x2e\x29\xb6\xe6\x02\x04\x00\x00\xff\xff\x04\x91\x04\x12\x26\x00\x00\x00")

func _000002_create_build_artifacts_tableDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000002_create_build_artifacts_tableDownSql,
		"000002_create_build_artifacts_table.down.sql",
	)
}

func _000002_create_build_artifacts_tableDownSql() (*asset, error) {
	bytes, err := _000002_create_build_artifacts_tableDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000002_create_build_artifacts_table.down.sql", size: 38, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000002_create_build_artifacts_tableUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x4c\xcb\xb1\x0a\xc2\x30\x10\x87\xf1\x3d\x4f\xf1\x1f\x15\x7c\x03\xa7\xaa\x19\x0a\x22\x28\xcd\x7c\x5c\xd3\x13\x0e\x12\x29\xb9\x0b\xa8\x4f\x2f\xdd\x5c\xbe\xe5\xe3\x77\x7e\xc4\x61\x8a\x98\x86\xd3\x35\x62\xee\x5a\x16\xe2\xe6\xfa\xe4\xec\x86\x5d\x00\xe4\xbd\x4a\xd3\x2a\x2f\xa7\xde\x75\x41\x4a\xe3\x05\xe9\x36\xde\x53\x3c\x04\xc0\xa4\x29\x17\xfd\xca\xbf\x9b\x3f\x2e\xbc\xdd\xc2\xe6\xc4\x39\x8b\x19\xb9\x56\xc1\x16\x73\xae\x6b\xd8\x1f\xc3\x2f\x00\x00\xff\xff\xb2\x20\x39\x56\x7c\x00\x00\x00")

func _000002_create_build_artifacts_tableUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000002_create_build_artifacts_tableUpSql,
		"000002_create_build_artifacts_table.up.sql",
	)
}

func _000002_create_build_artifacts_tableUpSql() (*asset, error) {
	bytes, err := _000002_create_build_artifacts_tableUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000002_create_build_artifacts_table.up.sql", size: 124, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
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
	"000001_create_build_queue_table.down.sql":     _000001_create_build_queue_tableDownSql,
	"000001_create_build_queue_table.up.sql":       _000001_create_build_queue_tableUpSql,
	"000002_create_build_artifacts_table.down.sql": _000002_create_build_artifacts_tableDownSql,
	"000002_create_build_artifacts_table.up.sql":   _000002_create_build_artifacts_tableUpSql,
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
	"000001_create_build_queue_table.down.sql":     &bintree{_000001_create_build_queue_tableDownSql, map[string]*bintree{}},
	"000001_create_build_queue_table.up.sql":       &bintree{_000001_create_build_queue_tableUpSql, map[string]*bintree{}},
	"000002_create_build_artifacts_table.down.sql": &bintree{_000002_create_build_artifacts_tableDownSql, map[string]*bintree{}},
	"000002_create_build_artifacts_table.up.sql":   &bintree{_000002_create_build_artifacts_tableUpSql, map[string]*bintree{}},
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
