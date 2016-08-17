Broadcast server notifications to clients in single thread
===============


This test uses AsyncCallHandler to send server notifications to attached clients.

On start, each client calls `ListenNotifications()` service method, and then reads infinite stream of server messages.

On server, the `event_producer_thread()` generates server events, and each event notification is broadcasted by `NotifyService` to all attached clients.

All clients are notified from single thread using "asynchronous" grpc api.