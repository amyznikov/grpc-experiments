/*
 * mutual-auth-client.cc
 *
 *  Created on: Aug 9, 2016
 *      Author: amyznikov
 */
#include <unistd.h>
#include <syscall.h>
#include <thread>
#include <chrono>
#include <fstream>
#include <grpc/grpc.h>
#include <grpc++/grpc++.h>
#include "mutual-auth.grpc.pb.h"

using namespace grpc;
using namespace std;
using namespace mutual_auth_test;


//////////////////////////////////////////////////////////////


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


static string readfile(const char * fname) {
  std::ifstream ifs(fname);
  return string((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
}


#define PDBG(...) \
    fprintf(stderr, "[%d] %s(): %6d ", gettid(), __func__, __LINE__), \
    fprintf(stderr, __VA_ARGS__), \
    fputc('\n', stderr), \
    fflush(stderr)



//////////////////////////////////////////////////////////////


class SayHelloClient {
  unique_ptr<Hello::Stub> stub_;

public:
  SayHelloClient(shared_ptr<Channel> channel)
      : stub_(Hello::NewStub(channel))
  {
  }

  void sayHello()
  {
    ClientContext ctx;
    SayHelloReply resp;
    Status status;

    PDBG("C sayHello()");
    status = stub_->sayHello(&ctx, SayHelloRequest(), &resp);
    PDBG("R sayHello(): status=%d %s resp=%s", status.error_code(), status.error_message().c_str(), resp.message().c_str());
  }

};



static void run_test(const std::string & ckey, const std::string & ccert, const std::string & scert)
{
  SslCredentialsOptions ssl_opts;

  ssl_opts.pem_private_key = ckey;
  ssl_opts.pem_cert_chain = ccert;
  ssl_opts.pem_root_certs = scert;

  shared_ptr<ChannelCredentials> creds = SslCredentials(ssl_opts);

  shared_ptr<Channel> channel = CreateChannel("localhost:50051", creds);

  SayHelloClient client(channel);
  client.sayHello();
}


int main( int argc, char * argv[])
{
  // client private key
  const char * ckeyfilename = NULL;
  string ckey;

  // client certificate
  const char * ccertfilename = NULL;
  string ccert;

  // server certificate
  const char * scertfilename = NULL;
  string scert;

  for (int i = 1; i < argc; ++i) {

    if ( strncmp(argv[i], "ckey=", 5) == 0 ) {
      ckeyfilename = argv[i] + 5;
    }
    else if ( strncmp(argv[i], "ccert=", 6) == 0 ) {
      ccertfilename = argv[i] + 6;
    }
    else if ( strncmp(argv[i], "scert=", 6) == 0 ) {
      scertfilename = argv[i] + 6;
    }
    else {
      fprintf(stderr, "Invalid argument %s\n", argv[i]);
      fprintf(stderr, "Usage:\n");
      fprintf(stderr, "  client "
          "[ckey=<client-private-key>] "
          "[ccert=<client-certificate>] "
          "[scert=<server-certificate>]"
          "\n");
      return 1;
    }
  }


  if ( ckeyfilename && (ckey = readfile(ckeyfilename)).empty() ) {
    fprintf(stderr, "readfile(%s) fails: %s\n", ckeyfilename, strerror(errno));
    return 1;
  }

  if ( ccertfilename && (ccert = readfile(ccertfilename)).empty() ) {
    fprintf(stderr, "readfile(%s) fails: %s\n", ccertfilename, strerror(errno));
    return 1;
  }

  if ( scertfilename && (scert = readfile(scertfilename)).empty() ) {
    fprintf(stderr, "readfile(%s) fails: %s\n", scertfilename, strerror(errno));
    return 1;
  }



  grpc_init();
  run_test(ckey, ccert, scert);
  grpc_shutdown();

  return 0;
}
