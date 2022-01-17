### setup
```
appserver --- internet --- box --- dns-server
```

### run

# TODO: https://blog.cloudflare.com/a-question-of-timing/

# minimal http server:
user@srv:~$ CMD="printf '%s\n%s\n%s\n%s\n\n' 'HTTP/1.0 200 OK' 'Connection: close' 'Content-Type: text/plain' 'Content-Length: 2'; sleep 2; printf '%s' 'OK'"
user@srv:~$ eval $CMD
user@srv:~$ while true; do nc -l -p 6666 -c "$CMD"; date; done

# add dns- and network-latency using 'tc'
user@box:~$ DNS_NAME='ip12.ip-178-33-65.eu'
user@box:~$ IP="$( dig "$DNS_NAME" A +short )"
user@box:~$ DNS_SERVER="$( grep nameserver /etc/resolv.conf | head -n1 | cut -d' ' -f2 )"
user@box:~$ DEV="$( ip -oneline route get "$DNS_SERVER" | cut -d' ' -f3 )"
user@box:~$
user@box:~$ NETWORK_LATENCY=900ms
user@box:~$ DNS_LATENCY=500ms
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

# test latency:
user@box:~$ dig "@$DNS_SERVER" "$DNS_NAME" +noall +answer +stats | grep time:
;; Query time: 1023 msec
user@box:~$ ping -c3 "$IP" | grep rtt

# prepare curl:
user@box:~$ URL="http://$DNS_NAME:6666"
user@box:~$ curl --silent --write-out '%{json}' "$URL" -o /dev/null | jq .

# remove latency / reset network:
user@box:~$ sudo tc qdisc del dev "$DEV" root
