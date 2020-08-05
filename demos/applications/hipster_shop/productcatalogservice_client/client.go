package main

import (
	"context"

	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc"
	pb "pixielabs.ai/pixielabs/demos/applications/hipster_shop/proto"
)

func main() {
	ctx := context.Background()
	addr := "localhost:3550"
	conn, err := grpc.Dial(addr, grpc.WithInsecure())
	if err != nil {
		return
	}
	defer conn.Close()
	client := pb.NewProductCatalogServiceClient(conn)

	_, err = client.ListProducts(ctx, &pb.Empty{})
	if err != nil {
		return
	}

	_, err = client.GetProduct(ctx, &pb.GetProductRequest{Id: "OLJCESPC7Z"})
	if err != nil {
		return
	}

	_, err = client.SearchProducts(ctx, &pb.SearchProductsRequest{Query: "typewriter"})
	if err != nil {
		return
	}

	log.Info("Client terminated")
}
