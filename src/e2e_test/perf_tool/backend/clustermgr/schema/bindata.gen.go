// Code generated for package schema by go-bindata DO NOT EDIT. (@generated)
// sources:
// 000001_create_clusters_table.down.sql
// 000001_create_clusters_table.up.sql
// 000002_prepare_queue_table.down.sql
// 000002_prepare_queue_table.up.sql
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

var __000001_create_clusters_tableDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\x08\x71\x74\xf2\x71\x55\xf0\x74\x53\x70\x8d\xf0\x0c\x0e\x09\x56\x48\xce\x29\x2d\x2e\x49\x2d\x2a\xb6\xe6\x02\x04\x00\x00\xff\xff\x7c\x17\x30\x99\x1f\x00\x00\x00")

func _000001_create_clusters_tableDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000001_create_clusters_tableDownSql,
		"000001_create_clusters_table.down.sql",
	)
}

func _000001_create_clusters_tableDownSql() (*asset, error) {
	bytes, err := _000001_create_clusters_tableDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000001_create_clusters_table.down.sql", size: 31, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000001_create_clusters_tableUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x74\x53\x4d\x6f\xd3\x40\x10\xbd\xfb\x57\xbc\x5b\x5a\x29\x01\x81\xc4\xa9\xea\xc1\x25\x0b\xb2\x68\xa3\x92\xc6\x48\x39\x45\x5b\x7b\x62\x0f\x75\x66\xcd\x7e\xa4\x2d\xbf\x1e\xad\xed\x34\x4e\x80\x4b\x14\x79\xe6\xcd\x7b\xf3\xde\xec\xe7\xa5\x4a\x57\x0a\xab\xf5\xbd\x42\xd1\x04\xe7\xc9\x6e\x9c\xd7\x3e\x38\xa4\x0f\x50\x8b\xfc\x0e\x17\x93\xf4\x47\x9a\xdd\xa6\x37\xb7\x6a\x32\xc5\x24\x5b\xe4\x0f\x6a\x72\x79\x95\x24\x07\x6c\xac\x1c\xc0\x0e\x17\x09\x50\x70\x09\x16\x8f\xfb\x65\x76\x97\x2e\xd7\xf8\xa6\xd6\xf8\xaa\x16\x6a\x99\xae\xd4\x1c\x37\x6b\xcc\xd5\x97\x34\xbf\x5d\x45\x8e\x6c\xae\x16\xab\x6c\xb5\x9e\x26\xc0\x6c\x86\xea\x89\x20\x7a\x47\x30\x5b\xf8\x9a\x0e\x73\x13\xf4\x5f\xf7\xda\x16\xb5\xb6\x17\x9f\x3e\x7c\xbc\x44\xbe\xc8\xbe\xe7\x6a\x00\x3e\x85\x47\x2a\x8c\x6c\xb9\x02\xbb\x0e\x3a\xfa\xb2\x35\x16\xbe\x66\x77\x18\x37\x05\x4b\xd1\x84\x92\xa5\x02\x7b\x07\x92\xb2\x35\x2c\x7e\x0a\x2d\x25\x1c\xd9\x3d\x59\x14\x1a\x05\x59\xff\x2e\xc1\x78\xd4\xe3\xab\x27\x3d\x45\x4f\xda\xf2\x0b\xd3\xe6\x60\x5c\xdc\xda\x8d\x55\xa3\xf3\xa1\xef\x42\xd1\x98\x50\x42\x3b\xc7\x95\x50\x09\x6f\x4e\x14\x45\x96\xbf\xa6\xe5\x79\x36\x1f\xd6\xa3\x97\x96\x2c\xef\x48\xfc\x26\x84\x23\x51\xf7\x7f\xb0\xea\xd8\xf2\x7f\x16\x64\x5b\x0c\xf9\x5e\x5f\xe3\x2d\xd8\xbe\xa9\xd6\x0e\x62\xb0\x23\x2d\x2c\x55\x54\x74\xce\xfa\x6f\x41\x96\xbc\x7d\xdd\x70\xf9\x02\x6f\x2a\xf2\x35\x59\x3c\xb3\xaf\xcf\xd1\x53\x04\xe1\x5f\x81\x9a\x57\x70\x49\xe2\x79\xcb\xe4\xa0\xe1\x58\xaa\x86\x60\x83\xc4\x4d\xb4\x8c\x70\x67\x1a\x8e\x44\x31\xab\x5e\x47\xd1\x90\x96\xd0\x6e\x3c\xef\x28\xba\x12\xda\x52\x7b\x2a\xf1\x5c\x93\x9c\x66\xe1\xde\x6c\xe9\x63\x7e\xeb\xb0\x41\xa4\x6f\xa8\x78\x4f\x02\x5d\x14\xe4\x5c\xef\x1d\x8d\x03\x9a\xcd\xba\x6b\x09\xae\x77\x76\xa0\x3e\xde\xbe\xaf\xb5\x47\xad\xf7\x84\x47\x8a\x73\x06\xba\xf7\x2c\xc1\x51\x7f\x84\xc6\xa0\x31\xbd\xb9\x27\xca\xe3\x8f\xf3\x7a\xd7\x0e\x6b\x0d\x21\x9d\xbe\x82\x88\x1a\x0a\xa7\x8f\x75\x00\x89\x29\x69\xd3\x1a\xd3\xb8\xb8\xcc\x4f\x67\x24\xde\x32\xeb\x86\x7f\x53\xd9\x55\xd1\x55\xe3\x9c\x51\x6f\x77\xd2\xc9\xe5\x55\xf2\x27\x00\x00\xff\xff\x3c\x09\xfc\x79\x0e\x04\x00\x00")

func _000001_create_clusters_tableUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000001_create_clusters_tableUpSql,
		"000001_create_clusters_table.up.sql",
	)
}

func _000001_create_clusters_tableUpSql() (*asset, error) {
	bytes, err := _000001_create_clusters_tableUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000001_create_clusters_table.up.sql", size: 1038, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000002_prepare_queue_tableDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\x08\x71\x74\xf2\x71\x55\xf0\x74\x53\x70\x8d\xf0\x0c\x0e\x09\x56\x48\xce\x29\x2d\x2e\x49\x2d\x8a\x2f\x28\x4a\x2d\x48\x2c\x4a\x8d\x2f\x2c\x4d\x2d\x4d\xb5\xe6\x02\x04\x00\x00\xff\xff\x02\x86\x70\x7f\x2c\x00\x00\x00")

func _000002_prepare_queue_tableDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000002_prepare_queue_tableDownSql,
		"000002_prepare_queue_table.down.sql",
	)
}

func _000002_prepare_queue_tableDownSql() (*asset, error) {
	bytes, err := _000002_prepare_queue_tableDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000002_prepare_queue_table.down.sql", size: 44, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000002_prepare_queue_tableUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x64\x90\xcd\x6e\xea\x30\x10\x85\xf7\x79\x8a\xb3\xbc\x57\x02\x5e\xa0\xab\x40\xdc\x2a\x2a\x45\x55\x08\x8b\xac\x22\x83\x07\x18\x29\x75\x92\xf1\x58\x85\x3e\x7d\x15\x68\x95\x2a\x6c\xc7\xe7\xe7\x3b\x5e\x15\x26\x2d\x0d\xca\x74\xb9\x36\x38\x34\x31\x28\x49\xdd\x09\x75\x56\xa8\xee\x23\x45\xc2\xbf\x04\x98\xcf\xc1\x5e\x49\xbc\x6d\xc0\x0e\x31\x90\x83\xb6\x68\xc5\x91\x40\xcf\x84\x9b\x74\x91\x00\x3d\xbb\x41\x8a\xf7\x22\x7f\x4b\x8b\x0a\xaf\xa6\xc2\x8b\xd9\x98\x22\x2d\x4d\x86\x65\x85\xcc\x3c\xa7\xbb\x75\x89\x74\x8b\x3c\x33\x9b\x32\x2f\xab\xd9\xbd\x81\x2e\x1d\x09\x7f\x90\xd7\x3a\x46\x76\x68\x8f\xb7\xe8\xf1\x0c\xa1\x3e\x52\x50\xf6\x27\xd8\x5f\xda\xa1\x74\xea\xdc\xed\xf2\xec\x31\x54\x48\xe5\x5a\xb3\xbb\x40\xdb\x13\xe9\x99\x04\x9f\xac\xe7\xa9\x7b\x86\xe8\xb9\x8f\xd4\x5c\xc1\x8e\xbc\xf2\x91\x29\xc0\xa2\xb3\xa2\x7c\x88\x8d\x15\x48\xf4\x03\x9d\xf5\x7f\xbc\x13\x8e\xb1\x8c\xbd\xfe\xb0\x04\x12\xb6\x0d\x7f\x91\xab\x43\x47\x07\x70\x98\x0c\xec\xf6\x8b\xd5\x7d\xd5\x76\x78\x1f\xf5\xc3\x67\xef\xaf\x4a\x21\xc1\x43\xca\x70\xb7\xc9\xff\xa7\x24\xf9\x0e\x00\x00\xff\xff\x66\xc9\x28\xa5\xcd\x01\x00\x00")

func _000002_prepare_queue_tableUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000002_prepare_queue_tableUpSql,
		"000002_prepare_queue_table.up.sql",
	)
}

func _000002_prepare_queue_tableUpSql() (*asset, error) {
	bytes, err := _000002_prepare_queue_tableUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000002_prepare_queue_table.up.sql", size: 461, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
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
	"000001_create_clusters_table.down.sql": _000001_create_clusters_tableDownSql,
	"000001_create_clusters_table.up.sql":   _000001_create_clusters_tableUpSql,
	"000002_prepare_queue_table.down.sql":   _000002_prepare_queue_tableDownSql,
	"000002_prepare_queue_table.up.sql":     _000002_prepare_queue_tableUpSql,
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
	"000001_create_clusters_table.down.sql": &bintree{_000001_create_clusters_tableDownSql, map[string]*bintree{}},
	"000001_create_clusters_table.up.sql":   &bintree{_000001_create_clusters_tableUpSql, map[string]*bintree{}},
	"000002_prepare_queue_table.down.sql":   &bintree{_000002_prepare_queue_tableDownSql, map[string]*bintree{}},
	"000002_prepare_queue_table.up.sql":     &bintree{_000002_prepare_queue_tableUpSql, map[string]*bintree{}},
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
