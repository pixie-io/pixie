package utils

import (
	"bufio"
	"bytes"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"os/user"
	"strings"
)

type exitCode struct{ Code int }

// HandleExit finally called by Exit() because this was deferred in main.
func HandleExit() {
	// A more elegant way to panic, i.e. when we don't really want or need to see the stack trace.
	// If panic is called with our custom type "exitCode," then we directly
	// exit from here. Trick is, this is deferred at the top of main, so the rest of the
	// defer stack is unwound.
	if e := recover(); e != nil {
		if exit, ok := e.(exitCode); ok {
			os.Exit(exit.Code)
		}
		// not an Exit, bubble up:
		panic(e)
	}
}

// Exit is like os.Exit() but it will unwind the defer stack.
func Exit(r int) {
	// This unrolls the defer stack through the use of panic,
	// but the error code is our custom type "exitCode."
	// At the last moment, main will invoke the deferred exit handler "handleExit(),"
	// which will find this custom type and rather than continuing the panic,
	// the program will exit gracefully with return value r.
	panic(exitCode{r})
}

// Check will panic if e != nil.
func Check(e error) {
	if e != nil {
		panic(e)
	}
}

// StringIsInSlice returns true if it finds the target string in the slice.
func StringIsInSlice(a string, list []string) bool {
	// TODO(jps): do we have a lib function for this? Or make one in Stirling?
	for _, b := range list {
		if b == a {
			return true
		}
	}
	return false
}

func writeBytesToFile(bytes []byte, filePathAndName string) {
	// TODO(jps): Move to a utils file.
	f, err := os.Create(filePathAndName)
	Check(err)
	defer f.Close()
	_, err = f.WriteString(string(bytes))
	Check(err)
}

// ReadLines reads a file and splits it into lines.
func ReadLines(path string) []string {
	file, err := os.Open(path)
	Check(err)
	defer file.Close()

	var lines []string
	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		lines = append(lines, scanner.Text())
	}
	Check(scanner.Err())
	return lines
}

// GetUserName returns the user name or errors out.
func GetUserName() string {
	userObj, err := user.Current()
	Check(err)
	userName := userObj.Username
	return userName
}

// IsDir returns true if the directory exists.
func IsDir(path string) bool {
	fi, err := os.Stat(path)
	if err == nil {
		return fi.IsDir()
	}
	return false
}

func isExecAny(mode os.FileMode) bool {
	// Check if any of the +x bits are set:
	// i.e., true if any of these: u+x, g+x, o+x.
	return mode&0111 != 0
}

func isFile(path string) bool {
	fi, err := os.Stat(path)
	if err == nil {
		return fi.Mode().IsRegular()
	}
	return false
}

// IsExecutableFile returns true if the file exists & is executable.
func IsExecutableFile(path string) bool {
	fi, err := os.Stat(path)
	if err == nil {
		if fi.Mode().IsRegular() {
			// It's a file!
			// But is it executable?
			return isExecAny(fi.Mode())
		}
	}
	return false
}

func pathExists(path string) bool {
	return IsDir(path) || isFile(path)
}

// CheckThatDirExists will error out if the directory does not exist.
func CheckThatDirExists(path string) {
	if !IsDir(path) {
		s := fmt.Sprintln("path", path, "does not exist.")
		panic(errors.New(s))
	}
}

// CheckThatDirExistsAndIsEmpty will error out if the dir is not empty or does not exist.
func CheckThatDirExistsAndIsEmpty(path string) {
	CheckThatDirExists(path)

	f, err := os.Open(path)
	if err != nil {
		panic(err)
	}
	defer f.Close()

	filenames, err := f.Readdirnames(1)
	if len(filenames) > 0 {
		s := fmt.Sprintln("path", path, "is not empty.")
		panic(errors.New(s))
	}

	// By now, we now that there are no filenames returned by Readdirnames;
	// err should contain io.EOF.
	if err != io.EOF {
		fmt.Println("If path", path, "is empty (as appears to be the case) then the err code should io.EOF, but the err code is something else.")
		panic(err)
	}
}

// BinaryIsInstalled returns true if the binary is installed.
func BinaryIsInstalled(binary string) bool {
	// Use 'which' to see if the binary is in the path.
	// If so, we double check that the file exists and is executable.
	cmd := fmt.Sprintf("which %s || true", binary)
	verbosity := Silenced
	stdout := RunProcessUnified(cmd, "/dev/null", verbosity)
	return IsExecutableFile(stdout)
}

type processParams struct {
	cmd         string
	input       string
	logFilePath string
	verbosity   cmdVerbosity
	unifyOutput bool
}

type cmdVerbosity int

// Silenced is a command verbosity
const (
	Silenced cmdVerbosity = iota
	CmdOnly
	CmdAndOutputs
	OutputsOnly
)

func runProcess(params processParams) (string, string) {
	if pathExists(params.logFilePath) {
		s := fmt.Sprintln("logFilePath:", params.logFilePath, "already exists.")
		panic(errors.New(s))
	}
	if params.verbosity == CmdOnly || params.verbosity == CmdAndOutputs {
		fmt.Println("running:", params.cmd)
	}

	bashCmd := exec.Command("bash", "-c", params.cmd)
	if params.input != "" {
		stdin, err := bashCmd.StdinPipe()
		Check(err)

		go func() {
			defer stdin.Close()
			_, err := io.WriteString(stdin, params.input)
			Check(err)
		}()
	}

	var outret, errret string

	// Get the output, write it to the logfile, and panic on any error:
	if params.unifyOutput {
		output, err := bashCmd.CombinedOutput()
		if params.verbosity == OutputsOnly || params.verbosity == CmdAndOutputs {
			fmt.Print(string(output))
		}
		writeBytesToFile(output, params.logFilePath)
		Check(err)
		outret = strings.Trim(string(output), "\n")
		errret = ""
	} else {
		stdErrLogFilePath := params.logFilePath + ".stderr"
		var stdout, stderr bytes.Buffer
		bashCmd.Stdout = &stdout
		bashCmd.Stderr = &stderr
		err := bashCmd.Run()
		if params.verbosity == OutputsOnly || params.verbosity == CmdAndOutputs {
			fmt.Print(stdout.String())
			fmt.Print(stderr.String())
		}
		writeBytesToFile(stderr.Bytes(), stdErrLogFilePath)
		writeBytesToFile(stdout.Bytes(), params.logFilePath)
		Check(err)
		outret = strings.Trim(stdout.String(), "\n")
		errret = strings.Trim(stderr.String(), "\n")
	}
	return outret, errret
}

// RunProcessUnified runs a process with a unified log file for stderr & stdout.
func RunProcessUnified(cmd string, logFilePath string, verbosity cmdVerbosity) string {
	params := processParams{
		cmd:         cmd,
		input:       "",
		logFilePath: logFilePath,
		verbosity:   verbosity,
		unifyOutput: true,
	}
	stdout, _ := runProcess(params)
	return stdout
}

// RunProcessSplit runs a process splitting stderr & stdout to separate logfiles.
func RunProcessSplit(cmd string, logFilePath string, verbosity cmdVerbosity) (string, string) {
	params := processParams{
		cmd:         cmd,
		input:       "",
		logFilePath: logFilePath,
		verbosity:   verbosity,
		unifyOutput: false,
	}
	stdout, stderr := runProcess(params)
	return stdout, stderr
}
