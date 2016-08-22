Simple test for mutual TLS authentification.
========


``generate-certs.sh`` will create root CA key/certificate ca.key, ca.crt, server key/certificate 'server.key', 'server.crt', and client key/certificate 'client1.key', 'client1.crt', signed by 'ca.crt', used as CA root.
 
Start server

	$ GRPC_SSL_CIPHER_SUITES=ALL  ../server skey=server.key scert=server.crt cacert=ca.crt

	
Start client 

	$ GRPC_SSL_CIPHER_SUITES=ALL ../client ckey=client1.key ccert=client1.crt cacert=ca.crt
	
In the call handler, server will print client's auth info including PeerIdentity = 'client1' and other client certificate info.
 