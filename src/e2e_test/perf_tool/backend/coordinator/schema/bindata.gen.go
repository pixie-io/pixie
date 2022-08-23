// Code generated for package schema by go-bindata DO NOT EDIT. (@generated)
// sources:
// 000001_create_experiment_queue_table.down.sql
// 000001_create_experiment_queue_table.up.sql
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

var __000001_create_experiment_queue_tableDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\x08\x71\x74\xf2\x71\x55\xf0\x74\x53\x70\x8d\xf0\x0c\x0e\x09\x56\x48\xad\x28\x48\x2d\xca\xcc\x4d\xcd\x2b\x89\x2f\x2c\x4d\x2d\x4d\xb5\xe6\x02\x04\x00\x00\xff\xff\x5c\xf5\x79\xad\x27\x00\x00\x00")

func _000001_create_experiment_queue_tableDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000001_create_experiment_queue_tableDownSql,
		"000001_create_experiment_queue_table.down.sql",
	)
}

func _000001_create_experiment_queue_tableDownSql() (*asset, error) {
	bytes, err := _000001_create_experiment_queue_tableDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000001_create_experiment_queue_table.down.sql", size: 39, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000001_create_experiment_queue_tableUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x6c\x91\xcf\x72\x82\x30\x10\x87\xef\x3c\xc5\xde\xd4\x19\xed\x0b\xf4\x84\x1a\x6d\xc6\x8a\x36\xc0\x74\x38\x31\x41\x96\xba\xd3\x12\x34\x09\xd3\x3f\x4f\xdf\x49\x90\x96\x5a\x8f\xd9\xdd\xdf\xf7\x2d\xcb\x42\xb0\x30\x61\x90\x64\x7b\x06\xf8\x71\x42\x4d\x35\x2a\x9b\x1b\x2b\x2d\x42\x18\x03\x8b\xd2\x2d\x8c\x03\x80\xd1\x73\xc8\x13\x1e\xad\xf3\x24\x8c\x37\xf1\x68\xea\x4a\x22\x8d\x22\x1e\xad\xbb\xc7\x8a\x47\x3c\x7e\x60\xcb\xee\xc5\x84\xd8\x89\xcb\x14\x4b\x44\x96\xcf\xc3\xc5\x66\xb7\x5a\x8d\x82\xc9\x7d\x10\xf4\xd6\x70\xfe\xf8\x47\x7b\x6e\xb1\x45\xaf\x9b\xcd\x80\x4a\x20\x03\xf6\x88\xd0\x2a\x3a\xb7\xe8\x0a\x4d\x05\x52\x0d\x12\x01\xb8\x6a\x9a\xf2\x25\xa4\x11\x7f\x4a\xd9\xb4\xcb\x1a\xd4\x24\xdf\xe8\x0b\xcb\xdc\x9c\xf0\xd0\x83\x4e\xba\xb1\x4d\xd1\x56\x83\xfe\x00\x06\x6e\xf4\x2e\x80\x7f\xe9\xe2\xd3\xa2\xec\xc9\xfe\x32\x4d\xe5\x79\xbf\x59\x1f\xf3\xad\xeb\x2b\x5e\x72\xaa\xad\x73\x2b\xcd\xab\xc9\xdf\x25\x59\x52\x2f\xfd\x4e\xaa\xad\x0b\xd4\x9e\xe8\xda\x60\x8f\x64\x86\x4b\x91\x81\x3e\x51\x35\x1a\x0a\xac\x1a\x8d\x40\x16\x0e\x52\x39\xa5\xf6\xee\x1b\x78\x65\x07\x6a\x8d\x56\x13\x9a\x1b\x52\xaa\xd1\x5c\x7d\x0c\x1c\xa5\x81\x02\x51\x41\x17\x2b\x7b\xc3\x0f\xc5\xb1\x03\x80\xbd\xe0\xdb\x50\x64\xb0\x61\xd9\x98\xca\x89\xfb\xb7\xdf\x01\x00\x00\xff\xff\x3d\x7e\x10\xf2\x51\x02\x00\x00")

func _000001_create_experiment_queue_tableUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000001_create_experiment_queue_tableUpSql,
		"000001_create_experiment_queue_table.up.sql",
	)
}

func _000001_create_experiment_queue_tableUpSql() (*asset, error) {
	bytes, err := _000001_create_experiment_queue_tableUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000001_create_experiment_queue_table.up.sql", size: 593, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
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
	"000001_create_experiment_queue_table.down.sql": _000001_create_experiment_queue_tableDownSql,
	"000001_create_experiment_queue_table.up.sql":   _000001_create_experiment_queue_tableUpSql,
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
	"000001_create_experiment_queue_table.down.sql": &bintree{_000001_create_experiment_queue_tableDownSql, map[string]*bintree{}},
	"000001_create_experiment_queue_table.up.sql":   &bintree{_000001_create_experiment_queue_tableUpSql, map[string]*bintree{}},
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
