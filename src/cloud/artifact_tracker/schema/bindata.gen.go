// Code generated for package schema by go-bindata DO NOT EDIT. (@generated)
// sources:
// 000001_create_artifact_tables.down.sql
// 000001_create_artifact_tables.up.sql
// 000002_add_container_yamls.down.sql
// 000002_add_container_yamls.up.sql
// 000003_add_container_tmpl_yamls.down.sql
// 000003_add_container_tmpl_yamls.up.sql
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

var __000001_create_artifact_tablesDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\x08\x71\x74\xf2\x71\x55\xf0\x74\x53\x70\x8d\xf0\x0c\x0e\x09\x56\x48\x2c\x2a\xc9\x4c\x4b\x4c\x2e\x89\x4f\xce\x48\xcc\x4b\x4f\xcd\xc9\x4f\x2f\x56\x70\x76\x0c\x76\x76\x74\x71\xb5\xe6\xc2\xab\x03\x43\x5d\x64\x00\x56\x83\x4b\x2a\x0b\x52\x11\x4a\x01\x01\x00\x00\xff\xff\x2a\xeb\xa5\xc9\x85\x00\x00\x00")

func _000001_create_artifact_tablesDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000001_create_artifact_tablesDownSql,
		"000001_create_artifact_tables.down.sql",
	)
}

func _000001_create_artifact_tablesDownSql() (*asset, error) {
	bytes, err := _000001_create_artifact_tablesDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000001_create_artifact_tables.down.sql", size: 133, mode: os.FileMode(436), modTime: time.Unix(1603471763, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000001_create_artifact_tablesUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x94\x92\x4f\x8f\xda\x3c\x18\xc4\xef\xf9\x14\xa3\xbd\x90\x95\xc2\xab\xf7\x40\x7b\xe9\xc9\x05\xd3\x5a\x4b\x1c\xea\x38\x5d\x50\x55\x59\xde\xc4\x25\x96\x92\x18\xc5\x0e\x12\xdf\xbe\x22\xd0\x02\xad\xfa\x2f\xd7\x67\x9e\x67\x7e\x9e\xc9\x5c\x50\x22\x29\xe8\x46\x52\x9e\xb3\x8c\x83\x2d\xc1\x33\x09\xba\x61\xb9\xcc\xf1\x30\x0c\xb6\x9a\x3a\xef\xf7\x0f\x6f\xa2\xe8\x22\x96\xdb\x35\x85\xee\x83\xfd\xa2\xcb\xa0\xc2\x71\x6f\x40\x72\x50\x5e\xa4\xf1\xa4\xe0\x4f\x3c\x7b\xe6\x93\x24\xc2\x9f\xbe\xc9\x8a\xf1\x62\xa3\x48\xba\x78\x3d\xfb\x2b\xfd\x82\x88\x67\xc6\xff\x61\x61\x9e\x71\x49\x18\xa7\x42\xe5\x54\xaa\x5b\xbb\xc7\x9b\xd7\x90\xb7\xab\xeb\x73\x3c\xe2\x08\xb0\x15\x8a\x82\x2d\x50\x70\xf6\xa1\xa0\x58\xd0\x25\x29\x56\x12\xa7\x30\xd4\xce\x74\xa6\xd7\xc1\xa8\xc3\x2c\x7e\x3c\x51\x4c\xa7\xd7\x30\x3a\xdd\x1a\x58\x0f\x5d\x86\x41\x37\xcd\x11\xd5\xb0\x6f\x6c\xa9\x83\xa9\x12\xbc\x0c\x01\x95\x33\x1e\x9d\x0b\xf0\xc6\xb4\xe8\x4c\x69\xbc\xd7\xfd\x11\xc1\x61\x3f\x34\x0d\x6c\x80\x1b\xc2\xf9\xaa\xed\x82\x83\xee\x5c\xa8\x4d\x8f\xa0\x5f\x1a\xf3\x5f\x84\x1f\xbc\x3e\x12\x31\x7f\x4f\x44\xfc\xea\xff\x91\xa5\xec\xcd\x09\x2d\xd8\xd6\x40\xb2\x94\xe6\x92\xa4\xeb\x71\xe0\xda\xd6\x06\x55\x6b\x5f\xa3\xac\x75\x1f\xcf\xce\x0b\x07\xd3\x7b\xeb\x3a\xe5\x43\x8f\x83\xee\xc7\xd1\xe5\x96\x3e\x68\xdb\x9c\x5c\xd5\x35\x9c\xbb\xd6\x3f\x7d\x4e\xa2\x08\x58\x0b\x96\x12\xb1\xc5\x13\xdd\xc6\xb6\x4a\xee\x01\x93\x5b\x87\xf1\xec\x39\xd3\xf8\x37\xaa\xe8\x97\xed\xa8\xb2\xd6\xdd\xce\x34\x6e\x77\xee\xe9\x5a\x9b\xba\x54\x36\xfe\x16\xdf\x55\x90\x74\x23\x47\xc8\x3b\xca\xdb\xad\x91\x09\xcb\x4c\x50\xf6\x8e\xff\x3c\x85\xa0\x4b\x2a\x28\x9f\xd3\x1c\xe4\xdb\x20\xb6\xd5\x08\xf9\x35\x00\x00\xff\xff\xbc\xf1\xd7\xe5\x3d\x03\x00\x00")

func _000001_create_artifact_tablesUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000001_create_artifact_tablesUpSql,
		"000001_create_artifact_tables.up.sql",
	)
}

func _000001_create_artifact_tablesUpSql() (*asset, error) {
	bytes, err := _000001_create_artifact_tablesUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000001_create_artifact_tables.up.sql", size: 829, mode: os.FileMode(436), modTime: time.Unix(1603471763, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000002_add_container_yamlsDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x89\x0c\x70\x55\x48\x2c\x2a\xc9\x4c\x4b\x4c\x2e\x89\x2f\xa9\x2c\x48\x55\x70\x09\xf2\x0f\x50\x08\x73\xf4\x09\x75\x55\x50\x77\xf6\xf7\x0b\x71\xf4\xf4\x73\x0d\x8a\x0f\x76\x0d\x89\x8f\x74\xf4\xf5\x09\x56\xb7\xe6\x02\x04\x00\x00\xff\xff\x4e\x00\x7f\x70\x3b\x00\x00\x00")

func _000002_add_container_yamlsDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000002_add_container_yamlsDownSql,
		"000002_add_container_yamls.down.sql",
	)
}

func _000002_add_container_yamlsDownSql() (*asset, error) {
	bytes, err := _000002_add_container_yamlsDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000002_add_container_yamls.down.sql", size: 59, mode: os.FileMode(436), modTime: time.Unix(1603471763, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000002_add_container_yamlsUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x89\x0c\x70\x55\x48\x2c\x2a\xc9\x4c\x4b\x4c\x2e\x89\x2f\xa9\x2c\x48\x55\x70\x74\x71\x51\x08\x73\xf4\x09\x75\x55\x50\x77\xf6\xf7\x0b\x71\xf4\xf4\x73\x0d\x8a\x0f\x76\x0d\x89\x8f\x74\xf4\xf5\x09\x56\x57\x70\x72\x75\xf3\x0f\xc2\x90\xf4\xf1\xf4\x0b\x8d\x88\x77\xf4\x75\x31\x33\x51\xb7\xe6\x02\x04\x00\x00\xff\xff\x48\xab\xba\x7c\x5d\x00\x00\x00")

func _000002_add_container_yamlsUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000002_add_container_yamlsUpSql,
		"000002_add_container_yamls.up.sql",
	)
}

func _000002_add_container_yamlsUpSql() (*asset, error) {
	bytes, err := _000002_add_container_yamlsUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000002_add_container_yamls.up.sql", size: 93, mode: os.FileMode(436), modTime: time.Unix(1603471763, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000003_add_container_tmpl_yamlsDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x89\x0c\x70\x55\x48\x2c\x2a\xc9\x4c\x4b\x4c\x2e\x89\x2f\xa9\x2c\x48\x55\x70\x09\xf2\x0f\x50\x08\x73\xf4\x09\x75\x55\x50\x77\xf6\xf7\x0b\x71\xf4\xf4\x73\x0d\x8a\x0f\x76\x0d\x89\x0f\x71\xf5\x0d\xf0\x71\x0c\x71\x8d\x8f\x74\xf4\xf5\x09\x56\xb7\xe6\x02\x04\x00\x00\xff\xff\x4a\xe5\xf0\x80\x44\x00\x00\x00")

func _000003_add_container_tmpl_yamlsDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000003_add_container_tmpl_yamlsDownSql,
		"000003_add_container_tmpl_yamls.down.sql",
	)
}

func _000003_add_container_tmpl_yamlsDownSql() (*asset, error) {
	bytes, err := _000003_add_container_tmpl_yamlsDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000003_add_container_tmpl_yamls.down.sql", size: 68, mode: os.FileMode(436), modTime: time.Unix(1617081765, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000003_add_container_tmpl_yamlsUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x89\x0c\x70\x55\x48\x2c\x2a\xc9\x4c\x4b\x4c\x2e\x89\x2f\xa9\x2c\x48\x55\x70\x74\x71\x51\x08\x73\xf4\x09\x75\x55\x50\x77\xf6\xf7\x0b\x71\xf4\xf4\x73\x0d\x8a\x0f\x76\x0d\x89\x0f\x71\xf5\x0d\xf0\x71\x0c\x71\x8d\x8f\x74\xf4\xf5\x09\x56\xb7\xe6\x02\x04\x00\x00\xff\xff\xc8\x9f\x01\x18\x43\x00\x00\x00")

func _000003_add_container_tmpl_yamlsUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000003_add_container_tmpl_yamlsUpSql,
		"000003_add_container_tmpl_yamls.up.sql",
	)
}

func _000003_add_container_tmpl_yamlsUpSql() (*asset, error) {
	bytes, err := _000003_add_container_tmpl_yamlsUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000003_add_container_tmpl_yamls.up.sql", size: 67, mode: os.FileMode(436), modTime: time.Unix(1617081765, 0)}
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
	"000001_create_artifact_tables.down.sql":   _000001_create_artifact_tablesDownSql,
	"000001_create_artifact_tables.up.sql":     _000001_create_artifact_tablesUpSql,
	"000002_add_container_yamls.down.sql":      _000002_add_container_yamlsDownSql,
	"000002_add_container_yamls.up.sql":        _000002_add_container_yamlsUpSql,
	"000003_add_container_tmpl_yamls.down.sql": _000003_add_container_tmpl_yamlsDownSql,
	"000003_add_container_tmpl_yamls.up.sql":   _000003_add_container_tmpl_yamlsUpSql,
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
	"000001_create_artifact_tables.down.sql":   &bintree{_000001_create_artifact_tablesDownSql, map[string]*bintree{}},
	"000001_create_artifact_tables.up.sql":     &bintree{_000001_create_artifact_tablesUpSql, map[string]*bintree{}},
	"000002_add_container_yamls.down.sql":      &bintree{_000002_add_container_yamlsDownSql, map[string]*bintree{}},
	"000002_add_container_yamls.up.sql":        &bintree{_000002_add_container_yamlsUpSql, map[string]*bintree{}},
	"000003_add_container_tmpl_yamls.down.sql": &bintree{_000003_add_container_tmpl_yamlsDownSql, map[string]*bintree{}},
	"000003_add_container_tmpl_yamls.up.sql":   &bintree{_000003_add_container_tmpl_yamlsUpSql, map[string]*bintree{}},
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
