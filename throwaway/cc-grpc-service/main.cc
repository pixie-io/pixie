#include <grpcpp/grpcpp.h>

#include <iostream>

#include "throwaway/golang-grpc-service/proto/greet.grpc.pb.h"

using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

class GreeterServiceImpl final : public greet::Greeter::Service {
  Status SayHello(ServerContext* context, const greet::HelloRequest* request,
                  greet::HelloReply* response) {
    std::string user_name(request->name());

    std::cout << "Request received for " << user_name << std::endl;

    response->set_message(std::string("Hello ") + user_name);
    return Status::OK;
  }
};

void RunServer() {
  std::string server_address("0.0.0.0:50051");
  std::cout << "Starting up the server on " << server_address << std::endl;

  GreeterServiceImpl service;
  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server now listening on " << server_address << std::endl;

  server->Wait();
}

int main(int argc, char** argv) {
  std::cout << "Welcome to the greeter C++ server!" << std::endl;
  RunServer();
  return 0;
}
