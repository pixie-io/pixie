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

var __000001_create_clusters_tableUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x74\x53\x4d\x6f\xd3\x40\x10\xbd\xfb\x57\xbc\x5b\x5a\x29\x01\x81\xc4\xa9\xea\xc1\x25\x0b\xb2\x68\xa3\x92\xc6\x48\x39\x45\x5b\x7b\x62\x0f\x75\x66\xcd\x7e\xa4\x2d\xbf\x1e\xad\xed\x34\x4e\x80\x4b\x14\x79\xe6\xcd\x7b\xf3\xde\xec\xe7\xa5\x4a\x57\x0a\xab\xf5\xbd\x42\xd1\x04\xe7\xc9\x6e\x9c\xd7\x3e\x38\xa4\x0f\x50\x8b\xfc\x0e\x17\x93\xf4\x47\x9a\xdd\xa6\x37\xb7\x6a\x32\xc5\x24\x5b\xe4\x0f\x6a\x72\x79\x95\x24\x07\x6c\xac\x1c\xc0\x0e\x17\x09\x50\x70\x09\x16\x8f\xfb\x65\x76\x97\x2e\xd7\xf8\xa6\xd6\xf8\xaa\x16\x6a\x99\xae\xd4\x1c\x37\x6b\xcc\xd5\x97\x34\xbf\x5d\x45\x8e\x6c\xae\x16\xab\x6c\xb5\x9e\x26\xc0\x6c\x86\xea\x89\x20\x7a\x47\x30\x5b\xf8\x9a\x0e\x73\x13\xf4\x5f\xf7\xda\x16\xb5\xb6\x17\x9f\x3e\x7c\xbc\x44\xbe\xc8\xbe\xe7\x6a\x00\x3e\x85\x47\x2a\x8c\x6c\xb9\x02\xbb\x0e\x3a\xfa\xb2\x35\x16\xbe\x66\x77\x18\x37\x05\x4b\xd1\x84\x92\xa5\x02\x7b\x07\x92\xb2\x35\x2c\x7e\x0a\x2d\x25\x1c\xd9\x3d\x59\x14\x1a\x05\x59\xff\x2e\xc1\x78\xd4\xe3\xab\x27\x3d\x70\xb6\xfc\xc2\xb4\x39\xf8\x16\x97\x76\x63\xd1\xe8\x6c\xe8\xbb\x50\x34\x26\x94\xd0\xce\x71\x25\x54\xc2\x9b\x13\x41\x91\xe4\xaf\x69\x79\x9e\xcd\x07\x26\x7a\x69\xc9\xf2\x8e\xc4\x6f\x42\x38\x12\x75\xff\x07\xa7\x8e\x2d\xff\x67\x41\xb6\xc5\x10\xef\xf5\x35\xde\x72\xed\x9b\x6a\xed\x20\x06\x3b\xd2\xc2\x52\x45\x45\xe7\xac\xff\x16\x64\xc9\xdb\xd7\x0d\x97\x2f\xf0\xa6\x22\x5f\x93\xc5\x33\xfb\xfa\x1c\x3d\x45\x10\xfe\x15\xa8\x79\x05\x97\x24\x9e\xb7\x4c\x0e\x1a\x8e\xa5\x6a\x08\x36\x48\xdc\x44\xcb\x08\x77\xa6\xe1\x48\x14\xa3\xea\x75\x14\x0d\x69\x09\xed\xc6\xf3\x8e\xa2\x2b\xa1\x2d\xb5\xa7\x12\xcf\x35\xc9\x69\x16\xee\xcd\x96\x3e\xe5\xb7\x0e\x1b\x44\xfa\x86\x8a\xf7\x24\xd0\x45\x41\xce\xf5\xde\xd1\x38\xa0\xd9\xac\x3b\x96\xe0\x7a\x67\x07\xea\xe3\xe9\xfb\x5a\x7b\xd4\x7a\x4f\x78\xa4\x38\x67\xa0\x7b\xcf\x12\x1c\xf5\x37\x68\x0c\x1a\xd3\x9b\x7b\xa2\x3c\xfe\x38\xaf\x77\xed\xb0\xd6\x10\xd2\xe9\x23\x88\xa8\xa1\x70\xfa\x56\x07\x90\x98\x92\x36\xad\x31\x8d\x8b\xcb\xfc\x74\x46\xe2\x29\xb3\x6e\xf8\x37\x95\x5d\x15\x5d\x35\xce\x19\xf5\x76\x17\x9d\x5c\x5e\x25\x7f\x02\x00\x00\xff\xff\xb4\xb5\x63\xe4\x0d\x04\x00\x00")

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

	info := bindataFileInfo{name: "000001_create_clusters_table.up.sql", size: 1037, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
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

var __000002_prepare_queue_tableUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x64\x90\xcd\x6e\xea\x30\x10\x85\xf7\x79\x8a\xb3\xbc\x57\x02\x5e\xa0\xab\x40\xdc\x2a\x2a\x45\x55\x08\x8b\xac\x22\x83\x07\x18\x29\x75\x92\xf1\x58\x85\x3e\x7d\x15\x68\x95\x2a\x6c\xc7\xe7\xe7\x3b\x5e\x15\x26\x2d\x0d\xca\x74\xb9\x36\x38\x34\x31\x28\x49\xdd\x09\x75\x56\xa8\xee\x23\x45\xc2\xbf\x04\x98\xcf\xc1\x5e\x49\xbc\x6d\xc0\x0e\x31\x90\x83\xb6\x68\xc5\x91\x40\xcf\x84\x9b\x74\x91\x00\x3d\xbb\x41\x8a\xf7\x22\x7f\x4b\x8b\x0a\xaf\xa6\xc2\x8b\xd9\x98\x22\x2d\x4d\x86\x65\x85\xcc\x3c\xa7\xbb\x75\x89\x74\x8b\x3c\x33\x9b\x32\x2f\xab\xd9\xbd\x81\x2e\x1d\x09\x7f\x90\xd7\x3a\x46\x76\x68\x8f\xb7\xe8\xf1\x0c\xa1\x3e\x52\x50\xf6\x27\xd8\x5f\xda\xa1\x74\xea\xdc\xed\xf2\xec\x31\x54\x48\xe5\x5a\xb3\xbb\x40\xdb\x13\xe9\x99\x04\x9f\xac\xe7\xa9\x7b\x86\xe8\xb9\x8f\xd4\x5c\xc1\x8e\xbc\xf2\x91\x29\xc0\xa2\xb3\xa2\x7c\x88\x8d\x15\x48\xf4\x03\x9d\xf5\x7f\xbc\x13\x8e\xb1\x8c\xbd\xfe\xb0\x04\x12\xb6\x0d\x7f\x91\xab\x43\x47\x07\x70\x98\x0c\xec\xf6\x8b\xd5\x7d\xd5\x76\x78\x1f\xf5\xc3\x67\xef\xaf\x4a\x21\xc1\x43\xca\x70\xb7\xc9\xff\xa7\xe4\x3b\x00\x00\xff\xff\x0b\x7d\x16\x2d\xcc\x01\x00\x00")

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

	info := bindataFileInfo{name: "000002_prepare_queue_table.up.sql", size: 460, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
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
