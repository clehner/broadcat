broadcat
========

Turn any command line program into a simple broadcast server.

## What it does

**broadcat** is a simple tool for broadcasting lines of text to clients. It
binds to a TCP socket, accepts incoming connections, and listens on standard
input, or to standard output of a subprocess. When it receives a line, it sends
it to all connected clients. When a new client connects, broadcat sends it
the most recent line that it received. When no clients are connected, broadcat
sends the subprocess a STOP signal so it doesn't do needless computation, and
then when a client connects, it sends a CONT signal so the subprocess can
resume.

## Compiling

    make

## Usage

    ./broadcat [port] [command...]

### Example

A clock server:

    ./broadcat 9999 bash -c 'while date; do sleep 1; done'

To connect to the server, use netcat or telnet:

    $ nc localhost 9999
	Thu Jan 23 00:30:09 EST 2014
	Thu Jan 23 00:30:10 EST 2014
	Thu Jan 23 00:30:11 EST 2014
	Thu Jan 23 00:30:12 EST 2014
	^C

## Contributing

If you think of something cool to do with **broadcat**, or find a way to improve
it, or if you use it in a project that you want to be mentioned here, feel free
to make a pull request with the change.

## License

[MIT License](http://cel.mit-license.org/)
