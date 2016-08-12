/*
 * bridge-service.h
 *
 *  Created on: Aug 12, 2016
 *      Author: amyznikov
 */


#ifndef __grpc_generic_bridge_service_h__
#define __grpc_generic_bridge_service_h__

#include <grpc/grpc.h>
#include <grpc++/grpc++.h>
#include <grpc++/generic/async_generic_service.h>
#include <grpc++/generic/generic_stub.h>



class GenericBridgeService {

public:
  GenericBridgeService();
  void addChannel(const std::string & service_name, std::shared_ptr<grpc::Channel> channel);

  void setCompletionQueue(std::unique_ptr<grpc::ServerCompletionQueue> cq);
  grpc::AsyncGenericService * getAsyncGenericService();
  operator grpc::AsyncGenericService * ();
  void run();

private:

  enum CallState {
    wait_for_call = 0,
    wait_read_from_client = 1,
    wait_call_to_stub = 2,
    wait_send_buffer_to_stub = 3,
    wait_recv_buffer_from_stub = 4,
    wait_status_from_stub = 5,
    wait_send_buffer_to_client = 6,
    wait_for_finish = 10,
  };


  struct CallCtx {
    std::unique_ptr<grpc::GenericStub> stub_;

    grpc::GenericServerContext client_context;
    grpc::GenericServerAsyncReaderWriter client_stream;

    grpc::ClientContext stub_context;
    std::unique_ptr<grpc::GenericClientAsyncReaderWriter> stub_stream;

    grpc::ByteBuffer msgbuf;
    grpc::Status stub_status;

    enum CallState callstate;

    CallCtx()
        : client_stream(&client_context), callstate(wait_for_call)
    {
    }
  };


  grpc::AsyncGenericService generic_service_;
  std::unique_ptr<grpc::ServerCompletionQueue> cq_;
  std::map<std::string,std::shared_ptr<grpc::Channel> > channels_;

private:
  CallCtx * requestNewCall();
  void processCall(CallCtx * ctx, bool fok);
  std::shared_ptr<grpc::Channel> findChannel(const std::string & method_name);
};


#endif /* __grpc_generic_bridge_service_h__ */
