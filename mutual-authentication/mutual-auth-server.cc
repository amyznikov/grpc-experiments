/*
 * mutual-auth-server.cc
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

class SayHelloService GRPC_FINAL
  : public Hello::Service {

  Status sayHello(ServerContext* context, const SayHelloRequest* request, SayHelloReply* response)
  {
    PDBG("\n\n----------------------\nENTER");

    shared_ptr<const AuthContext> auth = context->auth_context();

    PDBG("PeerIdentityPropertyName = '%s'", auth->GetPeerIdentityPropertyName().c_str());

    PDBG("GetPeerIdentity:");
    vector<grpc::string_ref> pe = auth->GetPeerIdentity();
    for (vector<grpc::string_ref>::const_iterator ii = pe.begin(); ii != pe.end(); ++ii ) {
      grpc::string p(ii->begin(), ii->end());
      PDBG("PeerIdentity = '%s'", p.c_str());
    }

    PDBG("AuthPropertyIterator:");
    for ( AuthPropertyIterator ii = auth->begin(); ii!= auth->end(); ++ii ) {

      const grpc::string prop((*ii).first.begin(), (*ii).first.end());
      const grpc::string val((*ii).second.begin(), (*ii).second.end());

      PDBG("prop  = '%s'", prop.c_str());
      PDBG("value = '%s'", val.c_str());
    }

    response->set_message(ssprintf("Hello from thread %d", gettid()));

    PDBG("LEAVE\n---------------------\n\n\n\n");
    return Status::OK;
  }

};


class MyAuthMetadataProcessor
    : public AuthMetadataProcessor {
public:
  Status Process(const InputMetadata& auth_metadata, AuthContext* context, OutputMetadata* consumed_auth_metadata,
      OutputMetadata* response_metadata)
  {
    PDBG("ENTER");

    multimap<string_ref, string_ref>::const_iterator ii = auth_metadata.begin();
    for ( ; ii != auth_metadata.end(); ++ii ) {
      std::string key(ii->first.begin(), ii->first.end());
      std::string val(ii->second.begin(), ii->second.end());
      PDBG("'%s'='%s'", key.c_str(), val.c_str());
    }

    PDBG("LEAVE");
    return Status::OK;
  }
};

static void run_test(const std::string & skey, const std::string & scert, const std::string & cacert)
{
  std::string server_address("0.0.0.0:50051");
  SayHelloService service;
  ServerBuilder builder;


  SslServerCredentialsOptions ssl_opts(GRPC_SSL_REQUEST_AND_REQUIRE_CLIENT_CERTIFICATE_AND_VERIFY);

  ssl_opts.pem_root_certs = cacert;

  ssl_opts.pem_key_cert_pairs.push_back((SslServerCredentialsOptions::PemKeyCertPair) {
        .private_key = skey,
        .cert_chain = scert
      });


  shared_ptr<ServerCredentials> creds = SslServerCredentials(ssl_opts);
  shared_ptr<AuthMetadataProcessor> authproc(new MyAuthMetadataProcessor());
  creds->SetAuthMetadataProcessor(authproc);

  builder.AddListeningPort(server_address, creds);
  builder.RegisterService(&service);

  unique_ptr<Server> server = builder.BuildAndStart();
  if ( !server ) {
    PDBG("builder.BuildAndStart() fais");
  }
  else {
    server->Wait();
  }
}



//////////////////////////////////////////////////////////////

int main(int argc, char * argv[])
{
  // server private key
  const char * skeyfilename = NULL;
  std::string skey;

  // server certifiate
  const char * scertfilename = NULL;
  std::string scert;

  // CA root certificate
  const char * cacertfilename = NULL;
  std::string cacert;

  for ( int i = 1; i < argc; ++i ) {
    if ( strncmp(argv[i], "skey=", 5) == 0 ) {
      skeyfilename = argv[i] + 5;
    }
    else if ( strncmp(argv[i], "scert=", 6) == 0 ) {
      scertfilename = argv[i] + 6;
    }
    else if ( strncmp(argv[i], "cacert=", 7) == 0 ) {
      cacertfilename = argv[i] + 7;
    }
    else {
      fprintf(stderr, "Invalid arg %s\n", argv[i]);
      fprintf(stderr, "Usage:\n");
      fprintf(stderr, "  server "
          "[skey=<server-private-key>] "
          "[scert=<server-certificate>]"
          "[cacert=<ca-root-certificate>]"
          "\n");
      return 1;
    }
  }


  if ( skeyfilename && (skey = readfile(skeyfilename)).empty() ) {
    fprintf(stderr, "readfile(%s) fails: %s\n", skeyfilename, strerror(errno));
    return 1;
  }

  if ( scertfilename && (scert = readfile(scertfilename)).empty() ) {
    fprintf(stderr, "readfile(%s) fails: %s\n", scertfilename, strerror(errno));
    return 1;
  }

  if ( cacertfilename && (cacert = readfile(cacertfilename)).empty() ) {
    fprintf(stderr, "readfile(%s) fails: %s\n", cacertfilename, strerror(errno));
    return 1;
  }



  grpc_init();

  run_test(skey, scert, cacert);

  grpc_shutdown();
  return 0;
}

