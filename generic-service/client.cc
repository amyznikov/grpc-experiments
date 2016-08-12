/*
 * client.cc
 *
 *  Created on: Aug 7, 2016
 *      Author: amyznikov
 */
#include <unistd.h>
#include <syscall.h>
#include <thread>
#include <chrono>
#include <grpc/grpc.h>
#include <grpc++/grpc++.h>
#include "smaster.grpc.pb.h"
#include "hello-service.grpc.pb.h"

using namespace grpc;
using namespace std;
using namespace smaster;
using namespace hello_service;




//////////////////////////////////////////////////////////////


static string ssprintf(const char * format, ...)
  __attribute__ ((__format__ (__printf__, 1, 2)));

static std::string ssprintf(const char * format, ...)
{
  va_list arglist;
  char buf[1024] = "";

  va_start(arglist, format);
  vsnprintf(buf, sizeof(buf) - 1, format, arglist);
  va_end(arglist);

  return buf;
}

static inline pid_t gettid(void)
{
  return (pid_t) syscall (SYS_gettid);
}

#define PDBG(...) \
    fprintf(stderr, "[%d] %s(): %6d ", gettid(), __func__, __LINE__), \
    fprintf(stderr, __VA_ARGS__), \
    fputc('\n', stderr), \
    fflush(stderr)



//////////////////////////////////////////////////////////////

class HelloClient {
  unique_ptr<Hello::Stub> stub_;
public:
  HelloClient(shared_ptr<Channel> channel);
  void sayHello();
};

HelloClient::HelloClient(shared_ptr<Channel> channel)
    : stub_(Hello::NewStub(channel))
{
}

void HelloClient::sayHello()
{
  ClientContext ctx;
  HelloRequest request;
  HelloReply resp;
  Status status;

  request.set_r1("test");
  request.set_r2("again");
  cout << "C sayHello()\n";

  ctx.AddMetadata("my_meta_key", "my_meta_value");

  status = stub_->sayHello(&ctx, request, &resp);
  cout << "R sayHello(): resp = " << resp.message() << " status = " << status.error_code() << " " << status.error_message() << endl;
}


//////////////////////////////////////////////////////////////

class MasterClient {
  unique_ptr<SMaster::Stub> stub_;

public:
  MasterClient(shared_ptr<Channel> channel);
  void authenticate();
};


MasterClient::MasterClient(shared_ptr<Channel> channel)
    : stub_(SMaster::NewStub(channel))
{
}

void MasterClient::authenticate()
{
  ClientContext ctx;
  AuthRequest req;
  AuthResponce resp;
  Status status;

  PDBG("C authenticate()");
  req.set_cimei("12345");
  status = stub_->authenticate(&ctx, req, &resp);
  PDBG("R authenticate(): resp=%s status=%d %s", resp.authmsg().c_str(), status.error_code(), status.error_message().c_str());

}


//////////////////////////////////////////////////////////////




static void runTest()
{
  // share single channel for both clients
  shared_ptr<Channel> channel = CreateChannel("localhost:50051", InsecureChannelCredentials());

  HelloClient helloClient(channel);
  MasterClient masterClient(channel);

  masterClient.authenticate();
  helloClient.sayHello();

//  masterClient.authenticate();
//  helloClient.sayHello();

}


int main()
{
  grpc_init();

  runTest();

  grpc_shutdown();

  return 0;
}
