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

check_symbol_exists("__GLIBC__" "stdlib.h" LIBC_IS_GLIBC)
if (LIBC_IS_GLIBC)
    check_cxx_source_compiles("
        #include <fcntl.h>
        #include <sys/stat.h>

        int main() {
            struct statx buf;
            statx(AT_FDCWD, \"/foo\", AT_EMPTY_PATH, STATX_BASIC_STATS, &buf);
            return 0;
        }
    " HAVE_STATX)
    check_cxx_source_compiles(
        "#include <sys/types.h>
        #include <sys/stat.h>
        #include <unistd.h>
        #include <fcntl.h>

        int main()
        {
            struct statx st;
            int res = statx(AT_FDCWD, \".\", AT_NO_AUTOMOUNT, STATX_MNT_ID, &st);
            st.stx_mnt_id = 1;
        }"  HAVE_STATX_MNT_ID)
    check_cxx_source_compiles(
        "#include <sys/types.h>
        #include <sys/stat.h>
        #include <unistd.h>
        #include <fcntl.h>

        int main()
        {
            struct statx st;
            int res = statx(AT_FDCWD, \".\", AT_NO_AUTOMOUNT, STATX_SUBVOL, &st);
            st.stx_subvol = 1;
        }"  HAVE_STATX_SUBVOL)
    check_cxx_source_compiles(
        "#include <sys/types.h>
        #include <sys/stat.h>
        #include <unistd.h>
        #include <fcntl.h>

        int main()
        {
            struct statx st;
            int res = statx(AT_FDCWD, \".\", AT_NO_AUTOMOUNT, STATX_MNT_ID_UNIQUE, &st);
            st.stx_mnt_id = 1;
        }"  HAVE_STATX_MNT_ID_UNIQUE)

else()
    set(HAVE_STATX 0)
    set(HAVE_STATX_MNT_ID 0)
    set(HAVE_STATX_SUBVOL 0)
    set(HAVE_STATX_MNT_ID_UNIQUE 0)
endif()

