# Running ftptest unittest
This test uses the "ftpd" Ruby Gem (https://rubygems.org/gems/ftpd).
This means you need to install that Gem for the test to run correctly; this can
be done either by:
- Installing it from your distribution's repositories if they provide it OR
- Installing it manually by running `gem install --user-install ftpd` which
  typically installs it under your user's home dir, and follow the on screen
  instructions to e.g. make sure the path it installed the Gem at is in your
  $PATH

# Running webdavtest unittest
This test uses the "wsgidav" Python module (https://pypi.org/project/WsgiDAV/).
This means you need to install that Python module for the test to run correctly;
this can be done either by:
- Installing it from your distribution's repos if they provide it OR
- Installing it manually by running `pip3 install wsgidav` which typically installs
  it under your user's home dir, and follow the on screen instructions to e.g. make
  sure the path it installed the Gem at is in your $PATH
