Simple test for mutual TLS authentification.
========


``generate-certs.sh`` will create server private key and certificate 'server.key', 'server.crt', as well as client key and certificate 'client1.key', 'client1.crt', signed by 'server.crt', used as CA root here.
 
Start server

	$ ./server skey=server.key scert=server.crt

	
Start client 

	$ ./client ckey=client1.key ccert=client1.crt scert=server.crt
	
In the call handler, server will print client's auth info including PeerIdentity = 'client1' and other client certificate info.

 