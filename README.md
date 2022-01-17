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
```
user@wwwserver:~$ CMD="printf '%s\n%s\n%s\n%s\n\n' 'HTTP/1.0 200 OK' 'Connection: close' 'Content-Type: text/plain' 'Content-Length: 2'; sleep 2; printf '%s' 'OK'"
user@wwwserver:~$ eval $CMD
user@wwwserver:~$ while true; do nc -l -p 6666 -c "$CMD"; date; done
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
user@box:~$ curl --silent --write-out '%{json}' "$URL" -o /dev/null | jq .
```

### output:
```
{
  "url_effective": "http://ip12.ip-178-33-65.eu:6666/",
  "method": "GET",
  "http_code": 200,
  "response_code": 200,
  "num_headers": 3,
  "http_connect": 0,
  "time_total": 4.413503,
  "time_namelookup": 0.556528,
  "time_connect": 1.484244,
  "time_appconnect": 0,
  "time_pretransfer": 1.484312,
  "time_starttransfer": 2.412429,
  "size_header": 78,
  "size_request": 89,
  "size_download": 2,
  "size_upload": 0,
  "speed_download": 0,
  "speed_upload": 0,
  "content_type": "text/plain",
  "num_connects": 1,
  "time_redirect": 0,
  "num_redirects": 0,
  "ssl_verify_result": 0,
  "proxy_ssl_verify_result": 0,
  "filename_effective": "/dev/null",
  "remote_ip": "178.33.65.12",
  "remote_port": 6666,
  "local_ip": "10.63.22.100",
  "local_port": 33870,
  "http_version": "1",
  "scheme": "HTTP",
  "curl_version": "libcurl/7.74.0 OpenSSL/1.1.1k zlib/1.2.11 brotli/1.0.9 libidn2/2.3.0 libpsl/0.21.0 (+libidn2/2.3.0) libssh2/1.9.0 nghttp2/1.43.0 librtmp/2.3"
}
```

### remove latency / reset network
```
user@box:~$ sudo tc qdisc del dev "$DEV" root
```

### TODO
https://blog.cloudflare.com/a-question-of-timing/
