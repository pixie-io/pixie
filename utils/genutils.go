package utils

// A general set of utils that are useful throughout the project
import (
	"bufio"
	"errors"
	"fmt"
	"os"
	"strings"
)

// FindBazelWorkspaceRoot Finds the Workspace directory as specified by bazel.
func FindBazelWorkspaceRoot() (string, error) {
	workspaceDir, exists := os.LookupEnv("BUILD_WORKSPACE_DIRECTORY")
	if exists {
		return workspaceDir, nil
	}
	return workspaceDir, errors.New("WORKSPACE Directory could not be found")
}

// GetStdinInput Gets the stdin from the terminal.
func GetStdinInput(message string) (string, error) {
	reader := bufio.NewReader(os.Stdin)
	fmt.Print(message)
	text, err := reader.ReadString('\n')
	text = strings.Trim(text, "\n")
	return text, err
}

// FileExists checks whether a string path exists.
func FileExists(filePath string) bool {
	_, err := os.Stat(filePath)
	return !os.IsNotExist(err)
}
