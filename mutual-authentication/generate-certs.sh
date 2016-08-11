#! /bin/bash

# See:
# http://stackoverflow.com/questions/10175812/how-to-create-a-self-signed-certificate-with-openssl
# https://www.madboa.com/geek/openssl/#how-do-i-generate-a-self-signed-certificate
# https://habrahabr.ru/post/192446/ 

domain=localhost


# Create ROOT CA
openssl req \
	-newkey rsa:2048 -nodes -keyout server.key \
	-x509 -days 365 -out server.crt \
	-subj "/C=UA/ST=HOME/L=ROOM/O=MYORG/OU=MYORGUNIT/CN=${domain}"



# Create test clients
for client in "client1" "client2" ; do
	
	openssl req \
		-newkey rsa:2048 -nodes -keyout $client.key \
		-out $client.csr -subj "/C=UA/ST=HOME/L=ROOM/O=MYORG/OU=MYORGUNIT/CN=$client"
		
	openssl x509 -req -in $client.csr -CA server.crt -CAkey server.key \
		-CAcreateserial -out $client.crt -days 365
		
done	


