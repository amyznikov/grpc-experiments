Experiments with AsyncGenericService and GenericStub
==


The GenericBridgeService class uses AsyncGenericService and GenericStub for 'bridging' client's calls to another grpc server:

	client -> smaster (bridge) -> hello-server 
	
To setup briding, create GenericBridgeService instance, add target channels, register service in ServerBuilder, and add Completion queue:

	GenericBridgeService bridge;
	ServerBuilder builder;
	
	builder.AddListeningPort(server_address,InsecureServerCredentials());
	
	builder.RegisterAsyncGenericService(bridge);  
  
	bridge.setCompletionQueue(builder.AddCompletionQueue());
	bridge.addChannel("/hello_service.Hello", 
		CreateChannel("localhost:50052",InsecureChannelCredentials()));

	builder.BuildAndStart();

	bridge.run();


Start smaster, start hello-server and finally start client - client will call smaster, smaster will bridge the call to hello server, read the responce and send it back to client.

