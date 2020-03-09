package main

import (
	"C"
	"unsafe"

	"bytes"
	"encoding/json"
	"time"

	flb "github.com/fluent/fluent-bit-go/output"
	"github.com/gogo/protobuf/types"
	"github.com/nats-io/nats.go"
	log "github.com/sirupsen/logrus"
	"github.com/vmihailenco/msgpack/v4"

	"pixielabs.ai/pixielabs/src/shared/cvmsgspb"
)

// V2CLogChannel is the name of the nats channel to publish logs to.
const V2CLogChannel = "v2c.log"

// NatsConn is an interface representing a nats.Conn, so that we can mock out nats.
type NatsConn interface {
	Publish(string, []byte) error
}

func init() {
	// Fluent-bit adds this weird time extension to every msgpack msg
	// we don't need to use this time value since the logs have the real time
	// so we just register the extension without a handler here.
	msgpack.RegisterExt(0, (*time.Time)(nil))
}

func msgMapToNatsPb(msg map[string]interface{}) ([]byte, error) {
	jsonBytes, err := json.Marshal(msg)
	if err != nil {
		return nil, err
	}
	logMessage := &cvmsgspb.VLogMessage{
		Data: jsonBytes,
	}
	any, err := types.MarshalAny(logMessage)
	if err != nil {
		return nil, err
	}
	natsMessage := &cvmsgspb.V2CMessage{
		Msg: any,
	}
	natsMsgBytes, err := natsMessage.Marshal()
	if err != nil {
		return nil, err
	}

	return natsMsgBytes, err
}

func connectNATS(natsURL string, natsPort string, tlsClientCert string, tlsClientKey string, tlsCACert string) (*nats.Conn, error) {
	var nc *nats.Conn
	nc, err := nats.Connect(natsURL,
		nats.ClientCert(tlsClientCert, tlsClientKey),
		nats.RootCAs(tlsCACert))

	if err != nil {
		log.WithError(err).Error("Failed to connect to nats.")
		return nil, err
	}

	return nc, nil
}

// FLBPluginRegister is part of the interface required of FLB plugins
//export FLBPluginRegister
func FLBPluginRegister(def unsafe.Pointer) int {
	return flb.FLBPluginRegister(def, "natstls", "NATS output plugin w/ TLS support.", flb.FLB_OUTPUT_NET|flb.FLB_IO_OPT_TLS)
}

// FLBPluginInit is part of the interface required of FLB plugins
//export FLBPluginInit
func FLBPluginInit(plugin unsafe.Pointer) int {
	// Example to retrieve an optional configuration parameter
	host := flb.FLBPluginConfigKey(plugin, "nats_host")
	port := flb.FLBPluginConfigKey(plugin, "nats_port")
	tlsClientCert := flb.FLBPluginConfigKey(plugin, "tls_client_cert_file")
	tlsClientKey := flb.FLBPluginConfigKey(plugin, "tls_client_key_file")
	tlsCACert := flb.FLBPluginConfigKey(plugin, "tls_ca_cert_file")
	if tlsClientCert == "" {
		log.Error("tls_client_cert_file is required\n")
		return flb.FLB_ERROR
	}
	if tlsClientKey == "" {
		log.Error("tls_client_key_file is required\n")
		return flb.FLB_ERROR
	}
	if tlsCACert == "" {
		log.Error("tls_ca_cert_file is required\n")
		return flb.FLB_ERROR
	}
	log.Infof("[nats_tls] host=%s, port=%s, tlsClientCert=%s, tlsClientKey=%s, tlsCACert=%s", host, port, tlsClientCert, tlsClientKey, tlsCACert)
	nc, err := connectNATS(host, port, tlsClientCert, tlsClientKey, tlsCACert)
	if err != nil {
		return flb.FLB_ERROR
	}
	flb.FLBPluginSetContext(plugin, nc)
	return flb.FLB_OK
}

// This function is for testing purposes since "import "C"" is not allowed in test files
func flbPluginFlushCtx(ctx, data unsafe.Pointer, length int, tag string) int {
	return FLBPluginFlushCtx(ctx, data, C.int(length), C.CString(tag))
}

// FLBPluginFlushCtx is part of the cgo interface required of FLB plugins.
//export FLBPluginFlushCtx
func FLBPluginFlushCtx(ctx, data unsafe.Pointer, length C.int, tag *C.char) int {
	buf := bytes.NewBuffer(C.GoBytes(data, C.int(length)))
	d := msgpack.NewDecoder(buf)
	nc := flb.FLBPluginGetContext(ctx).(NatsConn)
	for {
		// DecodeInterface pulls the next [time, map[string]interface{}] pair from the msgpack buffer.
		// An error means there are no more msgs in the buffer so we should break
		mpack, err := d.DecodeInterface()
		if err != nil {
			break
		}
		mpackArr := mpack.([]interface{})
		if len(mpackArr) != 2 {
			log.Errorf("Unpacked msgpack is not of valid format, expected length 2 []interface{} but got length: %d", len(mpackArr))
			return flb.FLB_ERROR
		}
		msg := mpackArr[1].(map[string]interface{})
		natsMsgBytes, err := msgMapToNatsPb(msg)
		if err != nil {
			log.WithError(err).Error("[nats_tls] Failed to marshal msgpack msg into NATS msg.")
			return flb.FLB_ERROR
		}
		err = nc.Publish(V2CLogChannel, natsMsgBytes)
		if err != nil {
			log.WithError(err).Error("[nats_tls] failed to publish to nats")
			return flb.FLB_ERROR
		}
	}

	return flb.FLB_OK
}

// FLBPluginExit is part of the cgo interface required of FLB plugins.
//export FLBPluginExit
func FLBPluginExit() int {
	return flb.FLB_OK
}

// NOTE(james): this is required.
func main() {
}
