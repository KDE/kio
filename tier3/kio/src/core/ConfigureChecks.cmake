include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckStructHasMember)
include(CheckFunctionExists)
include(CheckSymbolExists)

check_function_exists(backtrace        HAVE_BACKTRACE)
check_struct_has_member("struct sockaddr" sa_len "sys/types.h;sys/socket.h" HAVE_STRUCT_SOCKADDR_SA_LEN)

find_package(ACL)
set(HAVE_LIBACL ${ACL_FOUND})
set(HAVE_POSIX_ACL ${ACL_FOUND})
set_package_properties(ACL PROPERTIES DESCRIPTION "LibACL" URL "ftp://oss.sgi.com/projects/xfs/cmd_tars"
                       TYPE RECOMMENDED PURPOSE "Support for manipulating access control lists")


### KMountPoint

check_function_exists(getmntinfo  HAVE_GETMNTINFO)
check_function_exists(setmntent   HAVE_SETMNTENT)

check_include_files(mntent.h      HAVE_MNTENT_H)
check_include_files("stdio.h;sys/mnttab.h"  HAVE_SYS_MNTTAB_H)
check_include_files(sys/mntent.h  HAVE_SYS_MNTENT_H)
check_include_files("sys/param.h;sys/mount.h"  HAVE_SYS_MOUNT_H)

check_include_files(sys/types.h   HAVE_SYS_TYPES_H)
check_include_files(fstab.h       HAVE_FSTAB_H)
check_include_files(sys/param.h   HAVE_SYS_PARAM_H)

check_library_exists(volmgt volmgt_running "" HAVE_VOLMGT)

check_cxx_source_compiles("
  #include <sys/types.h>
  #include <sys/statvfs.h>
  int main(){
  ·struct statvfs *mntbufp;
·  int flags;
·  return getmntinfo(&mntbufp, flags);
·  }
" GETMNTINFO_USES_STATVFS )

###


