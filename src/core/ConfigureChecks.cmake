include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckStructHasMember)
include(CheckFunctionExists)
include(CheckSymbolExists)

check_struct_has_member("struct sockaddr" sa_len "sys/socket.h" HAVE_STRUCT_SOCKADDR_SA_LEN)

### KMountPoint

check_function_exists(getmntinfo  HAVE_GETMNTINFO)

check_include_files("sys/param.h;sys/mount.h"  HAVE_SYS_MOUNT_H)

check_include_files(fstab.h       HAVE_FSTAB_H)
check_include_files(sys/param.h   HAVE_SYS_PARAM_H)

check_cxx_source_compiles("
  #include <sys/types.h>
  #include <sys/statvfs.h>
  int main(){
    struct statvfs *mntbufp;
    int flags;
    return getmntinfo(&mntbufp, flags);
  }
" GETMNTINFO_USES_STATVFS )
