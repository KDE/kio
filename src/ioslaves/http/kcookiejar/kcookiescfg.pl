#!/usr/bin/perl

while(<>) {
    $treatAllAsSessionCookies = 1 if (/IgnoreExpirationDate=true/);
}

printf("# DELETE IgnoreExpirationDate\n");
if ($treatAllAsSessionCookies) {
  printf("# DELETE CookieGlobalAdvice\n");
  printf("CookieGlobalAdvice=AcceptForSession\n");
}
