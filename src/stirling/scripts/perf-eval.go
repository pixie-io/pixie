package main

import (
	"bufio"
	"bytes"
	"context"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"os/user"
	"path/filepath"
	"strings"
	"time"

	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/kubernetes"
	_ "k8s.io/client-go/plugin/pkg/client/auth/gcp"

	"pixielabs.ai/pixielabs/src/utils/shared/k8s"
)

type cmdVerbosity int

const (
	silenced cmdVerbosity = iota
	cmdOnly
	cmdAndOutputs
	outputsOnly
)

type processParams struct {
	cmd         string
	input       string
	logFilePath string
	verbosity   cmdVerbosity
	unifyOutput bool
}

func check(e error) {
	if e != nil {
		panic(e)
	}
}

type exitCode struct{ Code int }

func handleExit() {
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

func exit(r int) {
	// This unrolls the defer stack through the use of panic,
	// but the error code is our custom type "exitCode."
	// At the last moment, main will invoke the deferred exit handler "handleExit(),"
	// which will find this custom type and rather than continuing the panic,
	// the program will exit gracefully with return value r.
	panic(exitCode{r})
}

func getUserName() string {
	userObj, err := user.Current()
	check(err)
	userName := userObj.Username
	return userName
}

func isDir(path string) bool {
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

func isExecutableFile(path string) bool {
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
	return isDir(path) || isFile(path)
}

func stringIsInSlice(a string, list []string) bool {
	// TODO(jps): do we have a lib function for this? Or make one in Stirling?
	for _, b := range list {
		if b == a {
			return true
		}
	}
	return false
}

func readLines(path string) []string {
	file, err := os.Open(path)
	check(err)
	defer file.Close()

	var lines []string
	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		lines = append(lines, scanner.Text())
	}
	check(scanner.Err())
	return lines
}

func wordSepNormalizeFunc(f *pflag.FlagSet, name string) pflag.NormalizedName {
	from := []string{"_"}
	to := "-"
	for _, sep := range from {
		name = strings.Replace(name, sep, to, -1)
	}
	return pflag.NormalizedName(name)
}

func makePaths(dstPath string) (string, string) {
	// Make the destination paths.
	repoPath := filepath.Join(dstPath, "repo")
	logsPath := filepath.Join(dstPath, "logs")

	// This is the go equiv. of mkdir -p:
	check(os.MkdirAll(repoPath, os.ModePerm))
	check(os.MkdirAll(logsPath, os.ModePerm))
	return repoPath, logsPath
}

func binaryIsInstalled(binary string) bool {
	// Use 'which' to see if the binary is in the path.
	// If so, we double check that the file exists and is executable.
	cmd := fmt.Sprintf("which %s || true", binary)
	verbosity := silenced
	stdout := runProcessUnified(cmd, "/dev/null", verbosity)
	return isExecutableFile(stdout)
}

func writeBytesToFile(bytes []byte, filePathAndName string) {
	// TODO(jps): Move to a utils file.
	f, err := os.Create(filePathAndName)
	check(err)
	defer f.Close()
	_, err = f.WriteString(string(bytes))
	check(err)
}

func runProcess(params processParams) (string, string) {
	// TODO(jps): Move to a utils file.
	if pathExists(params.logFilePath) {
		s := fmt.Sprintln("logFilePath:", params.logFilePath, "already exists.")
		panic(errors.New(s))
	}
	if params.verbosity == cmdOnly || params.verbosity == cmdAndOutputs {
		fmt.Println("running:", params.cmd)
	}

	bashCmd := exec.Command("bash", "-c", params.cmd)
	if params.input != "" {
		stdin, err := bashCmd.StdinPipe()
		check(err)

		go func() {
			defer stdin.Close()
			_, err := io.WriteString(stdin, params.input)
			check(err)
		}()
	}

	var outret, errret string

	// Get the output, write it to the logfile, and panic on any error:
	if params.unifyOutput {
		output, err := bashCmd.CombinedOutput()
		if params.verbosity == outputsOnly || params.verbosity == cmdAndOutputs {
			fmt.Print(string(output))
		}
		writeBytesToFile(output, params.logFilePath)
		check(err)
		outret = strings.Trim(string(output), "\n")
		errret = ""
	} else {
		stdErrLogFilePath := params.logFilePath + ".stderr"
		var stdout, stderr bytes.Buffer
		bashCmd.Stdout = &stdout
		bashCmd.Stderr = &stderr
		err := bashCmd.Run()
		if params.verbosity == outputsOnly || params.verbosity == cmdAndOutputs {
			fmt.Print(stdout.String())
			fmt.Print(stderr.String())
		}
		writeBytesToFile(stderr.Bytes(), stdErrLogFilePath)
		writeBytesToFile(stdout.Bytes(), params.logFilePath)
		check(err)
		outret = strings.Trim(stdout.String(), "\n")
		errret = strings.Trim(stderr.String(), "\n")
	}
	return outret, errret
}

func runProcessUnified(cmd string, logFilePath string, verbosity cmdVerbosity) string {
	// TODO(jps): Move to a utils file.
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

func runProcessSplit(cmd string, logFilePath string, verbosity cmdVerbosity) (string, string) {
	// TODO(jps): Move to a utils file.
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

func runProcessUnifiedWithStdin(cmd string, input string, logFilePath string, verbosity cmdVerbosity) string {
	// TODO(jps): Move to a utils file.
	params := processParams{
		cmd:         cmd,
		input:       input,
		logFilePath: logFilePath,
		verbosity:   verbosity,
		unifyOutput: false,
	}
	stdout, _ := runProcess(params)
	return stdout
}

func getSrcRepoPath() string {
	// We will rely on the user invoking this from inside of their repo.
	// And, we find the repo path by walking up until a sub-directory ".git" is found.
	p, _ := os.Getwd()
	gitPath := filepath.Join(p, ".git")

	for !isDir(gitPath) {
		p = filepath.Dir(p)
		gitPath = filepath.Join(p, ".git")

		if len(p) < 4 {
			// We expect to find the repo under some path such as /home/user/...
			// if len(p) is very short, then we've unwound the path too far.
			wd, _ := os.Getwd()
			fmt.Println("Could not find a git repo (.git subdir) above the current working directory:", wd)
			exit(-1)
		}
	}
	repoPath := filepath.Dir(gitPath)
	return repoPath
}

func getPerfEvalGoFilePath(srcPath string) string {
	// TODO(jps): Consider using os.Args[0] or otherwise making this more robust.
	perfEvalGoFilePath := filepath.Join(srcPath, "src/stirling/scripts/perf-eval.go")

	if !isFile(perfEvalGoFilePath) {
		fmt.Println("Could not find:", perfEvalGoFilePath)
		exit(-1)
	}
	return perfEvalGoFilePath
}

func findFullHash(hashPfx string) string {
	// Form a command using "git log" + "grep".
	gitLogCmd := fmt.Sprintf("git log | grep ^commit | cut -d' ' -f2 | grep ^%s", hashPfx)

	// Run the command.
	gitLog := exec.Command("bash", "-c", gitLogCmd)

	// Get the output, and fail if we found nothing.
	logOutBytes, err := gitLog.Output()
	if err != nil {
		fmt.Println("command failed:", gitLogCmd)
		fmt.Println("Does the hash", hashPfx, "exist on this branch?")
		exit(-1)
	}

	// Spit the output by newline.
	logOut := strings.Trim(string(logOutBytes), "\n")
	logOuts := strings.Split(logOut, "\n")

	// Fail if there are multiple matching hashes.
	// No need to check if len==0 because that is handled above.
	if len(logOuts) > 1 {
		s := fmt.Sprintln("found", len(logOuts), "matching hashes.")
		panic(errors.New(s))
	}

	// Return the full hash.
	fullHash := logOuts[0]
	return fullHash
}

func getPerfEvalDstRepoPath(dstPathPfx string, user string, timestamp time.Time, hash string, tag string) string {
	// Build a path in which we will place a sandboxed repo of our code, like:
	// {dst_path_pfx}/perf-evals/{user}/YYYY-MM-DD/HH.MM.SS/{hash}-{tag}

	if !isDir(dstPathPfx) {
		fmt.Println("Please ensure that path", dstPathPfx, "exists before running, or provide \"--dst-path=/your/path\" on the command line.")
		exit(-1)
	}

	dateString := timestamp.Format("2006-01-02")
	timeString := timestamp.Format("15.04.05")

	// Truncate the 40 char. hash to just 16 chars (64 bits).
	// 64 bits ought to be enough for anybody.
	shortHash := hash[:16]
	hashTag := fmt.Sprintf("%s-%s", shortHash, tag)
	dstPath := filepath.Join(dstPathPfx, "perf-evals", dateString, timeString, hashTag)
	return dstPath
}

func checkThatDirExists(path string) {
	if !isDir(path) {
		s := fmt.Sprintln("path", path, "does not exist.")
		panic(errors.New(s))
	}
}

func checkThatDirExistsAndIsEmpty(path string) {
	checkThatDirExists(path)

	f, err := os.Open(path)
	if err != nil {
		panic(err)
	}
	defer f.Close()

	filenames, err := f.Readdirnames(1)
	if len(filenames) > 0 {
		fmt.Println("baz")
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

func checkoutRepo(tag string, hash string, srcPath string, dstRepoPath string, dstLogsPath string) {
	// TODO(jps): Consider using package go-git.
	// Sanity check that we are git cloning into an empty path.
	checkThatDirExistsAndIsEmpty(dstRepoPath)

	// Chdir into dstRepoPath, otherwise we start operating on the _upstream_ repo.
	check(os.Chdir(dstRepoPath))

	// Clone the repo into dstRepoPath (recursively cloning the submodules),
	// create a new branch named {tag} using the commit hash.
	cloneLogFilePath := filepath.Join(dstLogsPath, "git-clone.log")
	checkoutLogFilePath := filepath.Join(dstLogsPath, "git-checkout.log")
	submoduleUpdateLogFilePath := filepath.Join(dstLogsPath, "git-submodule-update.log")

	verbosity := cmdOnly
	gitCloneCmd := fmt.Sprintf("git clone %s %s", srcPath, dstRepoPath)
	gitCheckoutCmd := fmt.Sprintf("git checkout -b %s %s", tag, hash)
	gitSubmoduleUpdateCmd := "git submodule update --init --recursive"

	fmt.Println("cloning repo...")
	runProcessUnified(gitCloneCmd, cloneLogFilePath, verbosity)
	runProcessUnified(gitCheckoutCmd, checkoutLogFilePath, verbosity)
	runProcessUnified(gitSubmoduleUpdateCmd, submoduleUpdateLogFilePath, verbosity)
}

func getClusterNamespaces() []string {
	config := k8s.GetConfig()

	client, err := kubernetes.NewForConfig(config)
	check(err)

	namespaceSpecs, err := client.CoreV1().Namespaces().List(context.Background(), metav1.ListOptions{})
	check(err)
	namespaces := make([]string, 0)
	for _, namespaceSpec := range namespaceSpecs.Items {
		namespaces = append(namespaces, namespaceSpec.ObjectMeta.Name)
	}
	return namespaces
}

func deployPx(srcRepoPath string, dstRepoPath string, dstLogsPath string) {
	// The commands we will run.
	deployCmd := "px deploy -y" // (optional)
	deleteCmd := "px delete"
	skaffoldCmd := "skaffold run -p opt -f skaffold/skaffold_vizier.yaml --cache-artifacts=false"

	// Log file paths, for the commands we will run.
	deployLogFilePath := filepath.Join(dstLogsPath, "px-deploy.log")
	deleteLogFilePath := filepath.Join(dstLogsPath, "px-delete.log")
	skaffoldLogFilePath := filepath.Join(dstLogsPath, "skaffold.log")

	// Verbosity option, for commands we will run.
	verbosity := cmdOnly

	namespaces := getClusterNamespaces()

	if !stringIsInSlice("pl", namespaces) {
		// There is no pre-existing px deployment; do that now:
		fmt.Println("running initial pixe deploy to setup the namespace...")
		runProcessUnified(deployCmd, deployLogFilePath, verbosity)
	}
	fmt.Println("deleting pixie deployment...")
	runProcessUnified(deleteCmd, deleteLogFilePath, verbosity)

	check(os.Chdir(dstRepoPath))
	fmt.Println("using skaffold to deploy pixie...")
	runProcessUnified(skaffoldCmd, skaffoldLogFilePath, verbosity)
	check(os.Chdir(srcRepoPath))
}

func deployDemoApps(dstLogsPath string) {
	namespaces := getClusterNamespaces()

	// Log file paths.
	deleteSockShopLogFilePath := filepath.Join(dstLogsPath, "delete-px-sock-shop.log")
	deploySockShopLogFilePath := filepath.Join(dstLogsPath, "deploy-px-sock-shop.log")
	deleteOnlineBoutiqueLogFilePath := filepath.Join(dstLogsPath, "delete-px-online-boutique.log")
	deployOnlineBoutiqueLogFilePath := filepath.Join(dstLogsPath, "deploy-px-online-boutique.log")

	// Verbosity option, for commands we will run.
	verbosity := cmdOnly

	fmt.Println("deploying demo apps...")

	if stringIsInSlice("px-sock-shop", namespaces) {
		// Delete px-sock-shop if it was running:
		runProcessUnified("px demo delete px-sock-shop -y", deleteSockShopLogFilePath, verbosity)
	}
	if stringIsInSlice("px-online-boutique", namespaces) {
		// Delete px-online-boutique if it was running:
		runProcessUnified("px demo delete px-online-boutique -y", deleteOnlineBoutiqueLogFilePath, verbosity)
	}

	// (re)start px-sock-shop & px-online-boutique.
	runProcessUnified("px demo deploy px-sock-shop -y", deploySockShopLogFilePath, verbosity)
	runProcessUnified("px demo deploy px-online-boutique -y", deployOnlineBoutiqueLogFilePath, verbosity)
}

func enqueueDataCollection(targetScript string, dstPath string, warmupMinutes int, evalMinutes int) {
	// TODO(jps) Consider using a k8s job for this.
	// Use 'at' to enqueue the data collection command at time now+warmup+eval.
	totalRunTime := warmupMinutes + evalMinutes
	atCmd := fmt.Sprintf("at now +%d minutes", totalRunTime)
	enqLogFilePath := filepath.Join(dstPath, "logs", "enqueue-data-collection.log")
	runLogFilePath := filepath.Join(dstPath, "logs", "run-data-collection.log")

	// This is the command we are scheduling in the future.
	// Parameter --minutes specifies how many minutes worth of data to collect.
	collectCmd := fmt.Sprintf("go run %s collect --eval-minutes %d --eval-path %s 1> %s 2>&1", targetScript, evalMinutes, dstPath, runLogFilePath)

	// Verbosity option for runProcessXYZ()
	verbosity := cmdOnly

	// Because of how "at" works, we pipe in the data collection command (collectCmd) through stdin.
	fmt.Println("scheduling", collectCmd, atCmd, "...")
	runProcessUnifiedWithStdin(atCmd, collectCmd, enqLogFilePath, verbosity)
}

func getLaunchFlagSet() *pflag.FlagSet {
	userHomeDir, err := os.UserHomeDir()
	check(err)

	fs := pflag.NewFlagSet("launch", pflag.ExitOnError)
	fs.Int("warmup-minutes", 60, "Number of minutes to wait until starting perf measurements.")
	fs.Int("eval-minutes", 30, "Window of time over which to collect perf data.")
	fs.StringP("hash", "h", "0000000000", "Hash of commit.")
	fs.StringP("tag", "t", "test", "A 'tag' describing this experiment.")
	fs.String("dst-path", userHomeDir, "Destination path for experiment.")
	return fs
}

func getCollectFlagSet() *pflag.FlagSet {
	fs := pflag.NewFlagSet("launch", pflag.ExitOnError)
	fs.Int("eval-minutes", 30, "Window of time over which to collect perf data.")
	fs.String("eval-path", "none", "Full destination path of the experiment.")
	return fs
}

func bindAndParseCmdLineArgs(fs *pflag.FlagSet) {
	pflag.CommandLine.AddFlagSet(fs)
	pflag.CommandLine.SetNormalizeFunc(wordSepNormalizeFunc)
	pflag.Parse()
	viper.BindPFlags(pflag.CommandLine)
}

func launch() {
	// Setup cmd line args.
	launchFlagSet := getLaunchFlagSet()
	bindAndParseCmdLineArgs(launchFlagSet)

	tag := viper.GetString("tag")
	hashPfx := viper.GetString("hash")
	dstPathPfx := viper.GetString("dst-path")
	evalMinutes := viper.GetInt("eval-minutes")
	warmupMinutes := viper.GetInt("warmup-minutes")

	timenow := time.Now()
	user := getUserName()
	hash := findFullHash(hashPfx)
	srcPath := getSrcRepoPath()
	dstPath := getPerfEvalDstRepoPath(dstPathPfx, user, timenow, hash, tag)
	perfEvalGoFilePath := getPerfEvalGoFilePath(srcPath)
	repoPath, logsPath := makePaths(dstPath)

	// Printing these (useful values) for info:
	fmt.Println("tag:", tag)
	fmt.Println("user:", user)
	fmt.Println("hash:", hash)
	fmt.Println("srcPath:", srcPath)
	fmt.Println("dstPath:", dstPath)
	fmt.Println("repoPath:", repoPath)
	fmt.Println("logsPath:", logsPath)
	fmt.Println("")

	checkoutRepo(tag, hash, srcPath, repoPath, logsPath)
	deployPx(srcPath, repoPath, logsPath)
	deployDemoApps(logsPath)
	enqueueDataCollection(perfEvalGoFilePath, dstPath, warmupMinutes, evalMinutes)
}

func collect() {
	// Setup cmd line args.
	collectFlagSet := getCollectFlagSet()
	bindAndParseCmdLineArgs(collectFlagSet)

	// Grab the args.
	evalPath := viper.GetString("eval-path")
	evalMinutes := viper.GetInt("eval-minutes")

	// Setup the "px" command, its log file, and our system verbosity level.
	cmd := fmt.Sprintf("px run pixielabs/pem_resource_usage -o json -- --start_time=-%dm", evalMinutes)
	perfLogFilePath := filepath.Join(evalPath, "logs", "perf.jsons")
	verbosity := silenced

	// Run the data collection.
	checkThatDirExists(evalPath)
	runProcessSplit(cmd, perfLogFilePath, verbosity)

	// Sanity check that we found some records.
	records := readLines(perfLogFilePath)
	numRecords := len(records)
	fmt.Printf("Found %d perf records, recorded in %s.\n", numRecords, perfLogFilePath)
}

func printUsageMessage() {
	launchFlagSet := getLaunchFlagSet()
	collectFlagSet := getCollectFlagSet()
	fmt.Println("go run perf-eval.go [launch|collect] <flags>")
	fmt.Println("")
	fmt.Println(launchFlagSet.FlagUsages())
	fmt.Println(collectFlagSet.FlagUsages())
	fmt.Println(pflag.CommandLine.FlagUsages())
}

func getCommand() string {
	nargs := len(os.Args)
	if nargs < 2 {
		printUsageMessage()
		exit(-1)
	}
	command := os.Args[1]
	return command
}

func sanityChecks() {
	// It seems "likely" that go & px are installed, but "at" may very well be missing.
	// And, if your normal dev. machine is down, and you switch environments
	// (say from enigma to turing) then px might also be missing.
	if !binaryIsInstalled("go") {
		fmt.Println("Please install go. How did you do this?!?")
		exit(-1)
	}
	if !binaryIsInstalled("px") {
		fmt.Println("Please install px.")
		fmt.Println("bash -c \"$(curl -fsSL https://withpixie.ai/install.sh)\"")
		exit(-1)
	}
	if !binaryIsInstalled("at") {
		fmt.Println("Please install the binary \"at\", possibly \"sudo apt install at\".")
		exit(-1)
	}
}

func main() {
	defer handleExit()
	sanityChecks()

	switch command := getCommand(); command {
	case "launch":
		launch()
	case "collect":
		collect()
	default:
		printUsageMessage()
	}
}
