/*
 * client.cc
 *
 *  Created on: Aug 15, 2016
 *      Author: amyznikov
 */

#include <unistd.h>
#include <syscall.h>
#include <thread>
#include <chrono>
#include <grpc/grpc.h>
#include <grpc++/grpc++.h>
#include "server-to-client-notifications.grpc.pb.h"

using namespace grpc;
using namespace std;
using namespace test;


///////////////////////////////////////////////////////////////////

static string  ssprintf(const char * format, ...)
  __attribute__ ((__format__ (__printf__, 1, 2)));

static string ssprintf(const char * format, ...)
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

class Client {
  unique_ptr<StreamTester::Stub> stub_;

public:
  Client(shared_ptr<Channel> channel)
      : stub_(StreamTester::NewStub(channel))
  {
  }

  void listenServerEvents() {

    ClientContext context;
    ServerNotification e;

    PDBG("Start Listen");

    unique_ptr<ClientReader<ServerNotification>> reader = stub_->ListenNotifications(&context,
        ListenNotificationsRequest());

    while ( reader->Read(&e) ) {
      PDBG("Event: name=%s details=%s", e.event().c_str(), e.details().c_str());
    }

    Status status = reader->Finish();

    PDBG("Listen finished. Status=%d (%s)", status.error_code(), status.error_message().c_str());
  }

};



///////////////////////////////////////////////////////////////////


static void run_test()
{
  Client client(CreateChannel("localhost:50051", InsecureChannelCredentials()));
  client.listenServerEvents();
}


int main()
{
  grpc_init();
    run_test();
  grpc_shutdown();

  return 0;
}

