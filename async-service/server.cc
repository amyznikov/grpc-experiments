/*
 * server.cc
 *
 *  Created on: Aug 12, 2016
 *      Author: amyznikov
 */
#include <unistd.h>
#include <syscall.h>
#include <thread>
#include <chrono>
#include "asyncio.grpc.pb.h"
#include <grpc++/alarm.h>
#include "async-call-handler.h"

using namespace grpc;
using namespace std;
using namespace asyncio;


///////////////////////////////////////////////////////////////////

static string ssprintf(const char * format, ...)
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


class AsyncTestService
  : public AsyncCallHandler<AsyncTester::AsyncService> {

public:

  AsyncTestService() {
    AddMethod(&NormalRpcTest::OnCall, &AsyncTester::AsyncService::Requesttest1);
    AddMethod(&ServerStreamingTest::OnCall, &AsyncTester::AsyncService::Requesttest2);
    AddMethod(&ClientStreamingTest::OnCall, &AsyncTester::AsyncService::Requesttest3);
    AddMethod(&BidiStreamingTest::OnCall, &AsyncTester::AsyncService::Requesttest4);
  }

private:

  ///////

  class NormalRpcTest {
    TestResponce responce;
  public:
    bool OnCall(bool fok, ServerContext* context, ServerCompletionQueue * cq, const TestRequest * request,
        ServerAsyncResponseWriter<TestResponce>* response_writer, void * tag)
    {
      (void)(fok);
      (void) (context);
      (void) (cq);
      //PDBG("GOT %s", request->clientmessage().c_str());
      responce.set_servermessage("This is a server message");
      response_writer->Finish(responce, Status::OK, tag);
      return true;
    }
  };









  ///////

  class ServerStreamingTest {

    TestResponce responce;
    unique_ptr<Alarm> alarm;

    int i;
    enum {
      state_run = 0,
      state_wait_alarm = 1,
    } state;

  public:

    ServerStreamingTest()
        : i(0), state(state_run)
    {
    }

    bool OnCall(bool fok, ServerContext* context, ServerCompletionQueue * cq, const TestRequest* request,
        ServerAsyncWriter<TestResponce>* writer, void * tag)
    {
      (void) (context);
      (void) (cq);

      bool finish = false;

      if ( i == 0 ) {
        PDBG("GOT %s", request->clientmessage().c_str());
      }

      if ( i == 10 || !fok ) {
        finish = true;
        writer->Finish(Status::OK, tag);
      }
      else
      {
        switch ( state )
        {
        case state_wait_alarm:
          alarm.reset(new Alarm(cq, chrono::system_clock::now() + chrono::milliseconds(200), tag));
          state = state_run;
          break;

        case state_run:
          ++i;
          responce.set_servermessage(ssprintf("This is a server message number %d", i));
          writer->Write(responce, tag);
          state = state_wait_alarm;
          break;
        }
      }

      return finish;
    }
  };










  ///////

  class ClientStreamingTest {
    TestRequest request;
    TestResponce resp;
    size_t cummulative_request_length;
    enum {
      state_start = 0,
      state_reading = 1,
      state_finishing = 2,
    } state;

  public:
    ClientStreamingTest()
        : cummulative_request_length(0), state (state_start)
    {}

    bool OnCall(bool fok, ServerContext* context, ServerCompletionQueue * cq,
        ServerAsyncReader<TestResponce, TestRequest>* reader, void * tag)
    {
      (void)(context);
      (void)(cq);

      bool finish = false;

      switch ( state )
      {
      case state_start:
        reader->Read(&request, tag);
        state = state_reading;
        break;
      case state_reading:
        if ( fok ) {
          cummulative_request_length += request.clientmessage().size();
          reader->Read(&request, tag);
        }
        else {
          state = state_finishing;
          resp.set_servermessage(ssprintf("Cummulative string size is %zu", cummulative_request_length));
          reader->Finish(resp, Status::OK, tag);
        }
        break;

      case state_finishing:
        finish = true;
        break;
      }

      return finish;
    }
  };





  ///////

  class BidiStreamingTest {

    TestRequest req;
    TestResponce resp;

    enum {
      state_read_request,
      state_send_responce,
    } state;


  public:

    BidiStreamingTest()
        : state(state_read_request)
    {
    }

    bool OnCall(bool fok, ServerContext * context, ServerCompletionQueue * cq,
        grpc::ServerAsyncReaderWriter<TestResponce, TestRequest> * stream, void *tag)
    {
      (void)(context);
      (void)(cq);

      bool finish = !fok;

      if ( finish ) {
        stream->Finish(Status::OK, tag);
      }
      else {

        switch ( state )
        {
        case state_read_request:
          stream->Read(&req, tag);
          state = state_send_responce;
          break;

        case state_send_responce: {

          string s = req.clientmessage();
          reverse(s.begin(), s.end());

          resp.set_servermessage(s);

          stream->Write(resp, tag);
          state = state_read_request;
          }
          break;
        }
      }

      return finish;
    }
  };
    ///////

};





///////////////////////////////////////////////////////////////////


static void run_test()
{
  ServerBuilder builder;
  AsyncTestService testService;

  builder.AddListeningPort("0.0.0.0:50051", InsecureServerCredentials());

  builder.RegisterService(&testService);
  testService.setCompletionQueue(builder.AddCompletionQueue());

  unique_ptr<Server> server = builder.BuildAndStart();
  testService.run();

  //server->Wait();
}


int main()
{
  grpc_init();
    run_test();
  grpc_shutdown();

  return 0;
}
