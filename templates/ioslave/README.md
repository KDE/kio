KIO Slave Template
----------------------

### Build instructions

```
cd /where/your/project/is/created
mkdir build
cd build
cmake \
    -DKDE_INSTALL_PLUGINDIR=`kf5-config --qt-plugins` \
    -DKDE_INSTALL_KSERVICESDIR=`kf5-config --install services` \
    -DKDE_INSTALL_LOGGINGCATEGORIESDIR=`kf5-config --install data`qlogging-categories5 \
    -DCMAKE_BUILD_TYPE=Release  ..
make
make install
```

After this you can test the new protocol with:  
`kioclient5 'ls' 'myproto:///'`  
`kioclient5 'cat' 'myproto:///Item A'`

You can also explore the new protocol with dolphin or konqueror:  
`dolphin myproto:///`


### Adaption instructions

Because the KAppTemplate format does not yet allow custom variables,
the template uses "myproto" in the generated sources as the initial name of the scheme.
Grep for all instances of that string in all files and replace it with the name of your scheme,
and rename the `myproto.protocol` file accordingly.

The class `MyDataSystem` just serves as a starting point to see something working right
after creating the project. Replace that class and the calls to it with code as needed to map
the KIO system to the actual data service/system your KIO slave is wrapping.


### Related documentation

Find the documentation of `KIO::SlaveBase` for the API to implement at
https://api.kde.org/frameworks/kio/html/classKIO_1_1SlaveBase.html

Learn about debugging your new KIO slave at
https://community.kde.org/Guidelines_and_HOWTOs/Debugging/Debugging_IOSlaves
