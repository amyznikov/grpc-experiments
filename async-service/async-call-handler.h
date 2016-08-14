/*
 * async-call-handler.h
 *
 *  Created on: Aug 13, 2016
 *      Author: amyznikov
 */

#ifndef __async_call_handler_h__
#define __async_call_handler_h__

#include <grpc/grpc.h>
#include <grpc++/grpc++.h>


template<class Service>
class AsyncCallHandler : public Service {

protected:

  using ServerContext = grpc::ServerContext;
  using CompletionQueue = grpc::CompletionQueue;
  using ServerCompletionQueue = grpc::ServerCompletionQueue;
  template<class T> using ServerAsyncResponseWriter = grpc::ServerAsyncResponseWriter<T>;
  template<class T> using ServerAsyncWriter = grpc::ServerAsyncWriter<T>;
  template<class T1, class T2> using ServerAsyncReader = grpc::ServerAsyncReader<T1, T2>;
  template<class T1, class T2> using ServerAsyncReaderWriter = grpc::ServerAsyncReaderWriter<T1,T2>;

  enum CallType {
    NORMAL_RPC,
    SERVER_STREAMING,
    CLIENT_STREAMING,
    BIDI_STREAMING
  };

  enum CallState {
    callstate_wait_call = 0,
    callstate_proceed = 1,
    callstate_finish = 2,
  };


  struct CallCtx {
    virtual ~CallCtx() {}
    virtual void proceed(bool fok) = 0;
  };


  struct MethodCtx {
    virtual void requestNewCall() const = 0;
  };


  std::vector<std::unique_ptr<MethodCtx> > methods_;
  std::unique_ptr<ServerCompletionQueue> cq_;




  ////////////////////////////////////////////////////////////////////////
  template<class H, class REQ, class RESP>
  class Method
      : public MethodCtx
  {
    typedef bool (H::*NORMAL_RPC_HANDLER)(bool,
        ServerContext*,
        ServerCompletionQueue*,
        const REQ*,
        ServerAsyncResponseWriter<RESP>*,
        void*);


    typedef void (Service::*REQUEST_NORMAL_RPC)(
        ServerContext*,
        REQ*,
        ServerAsyncResponseWriter<RESP>*,
        CompletionQueue*,
        ServerCompletionQueue*,
        void*);


    typedef bool (H::*SERVER_STREAMING_HANDLER)(bool,
        ServerContext*,
        ServerCompletionQueue*,
        const REQ*,
        ServerAsyncWriter<RESP>*,
        void *);

    typedef void (Service::*REQUEST_SERVER_STREAMING)(ServerContext*,
        REQ*,
        ServerAsyncWriter<RESP>*,
        CompletionQueue*,
        ServerCompletionQueue*,
        void*);


    typedef bool (H::*CLIENT_STREAMING_HANDLER)(bool,
        ServerContext*,
        ServerCompletionQueue*,
        ServerAsyncReader<RESP, REQ>*,
        void*);


    typedef void (Service::*REQUEST_CLIENT_STREAMING)(ServerContext*,
        ServerAsyncReader<RESP, REQ>*,
        CompletionQueue*,
        ServerCompletionQueue*,
        void*);


    typedef bool (H::*BIDI_STREAMING_HANDLER)(bool,
        ServerContext*,
        ServerCompletionQueue*,
        ServerAsyncReaderWriter<RESP, REQ>*,
        void*);

    typedef void (Service::*REQUEST_BIDI_STREAMING)(ServerContext*,
        ServerAsyncReaderWriter<RESP,REQ>*,
        CompletionQueue*,
        ServerCompletionQueue*,
        void*);

    AsyncCallHandler * service_;
    const CallType callType_;

    union {
      const NORMAL_RPC_HANDLER call_normal_rpc_;
      const SERVER_STREAMING_HANDLER call_server_streaming_;
      const CLIENT_STREAMING_HANDLER call_client_streaming_;
      const BIDI_STREAMING_HANDLER call_bidi_streaming_;
    };

    union {
      const REQUEST_NORMAL_RPC request_normal_rpc_;
      const REQUEST_SERVER_STREAMING request_server_streaming_;
      const REQUEST_CLIENT_STREAMING request_client_streaming_;
      const REQUEST_BIDI_STREAMING request_bidi_streaming_;
    };


    struct CallHandler
        : public CallCtx
    {
      template<class T>
      static constexpr T smax(T x, T y) {
        return x>y ? x: y;
      }

      static const size_t storage_size = smax(smax(smax(
          sizeof(ServerAsyncResponseWriter<RESP>),
          sizeof(ServerAsyncWriter<RESP>)),
          sizeof(ServerAsyncReader<RESP, REQ>)),
          sizeof(ServerAsyncReaderWriter<RESP,REQ>));

      union {
        int64_t align;
        uint8_t storage[storage_size];
      };

      union {
        ServerAsyncResponseWriter<RESP> * rw_;
        ServerAsyncWriter<RESP> * w_;
        ServerAsyncReader<RESP, REQ> * r_;
        ServerAsyncReaderWriter<RESP,REQ> * stream_;
      };

      const Method * m_;
      CallState callstate_;

      ServerContext ctx_;
      REQ req_;

      H obj_;

      ~CallHandler()
      {
        switch ( m_->callType_ )
        {
        case NORMAL_RPC:
          rw_->~ServerAsyncResponseWriter<RESP>();
          break;
        case SERVER_STREAMING:
          w_->~ServerAsyncWriter<RESP>();
          break;
        case CLIENT_STREAMING:
          r_->~ServerAsyncReader<RESP, REQ>();
          break;
        case BIDI_STREAMING:
          stream_->~ServerAsyncReaderWriter<RESP,REQ>();
          break;
        }

      }

      CallHandler(const Method * m)
          : m_(m), callstate_(callstate_wait_call)
      {
        switch ( m->callType_ )
        {
        case NORMAL_RPC:
          rw_ = new (storage)ServerAsyncResponseWriter<RESP>(&ctx_);
          (m->service_->*(m->request_normal_rpc_))(&ctx_, &req_, rw_, m->service_->cq_.get(), m->service_->cq_.get(),
              this);
          break;
        case SERVER_STREAMING:
          w_ = new (storage)ServerAsyncWriter<RESP>(&ctx_);
          (m->service_->*(m->request_server_streaming_))(&ctx_, &req_, w_, m->service_->cq_.get(),
              m->service_->cq_.get(), this);
          break;
        case CLIENT_STREAMING:
          r_ = new (storage)ServerAsyncReader<RESP, REQ>(&ctx_);
          (m->service_->*(m->request_client_streaming_))(&ctx_, r_, m->service_->cq_.get(), m->service_->cq_.get(),
              this);
          break;
        case BIDI_STREAMING:
          stream_ = new (storage)ServerAsyncReaderWriter<RESP,REQ>(&ctx_);
          (m->service_->*(m->request_bidi_streaming_))(&ctx_, stream_, m->service_->cq_.get(), m->service_->cq_.get(),
              this);
          break;
        }
      }

      void proceed(bool fok)
      {
        switch ( callstate_ )
        {
        case callstate_wait_call:
          m_->requestNewCall();
          if ( !fok ) {
            delete this;
            break;
          }
          callstate_ = callstate_proceed;

        case callstate_proceed:
          switch ( m_->callType_ )
          {
          case NORMAL_RPC:
            fok = (obj_.*(m_->call_normal_rpc_))(fok, &ctx_, m_->service_->cq_.get(), &req_, rw_, this);
            break;
          case SERVER_STREAMING:
            fok = (obj_.*(m_->call_server_streaming_))(fok, &ctx_, m_->service_->cq_.get(), &req_, w_, this);
            break;
          case CLIENT_STREAMING:
            fok = (obj_.*(m_->call_client_streaming_))(fok, &ctx_, m_->service_->cq_.get(), r_, this);
            break;
          case BIDI_STREAMING:
            fok = (obj_.*(m_->call_bidi_streaming_))(fok, &ctx_, m_->service_->cq_.get(), stream_, this);
            break;
          }

          if ( fok ) {
            callstate_ = callstate_finish;
          }
          break;

        case callstate_finish:
          delete this;
          break;
        }
      }
    };

  public:
    Method(AsyncCallHandler * service, NORMAL_RPC_HANDLER fn, REQUEST_NORMAL_RPC ffn)
        : service_(service), callType_(NORMAL_RPC), call_normal_rpc_(fn), request_normal_rpc_(ffn)
      {}

    Method(AsyncCallHandler * service, SERVER_STREAMING_HANDLER fn, REQUEST_SERVER_STREAMING ffn)
        : service_(service), callType_(SERVER_STREAMING), call_server_streaming_(fn), request_server_streaming_(ffn)
      {}

    Method(AsyncCallHandler * service, CLIENT_STREAMING_HANDLER fn, REQUEST_CLIENT_STREAMING ffn)
        : service_(service), callType_(CLIENT_STREAMING), call_client_streaming_(fn), request_client_streaming_(ffn)
      {}

    Method(AsyncCallHandler * service, BIDI_STREAMING_HANDLER fn, REQUEST_BIDI_STREAMING ffn)
        : service_(service), callType_(BIDI_STREAMING), call_bidi_streaming_(fn), request_bidi_streaming_(ffn)
      {}

    void requestNewCall() const {
      new CallHandler(this);
    }

  };


  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


  template<class H, class S, class REQ, class RESP>
  void AddMethod(bool (H::*fn)(bool, ServerContext*, ServerCompletionQueue*, const REQ*, ServerAsyncResponseWriter<RESP>*, void *),
      void (S::*ffn)(ServerContext*, REQ*, ServerAsyncResponseWriter<RESP>*, CompletionQueue*, ServerCompletionQueue*,
          void *))
  {
    methods_.emplace_back(new Method<H, REQ, RESP>(this, fn, ffn));
  }


  template<class H, class S, class REQ, class RESP>
  void AddMethod(bool (H::*fn)(bool, ServerContext*, ServerCompletionQueue*, const REQ*, ServerAsyncWriter<RESP>*, void *),
      void (S::*ffn)(ServerContext*, REQ*, ServerAsyncWriter<RESP>*, CompletionQueue*, ServerCompletionQueue*, void *))
  {
    methods_.emplace_back(new Method<H, REQ, RESP>(this, fn, ffn));
  }


  template<class H, class S, class REQ, class RESP>
  void AddMethod(bool (H::*fn)(bool, ServerContext*, ServerCompletionQueue*, ServerAsyncReader<RESP, REQ>*, void * ),
      void (S::*ffn)(ServerContext*, ServerAsyncReader<RESP, REQ>*, CompletionQueue*, ServerCompletionQueue*, void*))
  {
    methods_.emplace_back(new Method<H, REQ, RESP>(this, fn, ffn));
  }


  template<class H, class S, class REQ, class RESP>
  void AddMethod(bool (H::*fn)(bool, ServerContext*, ServerCompletionQueue*, ServerAsyncReaderWriter<RESP, REQ>*, void*),
      void (S::*ffn)(ServerContext*, ServerAsyncReaderWriter<RESP, REQ>*, CompletionQueue*, ServerCompletionQueue*,
          void*))
  {
    methods_.emplace_back(new Method<H, REQ, RESP>(this, fn, ffn));
  }

public:
  AsyncCallHandler()
  {
  }

  void setCompletionQueue(std::unique_ptr<ServerCompletionQueue> cq)
  {
    cq_.reset(cq.release());
  }

  void run()
  {
    void * tag;
    bool fok;

    for ( auto m = methods_.begin(); m != methods_.end(); ++m ) {
      (*m)->requestNewCall();
    }

    while ( cq_->Next(&tag, &fok) ) {
      (static_cast<CallCtx *>(tag))->proceed(fok);
    }
  }
};



#endif /* __async_call_handler_h__ */
