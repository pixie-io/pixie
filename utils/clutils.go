package utils

// Utils to interact with the command line.
// Built for easy streaming of stderr and stdout
// from the command call to the go stderr/stdout.
// Uses logrus for logging.

import (
	"bufio"
	"fmt"
	"io"
	"os"
	"os/exec"
	"os/signal"
	"strings"
	"syscall"

	log "github.com/sirupsen/logrus"
)

// MakeCommand makes Cmd struct from string into executable form.
func MakeCommand(cmdString string) *exec.Cmd {
	args := strings.Fields(cmdString)
	cmd := exec.Command(args[0], args[1:]...)
	return cmd
}

// ScanStream reads in a stream and writes to stdout async. Good for stdout from exec.Cmd.
func ScanStream(stream io.ReadCloser, write func(...interface{})) {
	scanner := bufio.NewScanner(stream)
	scanner.Split(bufio.ScanLines)
	go func() {
		for scanner.Scan() {
			for _, emp := range strings.Split(scanner.Text(), "\\n") {
				write(emp)
			}
		}
	}()
}

// addSignalInterruptCatch adds a catch for keyboard interrupt. Useful if you want to interrupt another process before exiting a script.
func addSignalInterruptCatch(action func()) {
	c := make(chan os.Signal, 1)
	signal.Notify(c, os.Interrupt)
	go func() {
		for range c {
			// sig is a ^C, handle it
			action()
		}
	}()
}

// RunCmd runs command and add stdout/stderr buffers that pass to the go output.
func RunCmd(cmd *exec.Cmd) error {
	stderr, err := cmd.StderrPipe()
	if err != nil {
		return err
	}
	ScanStream(stderr, log.Warning)

	stdout, err := cmd.StdoutPipe()
	if err != nil {
		return err
	}
	ScanStream(stdout, log.Info)

	err = cmd.Start()
	if err != nil {
		return err
	}
	addSignalInterruptCatch(func() {
		fmt.Println("Sending SIGINT")
		cmd.Process.Signal(syscall.SIGINT)
	})

	err = cmd.Wait()
	if err != nil {
		return err
	}

	return nil
}
