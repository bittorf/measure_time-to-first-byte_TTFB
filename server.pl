#!/usr/bin/perl
use Socket;
my $port = 6666;

socket( SOCK, PF_INET, SOCK_STREAM, "tcp" );
setsockopt( SOCK, SOL_SOCKET, SO_REUSEADDR, 1 );
bind( SOCK, sockaddr_in($port, INADDR_ANY) );
listen( SOCK, SOMAXCONN );

while( accept(CLIENT, SOCK) ){
  sleep(1);
  print CLIENT "HTTP/1.0 200 OK\r\n";
  sleep(3);
  print CLIENT "Content-Type: text/plain\r\n" .
	       "Content-Length: 6\r\n" .
	       "Connection: close\r\n\r\n" .
               "answer";
  close CLIENT;
}
