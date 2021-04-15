package main

import (
	"context"
	"errors"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"time"

	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/kubernetes"
	_ "k8s.io/client-go/plugin/pkg/client/auth/gcp"

	"pixielabs.ai/pixielabs/src/stirling/scripts/utils"
	"pixielabs.ai/pixielabs/src/utils/shared/k8s"
)

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
	utils.Check(os.MkdirAll(repoPath, os.ModePerm))
	utils.Check(os.MkdirAll(logsPath, os.ModePerm))
	return repoPath, logsPath
}

func getSrcRepoPath() string {
	// We will rely on the user invoking this from inside of their repo.
	// And, we find the repo path by walking up until a sub-directory ".git" is found.
	p, _ := os.Getwd()
	gitPath := filepath.Join(p, ".git")

	for !utils.IsDir(gitPath) {
		p = filepath.Dir(p)
		gitPath = filepath.Join(p, ".git")

		if len(p) < 4 {
			// We expect to find the repo under some path such as /home/user/...
			// if len(p) is very short, then we've unwound the path too far.
			wd, _ := os.Getwd()
			fmt.Println("Could not find a git repo (.git subdir) above the current working directory:", wd)
			utils.Exit(-1)
		}
	}
	repoPath := filepath.Dir(gitPath)
	return repoPath
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
		utils.Exit(-1)
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

	if !utils.IsDir(dstPathPfx) {
		fmt.Println("Please ensure that path", dstPathPfx, "exists before running, or provide \"--dst-path=/your/path\" on the command line.")
		utils.Exit(-1)
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

func checkoutRepo(tag string, hash string, srcPath string, dstRepoPath string, dstLogsPath string) {
	// TODO(jps): Consider using package go-git.
	// Sanity check that we are git cloning into an empty path.
	utils.CheckThatDirExistsAndIsEmpty(dstRepoPath)

	// Chdir into dstRepoPath, otherwise we start operating on the _upstream_ repo.
	utils.Check(os.Chdir(dstRepoPath))

	// Clone the repo into dstRepoPath (recursively cloning the submodules),
	// create a new branch named {tag} using the commit hash.
	cloneLogFilePath := filepath.Join(dstLogsPath, "git-clone.log")
	checkoutLogFilePath := filepath.Join(dstLogsPath, "git-checkout.log")
	submoduleUpdateLogFilePath := filepath.Join(dstLogsPath, "git-submodule-update.log")

	verbosity := utils.CmdOnly
	gitCloneCmd := fmt.Sprintf("git clone %s %s", srcPath, dstRepoPath)
	gitCheckoutCmd := fmt.Sprintf("git checkout -b %s %s", tag, hash)
	gitSubmoduleUpdateCmd := "git submodule update --init --recursive"

	fmt.Println("cloning repo...")
	utils.RunProcessUnified(gitCloneCmd, cloneLogFilePath, verbosity)
	utils.RunProcessUnified(gitCheckoutCmd, checkoutLogFilePath, verbosity)
	utils.RunProcessUnified(gitSubmoduleUpdateCmd, submoduleUpdateLogFilePath, verbosity)
}

func getClusterNamespaces() []string {
	config := k8s.GetConfig()

	client, err := kubernetes.NewForConfig(config)
	utils.Check(err)

	namespaceSpecs, err := client.CoreV1().Namespaces().List(context.Background(), metav1.ListOptions{})
	utils.Check(err)
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
	verbosity := utils.CmdOnly

	namespaces := getClusterNamespaces()

	if !utils.StringIsInSlice("pl", namespaces) {
		// There is no pre-existing px deployment; do that now:
		fmt.Println("running initial pixe deploy to setup the namespace...")
		utils.RunProcessUnified(deployCmd, deployLogFilePath, verbosity)
	}
	fmt.Println("deleting pixie deployment...")
	utils.RunProcessUnified(deleteCmd, deleteLogFilePath, verbosity)

	utils.Check(os.Chdir(dstRepoPath))
	fmt.Println("using skaffold to deploy pixie...")
	utils.RunProcessUnified(skaffoldCmd, skaffoldLogFilePath, verbosity)
	utils.Check(os.Chdir(srcRepoPath))
}

func deployDemoApps(dstLogsPath string) {
	namespaces := getClusterNamespaces()

	// Log file paths.
	deleteSockShopLogFilePath := filepath.Join(dstLogsPath, "delete-px-sock-shop.log")
	deploySockShopLogFilePath := filepath.Join(dstLogsPath, "deploy-px-sock-shop.log")
	deleteOnlineBoutiqueLogFilePath := filepath.Join(dstLogsPath, "delete-px-online-boutique.log")
	deployOnlineBoutiqueLogFilePath := filepath.Join(dstLogsPath, "deploy-px-online-boutique.log")

	// Verbosity option, for commands we will run.
	verbosity := utils.CmdOnly

	fmt.Println("deploying demo apps...")

	if utils.StringIsInSlice("px-sock-shop", namespaces) {
		// Delete px-sock-shop if it was running:
		utils.RunProcessUnified("px demo delete px-sock-shop -y", deleteSockShopLogFilePath, verbosity)
	}
	if utils.StringIsInSlice("px-online-boutique", namespaces) {
		// Delete px-online-boutique if it was running:
		utils.RunProcessUnified("px demo delete px-online-boutique -y", deleteOnlineBoutiqueLogFilePath, verbosity)
	}

	// (re)start px-sock-shop & px-online-boutique.
	utils.RunProcessUnified("px demo deploy px-sock-shop -y", deploySockShopLogFilePath, verbosity)
	utils.RunProcessUnified("px demo deploy px-online-boutique -y", deployOnlineBoutiqueLogFilePath, verbosity)
}

func runDataCollection(dstPath string, warmupMinutes int, evalMinutes int) {
	totalWaitingMinutes := warmupMinutes + evalMinutes

	// This will print to the file "run-data-collection.log":
	timenow := time.Now()
	dateString := timenow.Format("2006-01-02")
	timeString := timenow.Format("15:04:05")
	fmt.Println("runDataCollection(): warmupMinutes:", warmupMinutes)
	fmt.Println("runDataCollection(): evalMinutes:", evalMinutes)
	fmt.Println("runDataCollection(): time now:", dateString, timeString)
	fmt.Println("runDataCollection(): sleeping for", totalWaitingMinutes, "minutes.")

	time.Sleep(time.Duration(totalWaitingMinutes) * time.Minute)
	timenow = time.Now()
	dateString = timenow.Format("2006-01-02")
	timeString = timenow.Format("15:04:05")
	fmt.Println("runDataCollection(): time now:", dateString, timeString)
	fmt.Println("")

	// Setup the "px" command, its log file, and our system verbosity level.
	cmd := fmt.Sprintf("px run pixielabs/pem_resource_usage -o json -- --start_time=-%dm", evalMinutes)
	perfLogFilePath := filepath.Join(dstPath, "logs", "perf.jsons")
	verbosity := utils.Silenced

	// Run the data collection.
	utils.CheckThatDirExists(dstPath)
	utils.RunProcessSplit(cmd, perfLogFilePath, verbosity)

	// Sanity check that we found some records.
	records := utils.ReadLines(perfLogFilePath)
	numRecords := len(records)
	fmt.Printf("Found %d perf records, recorded in %s.\n", numRecords, perfLogFilePath)
}

func startDataCollectionProcess(srcPath string, dstPath string, warmupMinutes int, evalMinutes int) {
	// TODO(jps) Consider using a k8s job for this.

	binaryFilePath := os.Args[0]

	if !utils.IsExecutableFile(binaryFilePath) {
		fmt.Println("Could not find an executable binary from os.Args[0]:", binaryFilePath)
		utils.Exit(-1)
	}

	runLogFilePath := filepath.Join(dstPath, "logs", "run-data-collection.log")
	collectCmd := fmt.Sprintf("%s collect --warmup-minutes %d --eval-minutes %d --dst-path %s 1> %s 2>&1", binaryFilePath, warmupMinutes, evalMinutes, dstPath, runLogFilePath)
	cmd := exec.Command("bash", "-c", collectCmd)
	utils.Check(cmd.Start())

	// Useful message to show the log file.
	// Also, it's nice to have an extra space spearating this out from the rest of stdout.
	fmt.Println("")
	fmt.Println("Started data collection process in the background, see log file:")
	fmt.Println(runLogFilePath)
}

func getLaunchFlagSet() *pflag.FlagSet {
	userHomeDir, err := os.UserHomeDir()
	utils.Check(err)

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
	fs.Int("warmup-minutes", 60, "Number of minutes to wait until starting perf measurements.")
	fs.Int("eval-minutes", 30, "Window of time over which to collect perf data.")
	fs.String("dst-path", "none", "Full destination path of the experiment.")
	return fs
}

func bindAndParseCmdLineArgs(targetFlagSet *pflag.FlagSet) {
	pflag.CommandLine.AddFlagSet(targetFlagSet)
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
	user := utils.GetUserName()
	hash := findFullHash(hashPfx)
	srcPath := getSrcRepoPath()
	dstPath := getPerfEvalDstRepoPath(dstPathPfx, user, timenow, hash, tag)
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
	startDataCollectionProcess(srcPath, dstPath, warmupMinutes, evalMinutes)
}

func collect() {
	// Setup cmd line args.
	collectFlagSet := getCollectFlagSet()
	bindAndParseCmdLineArgs(collectFlagSet)

	// Grab the args.
	dstPath := viper.GetString("dst-path")
	evalMinutes := viper.GetInt("eval-minutes")
	warmupMinutes := viper.GetInt("warmup-minutes")

	runDataCollection(dstPath, warmupMinutes, evalMinutes)
}

func printUsageMessage() {
	launchFlagSet := getLaunchFlagSet()
	collectFlagSet := getCollectFlagSet()
	fmt.Println("./perf-eval.sh [launch|collect] <flags>")
	fmt.Println("")
	fmt.Println(launchFlagSet.FlagUsages())
	fmt.Println(collectFlagSet.FlagUsages())
	fmt.Println(pflag.CommandLine.FlagUsages())
}

func getCommand() string {
	nargs := len(os.Args)
	if nargs < 2 {
		printUsageMessage()
		utils.Exit(-1)
	}
	command := os.Args[1]
	return command
}

func sanityChecks() {
	// Although this will not fire often, it's nice to check.
	// Say you move from enigma to turing and your dev. env. isn't fully setup...
	if !utils.BinaryIsInstalled("px") {
		fmt.Println("Please install px.")
		fmt.Println("bash -c \"$(curl -fsSL https://withpixie.ai/install.sh)\"")
		utils.Exit(-1)
	}
}

func main() {
	defer utils.HandleExit()
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
