broadcat
========

**broadcat** is a simple tool for broadcasting lines of text to clients. It
binds to a TCP socket, accepts incoming connections, and listens on standard
input. When it receives a line on stdin, it sends it to all
connected clients. When a new client connects, **broadcat** sends it the most
recent line that it received from stdin.
