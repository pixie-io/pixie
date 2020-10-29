package main

/****************************************
 * We need to manually add some functionality to the gobpf so we can add by address.
 * This is simply a copy paste of those files.
 ****************************************/

/*
#cgo CFLAGS: -I/usr/include/bcc/compat
#cgo LDFLAGS: -lbcc
#include <bcc/bcc_common.h>
#include <bcc/libbpf.h>
#include <bcc/bcc_syms.h>
*/
import "C"
import (
	"fmt"
	"unsafe"
)

type bccSymbol struct {
	name         *C.char
	demangleName *C.char
	module       *C.char
	offset       C.ulonglong
}

// resolveSymbolPath returns the file and offset to locate symname in module
func resolveSymbolPath(module string, symname string, addr uint64, pid int) (string, uint64, error) {
	if pid == -1 {
		pid = 0
	}

	modname, offset, err := bccResolveSymname(module, symname, addr, pid)
	if err != nil {
		return "", 0, fmt.Errorf("unable to locate symbol %s in module %s: %v", symname, module, err)
	}

	return modname, offset, nil
}

func bccResolveSymname(module string, symname string, addr uint64, pid int) (string, uint64, error) {
	symbol := &bccSymbol{}
	symbolC := (*C.struct_bcc_symbol)(unsafe.Pointer(symbol))
	moduleCS := C.CString(module)
	defer C.free(unsafe.Pointer(moduleCS))
	symnameCS := C.CString(symname)
	defer C.free(unsafe.Pointer(symnameCS))

	res, err := C.bcc_resolve_symname(moduleCS, symnameCS, (C.uint64_t)(addr), C.int(pid), nil, symbolC)
	if res < 0 {
		return "", 0, fmt.Errorf("unable to locate symbol %s in module %s: %v", symname, module, err)
	}

	return C.GoString(symbolC.module), (uint64)(symbolC.offset), nil
}

func AttachUProbeWithAddr(evName string, attachType uint32, path string, addr uint64, fd, pid int) error {
	path, addr, err := resolveSymbolPath(path, "", addr, pid)
	if err != nil {
		return err
	}

	evNameCS := C.CString(evName)
	binaryPathCS := C.CString(path)
	res, err := C.bpf_attach_uprobe(C.int(fd), attachType, evNameCS, binaryPathCS, (C.uint64_t)(addr), (C.pid_t)(pid))
	C.free(unsafe.Pointer(evNameCS))
	C.free(unsafe.Pointer(binaryPathCS))

	if res < 0 {
		return fmt.Errorf("failed to attach BPF uprobe: %v", err)
	}
	return nil
}
