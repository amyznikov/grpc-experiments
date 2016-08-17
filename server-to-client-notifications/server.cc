/*
 * server.cc
 *
 *  Created on: Aug 15, 2016
 *      Author: amyznikov
 */

#include <unistd.h>
#include <syscall.h>
#include <thread>
#include <chrono>
#include <algorithm>
#include <vector>
#include <grpc/grpc.h>
#include <grpc++/grpc++.h>
#include "server-to-client-notifications.grpc.pb.h"
#include "async-call-handler.h"

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

class NotifyService
  : public AsyncCallHandler<StreamTester::AsyncService>
{
public:

  NotifyService();
  void broadcast(const ServerNotification & notify);

private:

  // Handles client's ListenNotifications() request in single thread
  class NotifySink {

    enum {
      initial_state = 0,
      wait_event = 1,
      send_notify = 2,
      finishing = 3,
      finished = 4,
    } state;

    NotifyService * service;
    ServerAsyncWriter<ServerNotification> * writer;
    void * tag;

  public:

    NotifySink(AsyncCallHandler<StreamTester::AsyncService> * service);
    ~NotifySink();

    void sendNotify(const ServerNotification & notify);

    bool OnCqCallback(bool fok, ServerContext* context, ServerCompletionQueue * cq,
        const ListenNotificationsRequest * request, ServerAsyncWriter<ServerNotification>* writer,
        void * tag);
  };

private:
  mutex mtx;
  vector<NotifySink*> sinks_;
};


///////////////////////////////////////////////////////////////////

NotifyService::NotifyService()
{
  AddMethod(&NotifySink::OnCqCallback, &StreamTester::AsyncService::RequestListenNotifications);
}


NotifyService::NotifySink::NotifySink(AsyncCallHandler<StreamTester::AsyncService> * _service)
    : state(initial_state), service(static_cast<NotifyService*>(_service))
{
  service->mtx.lock();
  service->sinks_.emplace_back(this);
  service->mtx.unlock();
}

NotifyService::NotifySink::~NotifySink()
{
  service->mtx.lock();
  service->sinks_.erase(find(service->sinks_.begin(), service->sinks_.end(), this));
  service->mtx.unlock();
}


void NotifyService::broadcast(const ServerNotification & notify)
{
  mtx.lock();
  for ( NotifySink * cc : sinks_ ) {
    cc->sendNotify(notify);
  }
  mtx.unlock();
}

void NotifyService::NotifySink::sendNotify(const ServerNotification & notify)
{
  switch (state) {
   case wait_event:
     state = send_notify;
     writer->Write(notify, tag);
     break;

   case send_notify:
     PDBG("Too slow sink: closing stream");
     state = finishing;
     break;

   default:
     break;
  }
}


// called from NotifyService::run() on Completion Queue wakeup
bool NotifyService::NotifySink::OnCqCallback(bool fok, ServerContext* context, ServerCompletionQueue * cq,
    const ListenNotificationsRequest * request, ServerAsyncWriter<ServerNotification>* writer, void * tag)
{
  (void)(context);
  (void) (cq);
  (void)(request);

  bool finish = !fok;

  if ( !finish ) {

    service->mtx.lock();

    switch ( state )
    {
    case initial_state:
      this->writer = writer;
      this->tag = tag;
      state = wait_event;
      break;

    case send_notify:
      state = wait_event;
      break;

    case wait_event:
      PDBG("BUG: Unexpected state waiting_event");
      finish = true;
      break;

    case finishing:
      writer->Finish(Status(StatusCode::DATA_LOSS, "Too slow client"), tag);
      state = finished;
      break;

    case finished:
      finish = true;
      break;
    }

    service->mtx.unlock();
  }

  return finish;
}





///////////////////////////////////////////////////////////////////


static void event_producer_thread(NotifyService * notifyService)
{
  ServerNotification notify;
  PDBG("STARTED");

  for ( int i = 0; ; ++i ) {

    this_thread::sleep_for(chrono::seconds(rand() % 3 + 1));

    notify.set_event(ssprintf("event-%d", i));
    notify.set_details(ssprintf("details-%d", i));

    notifyService->broadcast(notify);
  }

  PDBG("SHOULD BE NOT FINISHED");
}


static void run_test()
{
  ServerBuilder builder;
  NotifyService notifyService;


  builder.AddListeningPort("0.0.0.0:50051", InsecureServerCredentials());

  builder.RegisterService(&notifyService);
  notifyService.setCompletionQueue(builder.AddCompletionQueue());

  unique_ptr<Server> server = builder.BuildAndStart();

  new thread(event_producer_thread, &notifyService);

  notifyService.run();
}


int main()
{
  grpc_init();
    run_test();
  grpc_shutdown();

  return 0;
}

