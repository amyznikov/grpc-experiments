/*
 * client.cc
 *
 *  Created on: Aug 12, 2016
 *      Author: amyznikov
 */
#include <unistd.h>
#include <syscall.h>
#include <thread>
#include <chrono>
#include <grpc/grpc.h>
#include <grpc++/grpc++.h>
#include "asyncio.grpc.pb.h"

using namespace grpc;
using namespace std;
using namespace asyncio;


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
  unique_ptr<AsyncTester::Stub> stub_;

public:
  Client(shared_ptr<Channel> channel)
      : stub_(AsyncTester::NewStub(channel))
  {
  }

  void test1() {
    ClientContext context;
    TestRequest req;
    TestResponce resp;
    Status status;

    //PDBG("start");

    req.set_clientmessage("This is client message");

    status = stub_->test1(&context, req, &resp);

//    PDBG("Finished: resp='%s' status=%d %s", resp.servermessage().c_str(), status.error_code(), status.error_message().c_str());
  }

  void test2() {
    ClientContext context;
    TestRequest req;
    TestResponce resp;
    Status status;

    PDBG("start");

    req.set_clientmessage("This is client message");

    unique_ptr< ClientReader< TestResponce>> rd = stub_->test2(&context, req);
    while ( rd->Read(&resp) ) {
      PDBG("resp: %s", resp.servermessage().c_str());
    }

    status = rd->Finish();

    PDBG("Finished: status=%d %s", status.error_code(), status.error_message().c_str());
  }

  void test3() {
    ClientContext context;
    TestRequest req;
    TestResponce resp;
    Status status;

    PDBG("start");

    unique_ptr<ClientWriter<TestRequest>> w = stub_->test3(&context, &resp);

    for ( int i = 0; i < 10; ++i ) {

      req.set_clientmessage(ssprintf("This is client message %d", i));

      PDBG("sending %s", req.clientmessage().c_str());

      if ( !w->Write(req) ) {
        PDBG("Write fails");
        break;
      }

      this_thread::sleep_for(chrono::milliseconds(250));
    }

    PDBG("C w->WritesDone()");
    w->WritesDone();

    PDBG("C w->Finish()");

    status = w->Finish();


    PDBG("Finished: resp='%s' status=%d %s", resp.servermessage().c_str(), status.error_code(),
        status.error_message().c_str());
  }

  void test4() {
    ClientContext context;
    TestRequest req;
    TestResponce resp;
    Status status;

    PDBG("start");

    unique_ptr<ClientReaderWriter<TestRequest,TestResponce>> stream = stub_->test4(&context);

    for ( int i = 0; i < 10; ++i ) {

      req.set_clientmessage(ssprintf("This is client message %d", i));

      PDBG("sending %s", req.clientmessage().c_str());

      if ( !stream->Write(req) ) {
        PDBG("Write fails");
        break;
      }

      if ( !stream->Read(&resp) ) {
        PDBG("Read fails");
        break;
      }

      PDBG("Got '%s'", resp.servermessage().c_str());

      this_thread::sleep_for(chrono::milliseconds(250));
    }

    PDBG("C w->WritesDone()");
    stream->WritesDone();

    PDBG("C w->Finish()");

    status = stream->Finish();

    PDBG("Finished: status=%d %s", status.error_code(), status.error_message().c_str());
  }

};


///////////////////////////////////////////////////////////////////

static void run_test(int n)
{
  shared_ptr<Channel> channel = CreateChannel("localhost:50051", InsecureChannelCredentials());

  Client client(channel);

  if ( n < 1 ) {

    client.test1();

    client.test2();

    client.test3();

    client.test4();
  }

  else {
    for ( int i = 0; i < n; ++i )
    {
      client.test1();
    }
  }

}



///////////////////////////////////////////////////////////////////

int main( int argc, char * argv[])
{
  int n = 0;

  for ( int i = 1; i < argc; ++i ) {
    if ( strncmp(argv[i],"n=", 2) ==0  ) {
      if ( sscanf(argv[i]+2, "%d", &n) != 1) {
        fprintf(stderr,"invalid value %s\n", argv[i]);
        return 1;
      }
    }
    else {
      fprintf(stderr,"invalid argument %s\n", argv[i]);
      return 1;
    }
  }

  grpc_init();

  run_test( n );

  grpc_shutdown();


  return 0;
}
