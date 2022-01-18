
# Question

How to measure the time my appserver needs to generate an answer:  

_how much internal processing time does my application server need from  
 receiving the http-request till outputting the first answer byte,  
 regardless of the network conditions we do not control?_

In other words:  
This is [TTFB](https://en.wikipedia.org/wiki/Time_to_first_byte) but [without network latency.](https://blog.cloudflare.com/ttfb-time-to-first-byte-considered-meaningles/)

# TLDR! final conclusion

**this is still wrong and needs more investigation!**  
Measuring timings with and without artifical latency gives the same time:

```
user@wwwserver:~$ URL=...
user@wwwserver:~$ CALC='define x(n) { if (n<0) n=0.001; return n }'
user@wwwserver:~$ FORMAT="$CALC;x( %{time_total} - %{time_connect} - %{time_pretransfer} + %{time_namelookup} )\n"
user@wwwserver:~$ curl --silent --range 0-9 --write-out "$FORMAT" "$URL" -o /dev/null | bc -l
   4.000378
```

proof: when sending the same query on localhost without the laggy network:
```
user@wwwserver:~$ curl --silent --range 0-9 --write-out '%{time_total}\n' "http://127.0.0.1:6666" -o /dev/null
   "time_total": 4.00063,
user@wwwserver:~$ time printf '%s\r\n%s\r\n\r\n' 'GET / HTTP/1.0' 'Host: localhost' | nc 127.0.0.1 6666
   real	0m4.003s
```

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

# Background

When you want to check the answer time of your application servers, you want to know:  
Is there anything you can fix to make them perform better?  
You dont want to get false alarms, if the internet is laggy.  
You only care about things you can change, not things you have no influence on.  

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

The used DNS-server is a locally connected caching dnsmasq.  
The remote IP is around 600 kilometers away, both values are plausible.
```
user@box:~$ dig "@$DNS_SERVER" "$DNS_NAME" +noall +answer +stats | grep time:
   ;; Query time: 0 msec

user@box:~$ ping -c3 "$IP" | grep rtt
   rtt min/avg/max/mdev = 27.284/27.445/27.722/0.196 ms
```

### add dns- and network-latency using 'tc', 'HTB' and 'netem'
```
user@box:~$ # needed vars: $DEV $DNS_SERVER $DNS_LATENCY $IP $NETWORK_LATENCY
user@box:~$
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
user@box:~$ curl --range 0-9 --silent --write-out '%{json}' "$URL" -o /dev/null | jq . | grep time
```

### curl timings with added artificial latency
```
  "time_total": 6.404746,
  "time_namelookup": 0.548208,
  "time_connect": 1.476145,
  "time_appconnect": 0,
  "time_pretransfer": 1.476214,
  "time_starttransfer": 6.404657,
```

### curl without added latency
```
  "time_total": 4.057149,
  "time_namelookup": 0.001864,
  "time_connect": 0.029073,
  "time_appconnect": 0,
  "time_pretransfer": 0.029129,
  "time_starttransfer": 4.057048,
```

### curl on webserver host, without laggy network at all
```
  "time_total": 4.000617,
  "time_namelookup": 4.2e-05,
  "time_connect": 0.000219,
  "time_appconnect": 0,
  "time_pretransfer": 0.000286,
  "time_starttransfer": 1.000386,
```

### list and remove latency / reset network
```
user@box:~$ sudo tc qdisc ls  dev "$DEV"
user@box:~$ sudo tc qdisc del dev "$DEV" root
```

### ToDo

* https://blog.cloudflare.com/a-question-of-timing/
  * when using this method, I get 4.428 seconds instead of 4.003
* check multiple redirects
* measure with HTTPS to fill appconnect?
* measure timestamps with iptables?
