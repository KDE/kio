/*
   A script to test the PAC specification.

   To use this script, select "Use automatic proxy configuration URL:" in the
   KDE proxy configuration dialog and run:

   qdbus org.kde.kded5 /modules/proxyscout proxyForUrl http://blah (URL doesn't matter)

   If everything succeeds, the output you get will be http://<your IP>/. If not,
   you would get http://<your IP>/<test-result> where <test-result> contains the
   tests that "failed". You can lookup the failed test name in the comments below
   and file a report in the KDE bug tracking system if you feel the problem is in
   the implementation.
*/

function FindProxyForURL( url, host )
{
    var result = "PROXY http://" + myIpAddress();

    // plainhost1
    if ( !isPlainHostName( "foo" ) )
        result += "/plainhost1=failed";
    // plainhost2
    if ( isPlainHostName( "foo.bar" ) )
        result += "/plainhost2=failed";

    // dnsdomain1
    if ( !dnsDomainIs( "foo.bar", "bar" ) )
        result += "/dnsdomain1=failed";
    // dnsdomain2
    if ( dnsDomainIs( "foo.baz", "bar" ) )
        result += "/dnsdomain2=failed";

    // localordomain1
    if ( !localHostOrDomainIs( "foo", "bar" ) )
        result += "/localordomain1=failed";
    // localordomain2
    if ( !localHostOrDomainIs( "foo.bar", "foo.bar" ) )
        result += "/localordomain2=failed";
    // localordomain3
    if ( !localHostOrDomainIs( "foo", "foo.baz" ) )
        result += "/localordomain3=failed";
    // localordomain4
    if ( localHostOrDomainIs( "foo.bar", "foo.baz" ) )
        result += "/localordomain4=failed";

    // isresolvable1
    // on failure make sure you can resolve www.kde.org correctly :-)
    if ( !isResolvable( "www.kde.org" ) ) result += "/isresolvable1=failed";
    // isresolvable2
    // on failure make sure dummy.invalid doesn't resolve :-)
    if ( isResolvable( "dummy.invalid" ) ) result += "/isresolvable2=failed";

    // isinnet1
    // on failure check if localhost resolves to 127.0.0.1 as it should
    if ( isInNet( "localhost", "1.2.3.4", "255.0.0.0" ) )
        result += "/isinnet1=failed";
    // isinnet2
    if ( isInNet( "1.2.3.4", "1.2.3.5", "255.255.255.255" ) )
        result += "/isinnet2=failed";
    // isinnet3
    if ( !isInNet( "1.2.3.4", "1.2.3.5", "255.255.255.0" ) )
        result += "/isinnet3=failed";

    // dnsresolve1
    // on failure check if localhost resolves to 127.0.0.1 as it should
    if ( dnsResolve( "localhost" ) != "127.0.0.1" )
        result += "/dnsresolve1=failed";

    // dnslevels1
    if ( dnsDomainLevels( "foo" ) != 0 )
        result += "/dnslevels1=failed";
    // dnslevels2
    if ( dnsDomainLevels( "foo.bar" ) != 1 )
        result += "/dnslevels2=failed";
    // dnslevels3
    if ( dnsDomainLevels( "foo.bar.baz" ) != 2 )
        result += "/dnslevels3=failed";

    // shexp1
    if ( !shExpMatch( "foobar", "foobar" ) )
        result += "/shexp1=failed";
    // shexp2
    if ( shExpMatch( "FoObAr", "foobar" ) )
        result += "/shexp2=failed";
    // shexp3
    if ( !shExpMatch( "Foobar", "?oobar" ) )
        result += "/shexp3=failed";
    // shexp4
    if ( !shExpMatch( "FoObAr", "*b*" ) )
        result += "/shexp4=failed";
    // shexp5
    if ( shExpMatch( "FoObAr", "*x*" ) )
        result += "/shexp5=failed";
    // shexp6
    if ( shExpMatch( "www.kde.org", "*.kde" ) )
        result += "/shexp6=failed";

    var now = new Date;
    var days = new Array( "sun", "mon", "tue", "wed", "thu", "fri", "sat" );
    var months = new Array( "jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec" );

    // weekday1
    if ( !weekdayRange( "sun", "sat" ) )
        result += "/weekday1=failed";
    // weekday2
    if ( !weekdayRange( "sat", "sun" ) )
        result += "/weekday2=failed";
    // weekday3
    if ( !weekdayRange( days[ now.getDay() ] ) )
        result += "/weekday3=failed";
    // weekday4
    if ( weekdayRange( now.getDay() ? "sun" : "mon" ) )
        result += "/weekday4=failed";

    // date1
    if ( !dateRange( now.getDate() ) )
        result += "/date1=failed";
    // date2
    if ( !dateRange( now.getDate(), 31 ) )
        result += "/date2=failed";
    // date3
    if ( !dateRange( 1, now.getDate() ) )
        result += "/date3=failed";
    // date4
    if ( dateRange( now.getDay() > 5 ? 1 : 6, now.getDay() > 5 ? 3 : 8 ) )
        result += "/date4=failed";
    // date5
    if ( !dateRange( months[ now.getMonth() ] ) )
        result += "/date5=failed";
    // date6
    if ( !dateRange( months[ now.getMonth() ], "dec" ) )
        result += "/date6=failed";
    // date7
    if ( !dateRange( "dec", months[ now.getMonth() ] ) )
        result += "/date7=failed";
    // date8
    if ( !dateRange( now.getFullYear() ) )
        result += "/date8=failed";
    // date9
    if ( dateRange( now.getFullYear() - 1 ) )
        result += "/date9=failed";
    // date10
    // if this fails, check your clock first :-)
    if ( dateRange( 1, "jan", 1990, 31, "dec", 2000 ) )
        result += "/date10=failed";
    // date11
    // if this fails, check your clock first :-)
    if ( !dateRange( 1, "jan", 2000, 31, "dec", 3000 ) )
        result += "/date11=failed";

    // time1
    if ( !timeRange( now.getHours() ) )
        result += "/time1=failed";
    // time2
    if ( !timeRange( now.getHours(), now.getMinutes(), 0, 0 ) )
        result += "/time2=failed";
    // time3
    if ( !timeRange( now.getHours(), now.getMinutes(), now.getSeconds(), 0, 0, 0 ) )
        result += "/time3=failed";
    // time4
    if ( timeRange( now.getHours() > 5 ? 1 : 6, now.getHours() > 5 ? 3 : 8 ) )
        result += "/time4=failed";
    // time5
    if ( now.getHours() == now.getUTCHours() )
        result += "/time5=skipped";
    else if ( timeRange( now.getUTCHours() ) )
        result += "/time5=failed";
    // time6
    if ( now.getHours() == now.getUTCHours() )
        result += "/time6=skipped";
    else if ( !timeRange( now.getUTCHours(), "GMT" ) )
        result += "/time6=failed";

    return result;
}

