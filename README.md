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

# Run the experiment

## Question:

_how much time does my appserver need
 from request till outputting the first answerbyte,
 regardless of network conditions?_

### start minimal http server

Simulate connect-latency and answer latency:
```
user@wwwserver:~$ CMD1="printf '%s\n' 'HTTP/1.0 200 OK'"
user@wwwserver:~$ CMD2="printf '%s\n%s\n%s\n\n' 'Connection: close' 'Content-Type: text/plain' 'Content-Length: 6'"
user@wwwserver:~$ CMD3="printf '%s' 'answer'"
user@wwwserver:~$
user@wwwserver:~$ CMD="sleep 1; $CMD1; sleep 3; $CMD2; $CMD3"
user@wwwserver:~$ eval $CMD
user@wwwserver:~$
user@wwwserver:~$ while true; do nc -l -p 6666 -c "$CMD"; date; sleep 1; done
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
  "time_total": 6.360595,
  "time_namelookup": 0.501959,
  "time_connect": 1.429906,
  "time_appconnect": 0,
  "time_pretransfer": 1.429982,
  "time_starttransfer": 3.359345,
  "time_redirect": 0,
```

### curl without latency:
```
  "time_total": 4.059765,
  "time_namelookup": 0.001757,
  "time_connect": 0.029141,
  "time_appconnect": 0,
  "time_pretransfer": 0.029204,
  "time_starttransfer": 1.059095,
  "time_redirect": 0,
```

### remove latency / reset network
```
user@box:~$ sudo tc qdisc del dev "$DEV" root
```

### TODO
https://blog.cloudflare.com/a-question-of-timing/
