/*
 * grpc-shared-channel-client.cc
 *
 *  Created on: Aug 7, 2016
 *      Author: amyznikov
 */

#include <thread>
#include <chrono>
#include <grpc/grpc.h>
#include <grpc++/grpc++.h>
#include "shared-channel.grpc.pb.h"


using namespace std;
using namespace grpc;
using namespace test;



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
  SayHelloReply resp;
  Status status;

  cout << "C sayHello()\n";
  status = stub_->sayHello(&ctx, SayHelloRequest(), &resp);
  cout << "R sayHello(): resp = " << resp.hello() << " status = " << status.error_code() << " " << status.error_message() << endl;
}

//////////////////////////////////////////////////////////////


class GetContactsClient {
  unique_ptr<ContactListService::Stub> stub_;
public:
  GetContactsClient(shared_ptr<Channel> channel);
  void getContacts();
};



GetContactsClient::GetContactsClient(shared_ptr<Channel> channel)
    : stub_(ContactListService::NewStub(channel))
{
}

void GetContactsClient::getContacts()
{
  ClientContext ctx;
  Contact contact;
  Status status;

  cout << "C getContactList()\n";
  unique_ptr<ClientReader<Contact>> rd = stub_->getContactList(&ctx, ContactListRequest());
  while ( rd->Read(&contact) ) {
    cout << "CONTACT: " << contact.name() << " " << contact.phone() << endl;
  }

  status = rd->Finish();
  cout << "R getContactList(): status=" << status.error_code() << " " << status.error_message() << endl;
}

//////////////////////////////////////////////////////////////


static void runTest()
{
  // share single channel for two clients
  shared_ptr<Channel> channel = CreateChannel("localhost:50051", InsecureChannelCredentials());

  HelloClient helloClient(channel);
  GetContactsClient contactsClient(channel);


  /// Call
  helloClient.sayHello();
  contactsClient.getContacts();

  /// Once again
  helloClient.sayHello();
  contactsClient.getContacts();
}


int main()
{
  grpc_init();

  runTest();

  grpc_shutdown();

  return 0;
}
