/*
 * bridge-service.cc
 *
 *  Created on: Aug 12, 2016
 *      Author: amyznikov
 */

#include "bridge-service.h"
#include <unistd.h>
#include <syscall.h>

using namespace std;
using namespace grpc;

////////////////////////////////////////////////////////////////////////////

static inline pid_t gettid(void)
{
  return (pid_t) syscall (SYS_gettid);
}

#define PDBG(...) \
    fprintf(stderr, "[%d] %s(): %6d ", gettid(), __func__, __LINE__), \
    fprintf(stderr, __VA_ARGS__), \
    fputc('\n', stderr), \
    fflush(stderr)

////////////////////////////////////////////////////////////////////////////


static inline string extract_service_name(const string & method_name)
{
  return method_name.substr(0, method_name.find_last_of('/'));
}


GenericBridgeService::GenericBridgeService()
{
}

GenericBridgeService::operator AsyncGenericService * ()
{
  return &generic_service_;
}

AsyncGenericService * GenericBridgeService::getAsyncGenericService()
{
  return &generic_service_;
}


void GenericBridgeService::setCompletionQueue(std::unique_ptr<grpc::ServerCompletionQueue> cq)
{
  cq_.reset(cq.release());
}

void GenericBridgeService::addChannel(const std::string & service_name, std::shared_ptr<grpc::Channel> channel)
{
  channels_[service_name] = channel;
}



GenericBridgeService::CallCtx * GenericBridgeService::requestNewCall()
{
  CallCtx * ctx = new CallCtx();
  generic_service_.RequestCall(&ctx->client_context, &ctx->client_stream, cq_.get(), cq_.get(), (void*) 1);
  return ctx;
}

void GenericBridgeService::run()
{
  CallCtx * cctx;
  void * tag;
  bool fok;

  cctx = requestNewCall();

  while ( cq_->Next(&tag, &fok) ) {

    if ( tag == (void*) 1 ) { // if new call
      GPR_ASSERT(fok);
      tag = cctx, cctx = requestNewCall();
    }

    processCall(static_cast<CallCtx *>(tag), fok);
  }

}


inline shared_ptr<Channel> GenericBridgeService::findChannel(const string & method_name)
{
  std::map<std::string, std::shared_ptr<grpc::Channel> >::const_iterator ii = channels_.find(
      extract_service_name(method_name));
  return ii == channels_.end() ? 0 : ii->second;
}


void GenericBridgeService::processCall(CallCtx * ctx, bool fok)
{
  shared_ptr<Channel> channel;

  PDBG("ctx=%p callstate=%d peer=%s host=%s method=%s", ctx, ctx->callstate, ctx->client_context.peer().c_str(),
      ctx->client_context.host().c_str(), ctx->client_context.method().c_str());


  if ( !fok ) {
    switch ( ctx->callstate ) {
    case wait_call_to_stub:
      case wait_send_buffer_to_stub:
      case wait_recv_buffer_from_stub:
      case wait_status_from_stub:
      ctx->client_stream.Finish(Status(StatusCode::UNAVAILABLE, "Error on Stub Call"), ctx);
      ctx->callstate = wait_for_finish;
      break;

    case wait_for_call:
      case wait_read_from_client:
      case wait_send_buffer_to_client:
      case wait_for_finish:
      delete ctx;
      break;
    }

    return;
  }


  switch ( ctx->callstate )
  {
  case wait_for_call:

    if ( !(channel = findChannel(ctx->client_context.method())) ) {
      ctx->callstate = wait_for_finish;
      ctx->client_stream.Finish(Status(StatusCode::UNAVAILABLE, "Requested service not registered"), ctx);
    }
    else {
      ctx->stub_.reset(new GenericStub(channel));
      ctx->callstate = wait_read_from_client;
      ctx->client_stream.Read(&ctx->msgbuf, ctx);
    }

    break;

  case wait_read_from_client:
    ctx->callstate = wait_call_to_stub;
    ctx->stub_stream = ctx->stub_->Call(&ctx->stub_context, ctx->client_context.method(), cq_.get(), ctx);
    break;

  case wait_call_to_stub:
    ctx->callstate = wait_send_buffer_to_stub;
    ctx->stub_stream->Write(ctx->msgbuf, ctx);
    break;

  case wait_send_buffer_to_stub:
    ctx->callstate = wait_recv_buffer_from_stub;
    ctx->stub_stream->Read(&ctx->msgbuf, ctx);
    break;

  case wait_recv_buffer_from_stub:
    ctx->callstate = wait_status_from_stub;
    ctx->stub_stream->Finish(&ctx->stub_status, ctx);
    break;

  case wait_status_from_stub:
    ctx->callstate = wait_send_buffer_to_client;
    ctx->client_stream.Write(ctx->msgbuf, ctx);
    break;

  case wait_send_buffer_to_client:
    ctx->callstate = wait_for_finish;
    ctx->client_stream.Finish(ctx->stub_status, ctx);
    break;

  case wait_for_finish:
    delete ctx;
    break;
  }
}
