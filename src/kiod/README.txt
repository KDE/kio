kiod is KIO's daemon, which can run multiple DBus services (modules)

It is similar to kded, but without the KSycoca dependency,
the kdeinit dependency, the dir watching for kbuildsycoca,
the workspace startup phases etc.

kiod makes KIO more independent.

The modules still inherit KDEDModule, they are just hosted by another
daemon.
In kiod, the modules are only loaded on demand via DBus calls.
For this reason, the keys "X-KDE-Kded-autoload" and "X-KDE-Kded-load-on-demand"
have no effect for kiod modules.

