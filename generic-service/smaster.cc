/*
 * smaster.cc
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
#include "bridge-service.h"

#include "smaster.grpc.pb.h"

using namespace grpc;
using namespace std;
using namespace smaster;


///////////////////////////////////////////////////////////////////

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



///////////////////////////////////////////////////////////////////

class SMasterService GRPC_FINAL
    : public SMaster::Service {

  Status authenticate(ServerContext* context, const AuthRequest* request, AuthResponce* response) override
  {
    (void)(context);
    PDBG("CIMEI=%s", request->cimei().c_str());
    response->set_authmsg(ssprintf("Hello %s", request->cimei().c_str()));
    return Status::OK;
  }

};


///////////////////////////////////////////////////////////////////
class BridgeService
    : public GenericBridgeService
{

public:
  BridgeService() {
    addChannel("/hello_service.Hello", CreateChannel("localhost:50052",
            InsecureChannelCredentials()));
  }

};


///////////////////////////////////////////////////////////////////

static void run()
{
  string server_address("0.0.0.0:50051");

  SMasterService master;
  BridgeService bridge;

  ServerBuilder builder;
  builder.AddListeningPort(server_address,InsecureServerCredentials());

  builder.RegisterService(&master);


  builder.RegisterAsyncGenericService(bridge);
  bridge.setCompletionQueue(builder.AddCompletionQueue());


  unique_ptr<Server> server = builder.BuildAndStart();
  cout << "Server listening on " << server_address << endl;

  bridge.run();
}

int main()
{
  grpc_init();
  grpc_tracer_set_enabled("all", 1);
  run();
  grpc_shutdown();

  return 0;
}
