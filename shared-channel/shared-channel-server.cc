/*
 * grpc-shared-channel-server.cc
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
#include "shared-channel.grpc.pb.h"

using namespace grpc;
using namespace std;
using namespace test;


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
///////////////////////////////////////////////////////////////////


class HelloService GRPC_FINAL
  : public Hello::Service
{

  Status sayHello(ServerContext* context, const SayHelloRequest* request, SayHelloReply* response) override
  {
    response->set_hello(sprintf("hello from thread %d", gettid()));
    return Status::OK;
  }

};


class ContactListSerice final : public ContactListService::Service {

  class Contact: public test::Contact {
  public:
    Contact(const string & name, const string & phone) {
      set_name(name);
      set_phone(phone);
    }
  };

  Status getContactList(ServerContext* context, const ContactListRequest* request, ServerWriter< test::Contact>* writer) override
  {
    for ( int i = 0; i < 3; ++i ) {
      writer->Write(Contact(sprintf("name%d",i), sprintf("phone%d",i)));
    }
    return Status::OK;
  }

};



int main()
{
  HelloService helloService;
  ContactListSerice contactsService;

  ServerBuilder builder;

  builder.AddListeningPort("0.0.0.0:50051", InsecureServerCredentials());

  builder.RegisterService(&helloService);
  builder.RegisterService(&contactsService);

  unique_ptr<Server> server = builder.BuildAndStart();
  server->Wait();

  return 0;
}
