package main

import (
	"bufio"
	"bytes"
	"encoding/binary"
	"flag"
	"fmt"
	"io/ioutil"
	"net/http"
	"os"
	"os/signal"
	"strings"
	"unsafe"

	"github.com/fatih/color"
	"github.com/iovisor/gobpf/bcc"
)

var (
	tracePID     int
	printEnabled bool
)

func init() {
	flag.IntVar(&tracePID, "pid", -1, "The pid to trace")
	flag.BoolVar(&printEnabled, "print", true, "Print output")
}

// Track the event type. Must match consts in the C file.
type EventType int32

const (
	// Addr Event (accept).
	ETSyscallAddr EventType = iota + 1
	// Write event.
	ETSyscallWrite
	// Close Event.
	ETSyscallClose
)

// The attributes of the SyscallEvent. Must match consts in the C file.
type Attributes struct {
	EvType  EventType
	Fd      int32
	Bytes   int32
	MsgSize int32
}

// SyscallWriteEvent is the BPF struct for a syscall.
type SyscallWriteEvent struct {
	Attr Attributes
	Msg  []byte
}

// MessageInfo stores a buffer for partial messages on a specific file descriptor.
type MessageInfo struct {
	SocketInfo []byte
	Buf        bytes.Buffer
}

func mustAttachKprobeToSyscall(m *bcc.Module, probeType int, syscallName string, probeName string) {
	fnName := bcc.GetSyscallFnName(syscallName)
	kprobe, err := m.LoadKprobe(probeName)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Failed to attach probe: %s\n", err)
		os.Exit(1)
	}

	if probeType == bcc.BPF_PROBE_ENTRY {
		err = m.AttachKprobe(fnName, kprobe, -1)
	} else {
		err = m.AttachKretprobe(fnName, kprobe, -1)
	}

	if err != nil {
		fmt.Fprintf(os.Stderr, "Failed to attach entry probe %s: %s\n", probeName, err)
		os.Exit(1)
	}
}

type requestHandler struct {
	FdMap map[int32]*MessageInfo
}

// HandleBPFEvent handles an event from the BPF trace code.
func (r *requestHandler) HandleBPFEvent(v []byte) {
	var ev SyscallWriteEvent
	// To save space on the perf buffer we write out only the number of bytes necessary for the request.
	// The attributes are used to figure out how big the message is, so we start by reading them first.
	// After that we can allocate a buffer to read the data.
	if err := binary.Read(bytes.NewBuffer(v), bcc.GetHostByteOrder(), &ev.Attr); err != nil {
		fmt.Fprintf(os.Stderr, "Failed to decode struct: %+v\n", err)
		return
	}
	// Now read the actual data.
	ev.Msg = make([]byte, ev.Attr.MsgSize)
	if err := binary.Read(bytes.NewBuffer(v[unsafe.Sizeof(ev.Attr):]), bcc.GetHostByteOrder(), &ev.Msg); err != nil {
		fmt.Fprintf(os.Stderr, "Failed to decode struct: %+v\n", err)
		return
	}

	// Based on the event type we:
	//   Insert on AcceptEvent.
	//   Append on Msg event.
	//   Delete and print on close.
	switch ev.Attr.EvType {
	case ETSyscallAddr:
		r.FdMap[ev.Attr.Fd] = &MessageInfo{
			SocketInfo: ev.Msg,
		}
	case ETSyscallWrite:
		if elem, ok := r.FdMap[ev.Attr.Fd]; ok {
			elem.Buf.Write(ev.Msg)
		}
	case ETSyscallClose:
		if msgInfo, ok := r.FdMap[ev.Attr.Fd]; ok {
			delete(r.FdMap, ev.Attr.Fd)

			// Decode in a go routine to avoid blocking the perf buffer read.
			go parseAndPrintMessage(msgInfo)
		} else {
			fmt.Fprintf(os.Stderr, "Missing request with FD: %d\n", ev.Attr.Fd)
			return
		}
	}
}

func parseAndPrintMessage(msgInfo *MessageInfo) {
	// We have the complete request so we try to parse the actual HTTP request.
	resp, err := http.ReadResponse(bufio.NewReader(&msgInfo.Buf), nil)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Failed to parse request\n")
		return
	}

	if printEnabled {
		body := resp.Body
		b, _ := ioutil.ReadAll(body)
		body.Close()
		fmt.Printf("StatusCode: %s, Len: %s, ContentType: %s, Body: %s\n",
			color.GreenString("%d", resp.StatusCode),
			color.GreenString("%d", resp.ContentLength),
			color.GreenString("%s", resp.Header["Content-Type"]),
			color.GreenString("%s", string(b)))
	}
}

func main() {
	flag.Parse()
	if tracePID < 0 {
		panic("Argument --pid needs to be specified")
	}
	bpfProgramResolved := strings.ReplaceAll(bpfProgram, "$PID", fmt.Sprintf("%d", tracePID))
	bccMod := bcc.NewModule(bpfProgramResolved, []string{})
	mustAttachKprobeToSyscall(bccMod, bcc.BPF_PROBE_ENTRY, "accept4", "syscall__probe_entry_accept4")
	mustAttachKprobeToSyscall(bccMod, bcc.BPF_PROBE_RETURN, "accept4", "syscall__probe_ret_accept4")
	mustAttachKprobeToSyscall(bccMod, bcc.BPF_PROBE_ENTRY, "write", "syscall__probe_write")
	mustAttachKprobeToSyscall(bccMod, bcc.BPF_PROBE_ENTRY, "close", "syscall__probe_close")

	// Create the output table named "golang_http_response_events" that the BPF program writes to.
	table := bcc.NewTable(bccMod.TableId("syscall_write_events"), bccMod)
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

	// Map from file descriptor the MessageInfo.
	requestHander := &requestHandler{
		FdMap: make(map[int32]*MessageInfo, 0),
	}

	for {
		select {
		case <-intCh:
			fmt.Println("Terminating")
			os.Exit(0)
		case v := <-ch:
			requestHander.HandleBPFEvent(v)
		}
	}
}
