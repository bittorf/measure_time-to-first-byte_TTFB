
# Question:

when measuring the time for generating an answer of a remote appserver:  

_how much internal time does my application server need from  
 getting the request till outputting the first answer  
 byte, regardless of the network conditions we do not control?_

# Setup of appserver and box explained
```
   [app-server]
        |
    [internet]
        |
[artificial_latency]
        |
      [box]
        |
[artificial_latency]
        |
   [dns-server]
```

# TLDR! final conclusion

```
user@wwwserver:~$ FORMAT='%{time_total} - %{time_connect} - %{time_pretransfer} + %{time_namelookup} \n'
user@wwwserver:~$ curl --silent --write-out "$FORMAT" "http://127.0.0.1:6666" -o /dev/null | bc -l
   4.000378
```

proof:
```
user@wwwserver:~$ curl --silent --write-out '%{json}' "http://127.0.0.1:6666" -o /dev/null | jq . | grep total
   "time_total": 4.00063,
user@wwwserver:~$ time printf '%s\r\n%s\r\n\r\n' 'GET / HTTP/1.0' 'Host: localhost' | nc 127.0.0.1 6666
   real	0m4.003s
```

# Run the experiment

### start minimal http server

Simulate connect-latency (1 sec) and answer latency (3 sec), following RFC2616:
```
user@wwwserver:~$ touch server.pl && chmod +x server.pl && cat >server.pl <<EOF
#!/usr/bin/perl
use Socket;
my \$port = 6666;

socket( SOCK, PF_INET, SOCK_STREAM, "tcp" );
setsockopt( SOCK, SOL_SOCKET, SO_REUSEADDR, 1 );
bind( SOCK, sockaddr_in(\$port, INADDR_ANY) );
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
EOF

user@wwwserver:~$ ./server.pl
```

### prepare variables
```
user@box:~$ DNS_NAME='ip12.ip-178-33-65.eu'
user@box:~$ IP="$( dig "$DNS_NAME" A +short )"
user@box:~$
user@box:~$ # it *must* be external, not 127.0.0.1 and routed via same device like IP
user@box:~$ DNS_SERVER="$( grep nameserver /etc/resolv.conf | head -n1 | cut -d' ' -f2 )"
user@box:~$ DEV="$( ip -oneline route get "$DNS_SERVER" | cut -d' ' -f3 )"
user@box:~$
user@box:~$ NETWORK_LATENCY=900ms
user@box:~$ DNS_LATENCY=500ms
```

### test without artificial latency
```
user@box:~$ dig "@$DNS_SERVER" "$DNS_NAME" +noall +answer +stats | grep time:
   ;; Query time: 0 msec
user@box:~$ ping -c3 "$IP" | grep rtt
   rtt min/avg/max/mdev = 27.284/27.445/27.722/0.196 ms
```

### add dns- and network-latency using 'tc', 'HTB' and 'netem'
```
user@box:~$ sudo tc qdisc del dev $DEV root 2>/dev/null
user@box:~$ sudo tc qdisc add dev $DEV root handle 1: htb
user@box:~$
user@box:~$ sudo tc class  add dev $DEV parent 1: classid 1:1 htb rate 100mbit
user@box:~$ sudo tc filter add dev $DEV parent 1: protocol ip prio 1 u32 flowid 1:1 match ip dst "${DNS_SERVER}/32"
user@box:~$ sudo tc qdisc  add dev $DEV parent 1:1 handle 10: netem delay "${DNS_LATENCY}"
user@box:~$
user@box:~$ sudo tc class  add dev $DEV parent 1: classid 1:2 htb rate 100mbit
user@box:~$ sudo tc filter add dev $DEV parent 1: protocol ip prio 2 u32 flowid 1:2 match ip dst "${IP}/32"
user@box:~$ sudo tc qdisc  add dev $DEV parent 1:2 handle 20: netem delay "${NETWORK_LATENCY}"
```

### test our artificial latency
```
user@box:~$ dig "@$DNS_SERVER" "$DNS_NAME" +noall +answer +stats | grep time:
   ;; Query time: 500 msec
user@box:~$ ping -c3 "$IP" | grep rtt
   rtt min/avg/max/mdev = 927.496/927.617/927.788/0.124 ms
```

### final curl run
```
user@box:~$ URL="http://$DNS_NAME:6666"
user@box:~$ curl --silent --write-out '%{json}' "$URL" -o /dev/null | jq . | grep time
```

### curl timings with added artificial latency:
```
  "time_total": 6.358203,
  "time_namelookup": 0.501944,
  "time_connect": 1.430336,
  "time_appconnect": 0,
  "time_pretransfer": 1.430415,
  "time_starttransfer": 6.358112,
```

### curl without latency:
```
  "time_total": 4.057149,
  "time_namelookup": 0.001864,
  "time_connect": 0.029073,
  "time_appconnect": 0,
  "time_pretransfer": 0.029129,
  "time_starttransfer": 4.057048,
```

### remove latency / reset network
```
user@box:~$ sudo tc qdisc del dev "$DEV" root
```

### TODO
https://blog.cloudflare.com/a-question-of-timing/
