A C++ client-server application over TCP and UDP implementing its own protocol
for transmitting various data types like int, float, string etc.
A TCP client can send requests to the server to subscribe or unsubscribe to
a topic. The subscription persists between client sessions.
A UDP client can send requests to the server to publish data to a topic, it
is the server's responsibility to forward the data to all the subscribed
clients.
