# Proxy Testing

At the time of writing the slave only supports socks proxies.
To set up a proxy put it in the socks and/or ftp category of the proxy KCM.

Dante is a fairly nice socks proxy server https://www.inet.no/dante

## Dante config

To get a working dante config you'll at least need to configure internal and external addresses/interfaces:

```
internal: br0 port = 1080
external: 192.168.100.106
```

For easy authentication against the unix user database the additonal is necessary:

```
socksmethod: username
user.privileged: root
user.unprivileged: nobody
client pass {
    from: 0.0.0.0/0 to: 0.0.0.0/0
    log: connect error
}
socks pass {
    from: 0.0.0.0/0 to: 0.0.0.0/0
    protocol: tcp udp
    command: bind connect udpassociate
    log: error connect
    socksmethod: username
}
```

This should give you a working socks proxy with authentication requirement.

## Debugging

Something like this should do for debugging:

```
logoutput: stderr syslog /var/log/sockd.log
debug: 1
```
