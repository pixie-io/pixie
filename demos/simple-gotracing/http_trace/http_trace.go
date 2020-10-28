package main

import "C"
import (
	"bytes"
	"encoding/binary"
	"flag"
	"fmt"
	"os"
	"os/signal"
	"unsafe"

	"github.com/fatih/color"
	"github.com/iovisor/gobpf/bcc"
)

/*
#cgo CFLAGS: -I/usr/include/bcc/compat
#cgo LDFLAGS: -lbcc
#include <bcc/bcc_common.h>
#include <bcc/libbpf.h>
#include <bcc/bcc_syms.h>
*/
import "C"

// GoHTTPResponseEvent is the struct written to the perf buffer. It needs to back exactly with definition in the BPF program.
type GoHTTPResponseEvent struct {
	StatusCode uint64
	URILen     uint64
	URI        [64]byte

	MethodLen uint64
	Method    [8]byte

	MsgLen uint64
	Msg    [128]byte
}

var (
	binaryProg string
	printEnabled bool
)

func init() {
	flag.StringVar(&binaryProg, "binary", "", "The binary to probe")
	flag.BoolVar(&printEnabled, "print", true, "Print output")

}

func main() {
	flag.Parse()
	if len(binaryProg) == 0 {
		panic("Argument --binary needs to be specified")
	}

	bccMod := bcc.NewModule(bpfProgram, []string{})
	uprobeFD, err := bccMod.LoadUprobe("probe_golang_http_response")
	if err != nil {
		panic(err)
	}

	// Attach the uprobe to be called everytime main.computeE is called.
	// We need to specify the path to the binary so it can be patched.
	// We ideally want to patch return of net/http.(*response).finishRequest, but this
	// does not work with Go. Lookup for a fix for this soon. For now,
	// we manually patch the correct address which is at the end of the function.
	//
	// The simplest way to get this address after the main part of the function is executed. You can run:
	//    objdump -d app
	// Find the net/http.(*response).finishRequest function, and then pick the address
	// after the call to bufio.(*Writer).Flush.
	//
	// 0000000000632be0 <net/http.(*response).finishRequest>:
	//      632be0:       64 48 8b 0c 25 f8 ff    mov    %fs:0xfffffffffffffff8,%rcx
	//      ...
	//      632c5c:       e8 ef e9 ed ff          callq  511650 <bufio.(*Writer).Flush>
	//  --> 632c61:       48 8b 44 24 28          mov    0x28(%rsp),%rax
	//      ...
	//
	//
	probeAddr := uint64(0x0000000000632c61)
	evName := "gobpf_uprobe"
	err = AttachUProbeWithAddr(evName, bcc.BPF_PROBE_ENTRY, binaryProg, probeAddr, uprobeFD, -1)
	if err != nil {
		panic(err)
	}

	defer func() {
		evNameCS := C.CString(evName)
		C.bpf_detach_uprobe(evNameCS)
		C.free(unsafe.Pointer(evNameCS))
	}()

	// Create the output table named "golang_http_response_events" that the BPF program writes to.
	table := bcc.NewTable(bccMod.TableId("golang_http_response_events"), bccMod)
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
			var parsed GoHTTPResponseEvent
			if err := binary.Read(bytes.NewBuffer(v), bcc.GetHostByteOrder(), &parsed); err != nil {
				fmt.Printf("Failed to decode struct\n")
			} else if printEnabled {
				fmt.Printf("Method: %s, URI: %s, Status: %s, ReturnMsg: %s\n",
					color.GreenString("%s", string(parsed.Method[:])),
					color.GreenString("%s", string(parsed.URI[:])),
					color.GreenString("%d", parsed.StatusCode),
					color.GreenString("%s", string(parsed.Msg[:])))
			}
		}
	}
}
