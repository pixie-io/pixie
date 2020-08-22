package main

import (
	"encoding/binary"
	"flag"
	"fmt"
	"github.com/iovisor/gobpf/bcc"
	"os"
	"os/signal"
)

const bpfProgram = `
#include <uapi/linux/ptrace.h>

BPF_PERF_OUTPUT(trace);

// This function will be registered to be called everytime
// main.computeE is called.
inline int computeECalled(struct pt_regs *ctx) {
  // The input argument is stored in ax.
  long val = ctx->ax;
  trace.perf_submit(ctx, &val, sizeof(val));
  return 0;
}
`

var binaryProg string
func init() {
	flag.StringVar(&binaryProg, "binary", "", "The binary to probe")
}

func main() {
	flag.Parse()
	if len(binaryProg) == 0 {
		panic("Argument --binary needs to be specified")
	}

	bccMod := bcc.NewModule(bpfProgram, []string{})
	uprobeFD, err := bccMod.LoadUprobe("computeECalled")
	if err != nil {
		panic(err)
	}

	// Attach the uprobe to be called everytime main.computeE is called.
	// We need to specify the path to the binary so it can be patched.
	err = bccMod.AttachUprobe(binaryProg, "main.computeE", uprobeFD, -1)
	if err != nil {
		panic(err)
	}

	// Create the output table named "trace" that the BPF program writes to.
	table := bcc.NewTable(bccMod.TableId("trace"), bccMod)
	ch := make(chan []byte)

	pm, err := bcc.InitPerfMap(table, ch, nil)
	if err != nil {
		panic(err)
	}

	// Watch Ctrl-C so we can quit this program.
	intCh := make(chan os.Signal, 1)
	signal.Notify(intCh, os.Interrupt)

	pm.Start()
	defer pm.Stop()

	for {
		select {
		case <-intCh:
			fmt.Println("Terminating")
			os.Exit(0)
		case v := <-ch:
			// This is a bit of hack, but we know that iterations is a
			// 8 bytes int64 value.
			d := binary.LittleEndian.Uint64(v)
			fmt.Printf("Value = %v\n", d)
		}
	}
}
