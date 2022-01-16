### setup
```
appserver --- internet --- box --- dns-server
```

### run

# minimal http server:
user@srv:~$ CMD="printf '%s\n%s\n%s\n%s\n\n' 'HTTP/1.0 200 OK' 'Connection: close' 'Content-Type: text/plain' 'Content-Length: 2'; sleep 2; printf '%s' 'OK'"
user@srv:~$ eval $CMD
user@srv:~$ while true; do nc -l -p 6666 -c "$CMD"; date; done

# add dns-latency:
user@box:~$ DNS_NAME='ip12.ip-178-33-65.eu'
user@box:~$ DNS_PORT=1337
user@box:~$ slodns -d 1000 -j 0 -l 0 -p "$DNS_PORT"
user@box:~$ dig @127.0.0.1 -p "$DNS_PORT" "$DNS_NAME" +noall +answer +stats | grep time:
;; Query time: 1023 msec

# add network-latency:
user@box:~$ sudo tc qdisc add dev eth0 root netem delay 333ms
user@box:~$ sudo tc -p qdisc ls dev eth0
user@box:~$ ping -c3 "$DNS_NAME" | grep rtt

# prepare curl:
user@box:~$ curl -V
curl 7.74.0
user@box:~$ URL="http://$DNS_NAME"
user@box:~$ FORMAT="%{json}"
user@box:~$ FORMAT="dnslookup: %{time_namelookup} | connect: %{time_connect} | appconnect: %{time_appconnect} | pretransfer: %{time_pretransfer} | starttransfer: %{time_starttransfer} | total: %{time_total} | size: %{size_download}\n"

user@box:~$ curl --dns-servers 127.0.0.1:$DNS_PORT -so /dev/null --write-out "$FORMAT" "$URL:6666"

# remove network-latency:
user@box:~$ sudo tc qdisc del dev eth0 root
