package utils

import (
	"archive/tar"
	"io"
)

// ReadTarFileFromReader writes the file contents to a map where the key is the name
// of the file and the value contains its contents.
func ReadTarFileFromReader(r io.Reader) (map[string]string, error) {
	tarReader := tar.NewReader(r)

	fileMap := make(map[string]string)

	for {
		header, err := tarReader.Next()
		if err == io.EOF {
			break
		}
		if err != nil {
			return nil, err
		}
		if header.Typeflag == tar.TypeReg {
			fileMap[header.Name], err = readFileToString(tarReader, header.Size)
			if err != nil {
				return nil, err
			}
		}
	}
	return fileMap, nil
}

func readFileToString(tr *tar.Reader, numBytes int64) (string, error) {
	buf := make([]byte, numBytes)
	_, err := tr.Read(buf)
	if err != io.EOF {
		return "", err
	}
	return string(buf), nil
}
