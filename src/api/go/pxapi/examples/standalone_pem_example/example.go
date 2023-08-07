package main

import (
	"context"
	"fmt"
	"io"
	"os"
	"px.dev/pixie/src/api/go/pxapi"
	"px.dev/pixie/src/api/go/pxapi/errdefs"
	"px.dev/pixie/src/api/go/pxapi/types"
)

// Define PxL script with one table output.
var (
pxl = `
import px
df = px.DataFrame('tcp_stats_events', start_time='-1m')
df = df[['tx', 'rx', 'local_addr', 'remote_addr']]
px.display(df.stream(), 'tcp_stats')
`
)

func main() {
	hostID, ok := os.LookupEnv("PX_HOST_ID")
	if !ok {
		panic("please set PX_HOST_ID")
	}

	// Create a Pixie client with local standalonePEM listening address
	ctx := context.Background()
	client, err := pxapi.NewClient(ctx, pxapi.WithDirectAddr("0.0.0.0:12345"))
	if err != nil {
		panic(err)
	}
	// Create a connection to the host.
	vz, err := client.NewVizierClient(ctx, hostID)
	if err != nil {
		panic(err)
	}
	// Create TableMuxer to accept results table.
	tm := &tableMux{}
	// Execute the PxL script.
	resultSet, err := vz.ExecuteScript(ctx, pxl, tm)
	if err != nil && err != io.EOF {
		panic(err)
	}
	// Receive the PxL script results.
	defer resultSet.Close()
	if err := resultSet.Stream(); err != nil {
		if errdefs.IsCompilationError(err) {
			fmt.Printf("Got compiler error: \n %s\n", err.Error())
		} else {
			fmt.Printf("Got error : %+v, while streaming\n", err)
		}
	}
	// Get the execution stats for the script execution.
	stats := resultSet.Stats()
	fmt.Printf("Execution Time: %v\n", stats.ExecutionTime)
	fmt.Printf("Bytes received: %v\n", stats.TotalBytes)
}

// Satisfies the TableRecordHandler interface.
type tablePrinter struct{}
func (t *tablePrinter) HandleInit(ctx context.Context, metadata types.TableMetadata) error {
		return nil
}
func (t *tablePrinter) HandleRecord(ctx context.Context, r *types.Record) error {
	for _, d := range r.Data {
		fmt.Printf("%s ", d.String())
	}
	fmt.Printf("\n")
	return nil
}

func (t *tablePrinter) HandleDone(ctx context.Context) error {
	return nil
}

// Satisfies the TableMuxer interface.
type tableMux struct {
}

func (s *tableMux) AcceptTable(ctx context.Context, metadata types.TableMetadata) (pxapi.TableRecordHandler, error) {
	return &tablePrinter{}, nil
}
