package pxconfig

import (
	"encoding/json"
	"os"
	"os/user"
	"path/filepath"
	"sync"

	"github.com/gofrs/uuid"

	utils "pixielabs.ai/pixielabs/src/pixie_cli/pkg/utils"
)

// ConfigInfo store the config about the CLI.
type ConfigInfo struct {
	// UniqueClientID is the ID assigned to this user on first startup when auth information is not know. This can be later associated with the UserID.
	UniqueClientID string `json:"uniqueClientID"`
}

// TODO(zasgar): Reconcile with auth.
const (
	pixieDotPath    = ".pixie"
	pixieConfigFile = "config.json"
)

var (
	config *ConfigInfo
	once   sync.Once
)

// ensureDefaultConfigFilePath returns and creates the file path is missing.
func ensureDefaultConfigFilePath() (string, error) {
	u, err := user.Current()
	if err != nil {
		return "", err
	}

	pixieDirPath := filepath.Join(u.HomeDir, pixieDotPath)
	if _, err := os.Stat(pixieDirPath); os.IsNotExist(err) {
		err = os.Mkdir(pixieDirPath, 0744)
		if err != nil {
			return "", err
		}
	}

	pixieConfigFilePath := filepath.Join(pixieDirPath, pixieConfigFile)
	return pixieConfigFilePath, nil
}

func writeDefaultConfig(path string) (*ConfigInfo, error) {
	f, err := os.OpenFile(path, os.O_RDWR|os.O_CREATE, 0600)
	if err != nil {
		return nil, err
	}
	defer f.Close()

	clientID, err := uuid.NewV4()
	if err != nil {
		return nil, err
	}

	cfg := &ConfigInfo{UniqueClientID: clientID.String()}
	if err := json.NewEncoder(f).Encode(cfg); err != nil {
		return nil, err
	}
	return cfg, nil
}

func readDefaultConfig(path string) (*ConfigInfo, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer f.Close()

	cfg := &ConfigInfo{}
	if err := json.NewDecoder(f).Decode(cfg); err != nil {
		return nil, err
	}
	return cfg, nil
}

// Cfg returns the default config.
func Cfg() *ConfigInfo {
	once.Do(func() {
		configPath, err := ensureDefaultConfigFilePath()
		if err != nil {
			utils.WithError(err).Error("Failed to load/create config file path")
			os.Exit(1)
		}
		_, err = os.Stat(configPath)
		if os.IsNotExist(err) {
			// Write the default config.
			if config, err = writeDefaultConfig(configPath); err != nil {
				utils.WithError(err).Error("Failed to create default config")
				os.Exit(1)
			}
			return
		}

		if config, err = readDefaultConfig(configPath); err != nil {
			utils.WithError(err).Error("Failed to read config file")
			os.Exit(1)
		}
	})
	return config
}
