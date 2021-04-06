package main

import (
	"context"
	"fmt"
	"os"

	"pixielabs.ai/pixielabs/src/api/go/pxapi"
	"pixielabs.ai/pixielabs/src/api/go/pxapi/errdefs"
	"pixielabs.ai/pixielabs/src/api/go/pxapi/types"
)

var (
	pxl = `
import px
df = px.DataFrame('http_events')
df = df[['upid', 'req_path', 'remote_addr', 'req_method']]
df = df.head(10)
px.display(df, 'http')
`
)

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

type tableMux struct {
}

func (s *tableMux) AcceptTable(ctx context.Context, metadata types.TableMetadata) (pxapi.TableRecordHandler, error) {
	return &tablePrinter{}, nil
}

func main() {
	apiKey, ok := os.LookupEnv("PX_API_KEY")
	if !ok {
		panic("please set PX_API_KEY")
	}
	clusterID, ok := os.LookupEnv("PX_CLUSTER_ID")
	if !ok {
		panic("please set PX_CLUSTER_ID")
	}

	ctx := context.Background()
	client, err := pxapi.NewClient(ctx, pxapi.WithAPIKey(apiKey))
	if err != nil {
		panic(err)
	}

	fmt.Printf("Running on Cluster: %s\n", clusterID)

	tm := &tableMux{}

	fmt.Println("Running script")
	vz, err := client.NewVizierClient(ctx, clusterID)
	if err != nil {
		panic(err)
	}

	resultSet, err := vz.ExecuteScript(ctx, pxl, tm)
	if err != nil {
		panic(err)
	}

	defer resultSet.Close()
	if err := resultSet.Stream(); err != nil {
		if errdefs.IsCompilationError(err) {
			fmt.Printf("Got compiler error: \n %s\n", err.Error())
		} else {
			fmt.Printf("Got error : %+v, while streaming\n", err)
		}
	}

	stats := resultSet.Stats()
	fmt.Printf("Execution Time: %v\n", stats.ExecutionTime)
	fmt.Printf("Bytes received: %v\n", stats.TotalBytes)
}
