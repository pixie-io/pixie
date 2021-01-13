package main

import (
	"fmt"
	"math"
	"runtime"
	"time"

	"crypto/tls"
	log "github.com/sirupsen/logrus"
	"golang.org/x/net/context"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials"
	pb "pixielabs.ai/pixielabs/src/stirling/source_connectors/socket_tracer/protocols/http2/testing/proto"

	"github.com/spf13/pflag"
	"github.com/spf13/viper"
)

const (
	address = "127.0.0.1:50051"
)

func main() {
	pflag.Int("max_procs", 1, "The maximum number of OS threads created by the golang runtime.")
	pflag.Uint32("iters", 100000, "Number of requests to make for each TLS connection.")
	pflag.Uint32("rounds", 1, "Number of connection rounds to perform.")
	pflag.Uint32("sleep_time", 1000, "Sleep time in microseconds per iteration.")
	pflag.Bool("benchmark", false, "Whether to run in benchmarking mode")
	pflag.Uint32("run_time_seconds", 0, "Number of seconds for which to run")
	pflag.Parse()

	viper.BindPFlags(pflag.CommandLine)

	runtime.GOMAXPROCS(viper.GetInt("max_procs"))

	benchmarkMode := viper.GetBool("benchmark")
	sleepTime := time.Duration(viper.GetInt("sleep_time")) * time.Millisecond
	iters := viper.GetInt("iters")
	rounds := viper.GetInt("rounds")
	runTimeSeconds := time.Duration(viper.GetInt("run_time_seconds")) * time.Second

	if benchmarkMode {
		log.Printf("Running in benchmark mode. sleep_time and run_time_seconds will be overridden.")
		sleepTime = 0
		runTimeSeconds = time.Duration(10) * time.Second
	}

	// When running by time, set the number of iterations to max/infinity.
	if runTimeSeconds != 0 {
		log.Printf("Running by duration. iters flag will be ignored.")
		iters = math.MaxUint32
	}

	for i := 0; i < rounds; i++ {
		// Set up a connection to the server.
		tlsConfig := &tls.Config{InsecureSkipVerify: true}
		creds := credentials.NewTLS(tlsConfig)
		conn, err := grpc.Dial(address, grpc.WithTransportCredentials(creds))
		if err != nil {
			log.Fatalf("did not connect: %v", err)
		}
		defer conn.Close()
		c := pb.NewGreeterClient(conn)

		start := time.Now()
		statRequests := 0

		for j := 0; j < iters; j++ {
			ctx, cancel := context.WithTimeout(context.Background(), time.Second)
			defer cancel()

			name := fmt.Sprintf("world %d", j)
			resp, err := c.SayHello(ctx, &pb.HelloRequest{Name: name})
			if err != nil {
				log.Errorf("could not greet: %v", err)
			} else {
				if benchmarkMode {
					statRequests++
				} else {
					log.Printf("Greeting: %s", resp.Message)
				}
			}
			if sleepTime != 0 {
				time.Sleep(sleepTime)
			}

			if runTimeSeconds != 0 && time.Since(start) > runTimeSeconds {
				break
			}
		}

		// Print summary stats.
		duration := time.Since(start)
		fmt.Println("Elapsed time = ", duration.Seconds())
		fmt.Println("Requests per second = ", float64(statRequests)/duration.Seconds())
	}

}
