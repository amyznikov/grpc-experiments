/*
 * hello-service.cc
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
#include "hello-service.grpc.pb.h"

using namespace grpc;
using namespace std;
using namespace hello_service;

///////////////////////////////////////////////////////////////////

static string sprintf(const char * format, ...)
  __attribute__ ((__format__ (__printf__, 1, 2)));

static string sprintf(const char * format, ...)
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


///////////////////////////////////////////////////////////////////

class HelloService final : public Hello::Service {

  Status sayHello(ServerContext* context, const HelloRequest* request, HelloReply* response) override
  {
    PDBG("I GOT IT!!!!");

    // this_thread::sleep_for(chrono::seconds(5));

    PDBG("Sending reply");
    response->set_message(sprintf("hello from thread %d", gettid()));
    return Status::OK;
  }

};


///////////////////////////////////////////////////////////////////

int main()
{

  std::string server_address("0.0.0.0:50052");

  HelloService helloService;

  ServerBuilder builder;

  builder.AddListeningPort(server_address, InsecureServerCredentials());
  builder.RegisterService(&helloService);

  unique_ptr<Server> server(builder.BuildAndStart());
  cout << "Server listening on " << server_address << std::endl;
  server->Wait();

  return 0;
}




