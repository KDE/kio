/*
   A script to test Microsoft's IPv6 extension to the PAC specification. If
   you want to test the original PAC specification, use 'kpactest.pac'.

   To use this script, select "Use automatic proxy configuration URL:" in the
   KDE proxy configuration dialog and run:

   qdbus org.kde.kded5 /modules/proxyscout proxyForUrl http://blah (URL doesn't matter)

   If everything succeeds, the output you get will be http://<your IP>/. If not,
   you would get http://<your IP>/<test-result> where <test-result> contains the
   tests that "failed". You can lookup the failed test name in the comments below
   and file a report in the KDE bug tracking system if you feel the problem is in
   the implementation.

   Please note that since myIpAddressEx returns a semi-colon delimited list of all the
   valid ip address for your machine matches, including the IPv6 representations for
   the same network interface, you should always see at least two http://<your IP>/
   in the result.
*/

function FindProxyForURLEx( url, host )
{
    var result = "";

    // isResolvableEx1
    // on failure make sure you can resolve www.kde.org correctly :-)
    if ( !isResolvableEx( "www.kde.org" ) )
        result += "/isResolvableEx1=failed";
    // isResolvableEx2
    // on failure make sure dummy.invalid doesn't resolve :-)
    if ( isResolvableEx( "dummy.invalid" ) )
        result += "/isResolvableEx2=failed";

    // isInNetEx1
    // on failure check if localhost resolves to 127.0.0.1 as it should
    if ( isInNetEx( "localhost", "1.2.3.4/8" ) )
        result += "/isInNetEx1=failed";
    // isInNetEx2
    if ( isInNetEx( "1.2.3.4", "1.2.3.5/32" ) )
        result += "/isInNetEx2=failed";
    // isInNetEx3
    if ( !isInNetEx( "1.2.3.4", "1.2.3.5/24" ) )
        result += "/isInNetEx3=failed";

    // dnsResolveEx1
    // on failure check if localhost resolves to 127.0.0.1 as it should
    if ( dnsResolveEx( "localhost" ).indexOf("127.0.0.1") == -1 )
        result += "/dnsResolveEx1=failed";

    // sortIpAddressList
    var sorted = sortIpAddressList("2001:4898:28:3:201:2ff:feea:fc14;157.59.139.22;fe80::5efe:157.59.139.2");
    if ( sorted != "fe80::5efe:157.59.139.2;2001:4898:28:3:201:2ff:feea:fc14;157.59.139.22" )
        result += "/sortIpAddressList=failed";

    var output = new Array();
    var items = myIpAddressEx().split(";");
    for (var i = 0; i < items.length; ++i) {
        var entry = "PROXY http://" + items[i] + result;
        output[i] = entry;
    }

    return output.join(';');
}
