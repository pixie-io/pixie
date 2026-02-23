// Code generated for package schemas by go-bindata DO NOT EDIT. (@generated)
// sources:
// bazel-out/k8-fastbuild/bin/src/e2e_test/vizier/planner/dump_schemas/data/schema.pb
package schemas

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

var _srcE2e_testVizierPlannerDump_schemasDataSchemaPb = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\xcc\x57\x3d\x8f\x2b\x35\x14\xd5\x24\x9b\xec\xe6\xcc\x24\x3b\xc9\xee\xa3\xce\x2f\xe0\x5f\xd0\xd3\x22\x21\x59\x5e\x8f\x93\xcc\xcb\x8c\xed\xb5\x3d\x79\x09\xd4\xc0\x1f\x80\x86\x6a\x25\x3a\x90\x80\x86\x06\x0a\x84\x90\x10\x42\x54\x34\x4f\x48\x08\x89\x8e\xe6\x09\x90\x28\x28\x91\x3d\x1f\x49\x26\xab\x04\x21\x24\xd2\xc5\xc7\x37\xd7\xf7\x5e\x9f\x73\x7d\x07\x7f\x04\x18\xe5\x52\xcc\x65\x72\x47\xf8\x8a\x0b\x6b\x26\x3f\x07\x08\xd1\xb3\x69\xce\x49\xdc\x9f\xba\xc5\x45\xa1\xd2\x24\xee\x4e\xbf\x0d\x30\x41\xa8\x79\x2e\x2d\x27\x34\x49\x74\xdc\x9b\x7e\x7f\xb5\x83\x29\xa9\x6d\xdc\x99\x7e\x3a\xc0\x18\xc8\x24\xa3\xd9\x8e\x59\x03\x6d\xad\x62\xc0\x6a\xca\x38\xd1\x32\xe3\x71\x67\x1a\xe0\x1a\x03\x2e\x98\xde\x28\xcb\x93\x38\x98\x06\x18\xe2\x52\xf3\x7b\xc2\xf2\x24\xee\x4d\x03\x8c\x70\xe5\x96\x77\x32\xd9\xf8\xf5\xd8\x9d\x6d\x14\x31\x96\xda\xc2\x78\xe8\x1a\x03\x0f\x35\x36\x23\x5c\x66\xd4\x72\xc1\x36\x71\x67\xfa\xd6\x25\xbe\x0e\x10\x0b\x6a\x4d\x95\xf0\xcb\x77\xdc\xd2\xc9\xc7\x67\x95\xf5\x00\xdd\x3a\x63\xe0\xa2\xc9\x04\xb8\x70\xa9\xf9\xdf\x1c\x63\x63\x29\x5b\x12\xef\xab\xca\xe2\xd5\x23\x49\xdc\x62\xb4\xf3\x07\x92\x26\xfe\xe8\x31\xc2\x1d\xd4\x7b\x0e\xd1\x63\xb2\x10\xd6\xef\xff\x1e\x00\x89\xa8\x6b\x35\xf9\xe5\xac\xca\x14\x03\x8e\x0d\x0b\x4e\x13\xae\x8f\xf2\x63\xc7\xe4\x24\x3f\x7e\x0b\x10\xe5\x1b\x73\x9f\x9d\xbd\x22\x3a\xa7\x15\xd1\xf9\x27\x19\xbf\x86\x58\x69\xc9\x08\x5f\xa7\xb6\x4e\xfa\x95\x23\x39\xbb\x98\x9c\x29\x93\x49\x19\x64\x84\xbe\x49\xe7\x82\x66\x7e\x05\x5c\x30\x99\xe7\xfe\x28\xe1\x58\x97\xea\x2c\x15\x73\xc2\xb5\x96\x7a\xf2\xfa\x11\xc7\x2f\x21\x36\xb2\xd0\x8c\x13\x26\x85\xe0\xcc\xca\xf2\xce\x9c\xff\x6d\x3e\x21\x7a\xde\x95\xdf\x1a\xe2\x92\x49\x61\xf9\xda\xfa\xe5\x67\x01\x86\x82\xdb\x67\x52\x2f\x7d\x09\xcc\xe4\x83\xd6\xed\x45\xe8\x2b\x99\x38\xfe\x97\x6c\xb8\xd2\x6b\x72\xb7\xb1\xdc\xf9\x7e\xe8\x7b\x4e\xad\x89\xa2\x6c\xc9\xed\xb6\x7c\xeb\x32\x78\xd3\x94\x7c\x4d\x12\x2d\x55\x6d\x70\x65\xdb\x3e\xec\x81\x0f\xdb\xf6\x61\x77\x7d\xbc\x08\x00\x97\x73\x15\xf4\x7f\x42\xb9\x43\x32\x8d\x11\xba\xbf\x90\x19\xcd\xd3\x6c\x53\x07\xa2\xb4\xb4\x92\xc9\xf2\xf2\x06\xe8\x1a\x93\x79\xa6\x5d\x63\xe0\x43\x92\x8a\x0b\xbf\x17\x57\x31\xb2\x4c\x9a\xc6\xa1\x47\x28\xb3\xe9\xaa\x86\xe0\x4b\x41\x0c\xf7\x3d\xe4\xa1\xbf\x85\x34\x67\xab\x12\x7a\x1e\x20\xd2\x3c\x49\x9b\xce\xf2\xcd\xb9\x8a\x6c\xa7\xad\x50\x3d\x37\x07\x8d\xb8\x2d\xa6\x37\x11\x29\x2d\xef\x78\xa5\xc0\xc9\xf2\x5f\xf0\xbd\x8e\x50\xc9\x54\xd8\x53\x0a\x00\x2e\x52\x31\x93\xfe\xf7\x77\x01\x60\xb3\xa6\xa8\x5f\xfc\x9f\x45\xad\x8a\x66\x37\x8a\x3f\xda\xa9\x4e\xb6\xa5\x2f\x3b\x08\x69\x7e\xaf\xea\x6c\x3e\xe9\x9c\x13\x45\x62\x60\xa6\x69\xce\xb7\x09\xba\x56\xb4\xa0\x42\xf0\x52\x49\x13\xc7\xf0\x7b\xc2\x32\x6a\x4c\xfd\xdc\xde\x60\xe8\xb0\x9c\xdb\x45\xd9\x83\x1a\xd0\xa8\x7d\xcb\x5b\x8c\x3c\xb8\x6f\x5a\xf1\x32\x37\xf3\x2d\x2f\x9d\x51\xb3\xde\xaf\xe0\x7b\x01\x06\x4f\x57\x79\xd5\x54\xde\x39\xc6\x86\x5b\x0c\x37\xb2\x10\x73\x32\x67\xc4\x99\x94\x0e\x6e\x10\xcd\x8a\x2c\xdb\x07\x6f\x31\x2a\x0c\x4f\xdc\xa3\xaa\x88\x49\xdf\x28\xb3\x7f\x82\x6b\x2b\x2d\xcd\x5a\xf0\x0d\x86\x39\x5d\xb7\xc0\xcf\x03\x20\x2f\xd6\xf5\xc5\x7e\x74\x56\xda\x3f\xe4\xed\x7e\x51\x7f\x08\x10\xa9\xf9\xce\x7c\xf0\xd5\x59\x85\xdf\x6a\x5d\x03\x74\x35\xbf\x3f\xd9\xb5\x3e\xec\x22\x5c\x58\xdb\x68\xed\xfd\xee\x39\xe5\xe4\x49\xf4\x54\x6a\xb2\xe2\xda\xa4\x52\x34\xcc\x4a\x45\x0b\x9c\x20\xf2\xe3\x80\xb0\xdb\x0b\xf4\x23\x51\x3d\x26\x9a\x6a\x4a\xc2\x56\x87\x71\x6f\xfa\x2e\xea\x6b\x57\xd4\x2e\x1e\x1d\x25\x6f\x4b\xe9\xba\x75\x4d\xe4\x87\x7e\x29\xf2\x66\xc0\x2c\x9d\x4f\xda\x23\xd8\x5f\x70\x42\xaa\xe4\x6c\x0c\x9d\xbb\x41\xfb\xa7\xf0\xb0\x03\x3e\xa9\x44\xdf\x3a\xa4\x7d\x59\x6f\x77\x30\x74\x03\x1b\x37\xa6\x92\xf6\x9f\xc7\x29\x18\x95\xd5\x9b\xd1\x22\xab\x06\x12\x87\xf9\xe2\xed\x60\x37\x88\x98\x2a\x48\xe1\xdd\x08\xd3\x74\x00\x07\x2e\xf7\xc0\x31\x42\x51\xe4\xc4\x2e\x34\xa7\x49\xed\x30\x5c\xb9\x88\xf7\xe7\xa0\x81\x36\x66\x17\x71\xa5\x61\x0b\xaa\x5b\xd8\xb3\x36\xe6\xef\x87\x26\x6d\x33\x9d\xda\xbd\x03\x5e\x04\x18\x2f\xe9\x6c\x49\xf7\xbe\xe5\x9e\x9f\xab\x1e\xab\x7d\x96\xa5\x8e\x9f\xe9\xe3\x9f\xb4\xc7\x54\xfa\xab\x9b\x12\xb7\x8d\xe7\xc7\xb3\x4a\x34\x42\xdf\x25\x22\xd5\xa3\xaf\xbd\x2f\x83\x51\xf5\xf6\xa9\xc7\xff\xef\x00\x00\x00\xff\xff\xc5\x63\xc8\x13\x9a\x10\x00\x00")

func srcE2e_testVizierPlannerDump_schemasDataSchemaPbBytes() ([]byte, error) {
	return bindataRead(
		_srcE2e_testVizierPlannerDump_schemasDataSchemaPb,
		"src/e2e_test/vizier/planner/dump_schemas/data/schema.pb",
	)
}

func srcE2e_testVizierPlannerDump_schemasDataSchemaPb() (*asset, error) {
	bytes, err := srcE2e_testVizierPlannerDump_schemasDataSchemaPbBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "src/e2e_test/vizier/planner/dump_schemas/data/schema.pb", size: 0, mode: os.FileMode(0), modTime: time.Unix(0, 0)}
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
	"src/e2e_test/vizier/planner/dump_schemas/data/schema.pb": srcE2e_testVizierPlannerDump_schemasDataSchemaPb,
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
	"src": &bintree{nil, map[string]*bintree{
		"e2e_test": &bintree{nil, map[string]*bintree{
			"vizier": &bintree{nil, map[string]*bintree{
				"planner": &bintree{nil, map[string]*bintree{
					"dump_schemas": &bintree{nil, map[string]*bintree{
						"data": &bintree{nil, map[string]*bintree{
							"schema.pb": &bintree{srcE2e_testVizierPlannerDump_schemasDataSchemaPb, map[string]*bintree{}},
						}},
					}},
				}},
			}},
		}},
	}},
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
