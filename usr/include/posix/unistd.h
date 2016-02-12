#ifndef UNISTD_H
#define UNISTD_H

#include <sys/types.h>

#ifndef NOVFORK
#include <setjmp.h>
#include <alloca.h>
#endif

//
// Values for the second argument to access
//

#define R_OK    4               // Test for read permission
#define W_OK    2               // Test for write permission
#define X_OK    1               // Test for execute permission
#define F_OK    0               // Test for existence



#define SEEK_SET       0       // Seek relative to begining of file
#define SEEK_CUR       1       // Seek relative to current file position
#define SEEK_END       2       // Seek relative to end of file


//
// Locking functions
//

#define F_ULOCK        1       // Unlock locked sections
#define F_LOCK         2       // Lock a section for exclusive use
#define F_TLOCK        3       // Test and lock a section for exclusive use
#define F_TEST         4       // Test a section for locks by other processes

//
// Context for vfork()
//

#ifndef NOVFORK

struct _forkctx {
  struct _forkctx *prev;
  int pid;
  jmp_buf jmp;
  int fd[3];
  char **env;
};

#endif

typedef unsigned int useconds_t;

#ifdef  __cplusplus
extern "C" {
#endif

#ifdef LARGEFILES
#define lseek(f, offset, origin) lseek64((f), (offset), (origin))
#define ftruncate(f, size) ftruncate64((f), (size))
#else
loff_t lseek(handle_t f, loff_t offset, int origin);
int ftruncate(handle_t f, loff_t size);
#endif

int ftruncate64(handle_t f, off64_t size);
off64_t lseek64(handle_t f, off64_t offset, int origin);

int access(const char *name, int mode);
int close(handle_t h);
int fsync(handle_t f);
int read(handle_t f, void *data, size_t size);
int write(handle_t f, const void *data, size_t size);
#ifdef LARGEFILES
int pread(handle_t f, void *data, size_t size, off64_t offset);
int pwrite(handle_t f, const void *data, size_t size, off64_t offset);
#endif
int pipe(handle_t fildes[2]);
int chdir(const char *name);
char *getcwd(char *buf, size_t size);
handle_t dup(handle_t h);
handle_t dup2(handle_t h1, handle_t h2);
int link(const char *oldname, const char *newname);
int unlink(const char *name);
int mkdir(const char *name, int mode);
int rmdir(const char *name);
int chown(const char *name, int owner, int group);
int fchown(handle_t f, int owner, int group);
int isatty(handle_t f);

int readlink(const char *name, char *buf, size_t size);
int symlink(const char *oldpath, const char *newpath);
int chroot(const char *path);
int lockf(handle_t f, int func, off_t size);

int gethostname(char *name, int namelen);
int getpagesize();

void _exit(int status);
unsigned sleep(unsigned seconds);
int usleep(useconds_t usec);
unsigned alarm(unsigned seconds);
char *crypt(const char *key, const char *salt);

pid_t getpid();
pid_t getppid();
int getuid();
int getgid();
int setuid(uid_t uid);
int setgid(gid_t gid);
int geteuid();
int getegid();
int seteuid(uid_t uid);
int setegid(gid_t gid);
int getgroups(int size, gid_t list[]);

int __getstdhndl(int n);

#define STDIN_FILENO  __getstdhndl(0)
#define STDOUT_FILENO __getstdhndl(1)
#define STDERR_FILENO __getstdhndl(2)

#ifndef NOGETOPT
#ifndef _GETOPT_DEFINED
#define _GETOPT_DEFINED

int *_opterr();
int *_optind();
int *_optopt();
char **_optarg();

#define opterr (*_opterr())
#define optind (*_optind())
#define optopt (*_optopt())
#define optarg (*_optarg())

int getopt(int argc, char **argv, char *opts);
#endif
#endif

#ifndef NOVFORK

struct _forkctx *_vfork(struct _forkctx *fc);
int execve(const char *path, char *argv[], char *env[]);
int execv(const char *path, char *argv[]);
int execl(const char *path, char *arg0, ...);

#define vfork() (setjmp(_vfork((struct _forkctx *) alloca(sizeof(struct _forkctx)))->jmp))

#endif


char ***_environ();
#define environ (*_environ())

#define CVTBUFSIZE        (309 + 43)
#define ASCBUFSIZE        (26 + 2)
#define CRYPTBUFSIZE      14

#define P_WAIT      0

int spawn(int mode, const char *pgm, const char *cmdline, char *env[], void*tibptr);
#define MAXPATH                 256     // Maximum filename length (including trailing zero)
#define MFSNAMELEN              16      // Length of fs type name

#define PS1                     '/'     // Primary path separator
#define PS2                     '\\'    // Alternate path separator

int canonicalize(const char *filename, char *buffer, int size);

struct meminfo {
  unsigned int physmem_total;
  unsigned int physmem_avail;
  unsigned int virtmem_total;
  unsigned int virtmem_avail;
  unsigned int pagesize;
};

int sysinfo(int cmd, void *data, unsigned size);

#define SYSINFO_CPU  0
#define SYSINFO_MEM  1
#define SYSINFO_LOAD 2

#define PAGESIZE  4096

int msleep(int millisecs);

//
// File open flags (type and permissions)
//

#define O_ACCMODE               0x0003  // Access mode
#define O_RDONLY                0x0000  // Open for reading only
#define O_WRONLY                0x0001  // Open for writing only
#define O_RDWR                  0x0002  // Open for reading and writing
#define O_SPECIAL               0x0004  // Open for special access
#define O_APPEND                0x0008  // Writes done at EOF

#define O_RANDOM                0x0010  // File access is primarily random
#define O_SEQUENTIAL            0x0020  // File access is primarily sequential
#define O_TEMPORARY             0x0040  // Temporary file bit
#define O_NOINHERIT             0x0080  // Child process doesn't inherit file

#define O_CREAT                 0x0100  // Create and open file
#define O_TRUNC                 0x0200  // Truncate file
#define O_EXCL                  0x0400  // Open only if file doesn't already exist
#define O_DIRECT                0x0800  // Do not use cache for reads and writes

#define O_SHORT_LIVED           0x1000  // Temporary storage file, try not to flush
#define O_NONBLOCK              0x2000  // Open in non-blocking mode
#define O_TEXT                  0x4000  // File mode is text (translated)
#define O_BINARY                0x8000  // File mode is binary (untranslated)

//
// File mode flags (type and permissions)
//


#define S_IFMT         0170000         // File type mask
#define S_IFPKT        0160000         // Packet device
#define S_IFSOCK       0140000         // Socket
#define S_IFLNK        0120000         // Symbolic link
#define S_IFREG        0100000         // Regular file
#define S_IFBLK        0060000         // Block device
#define S_IFDIR        0040000         // Directory
#define S_IFCHR        0020000         // Character device
#define S_IFIFO        0010000         // Pipe

#define S_IREAD        0000400         // Read permission, owner
#define S_IWRITE       0000200         // Write permission, owner
#define S_IEXEC        0000100         // Execute/search permission, owner

#define S_ISLNK(m)      (((m) & S_IFMT) == S_IFLNK)
#define S_ISREG(m)      (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)      (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)      (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)      (((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m)     (((m) & S_IFMT) == S_IFIFO)
#define S_ISSOCK(m)     (((m) & S_IFMT) == S_IFSOCK)
#define S_ISPKT(m)      (((m) & S_IFMT) == S_IFPKT)

#define S_IRWXU 00700
#define S_IRUSR 00400
#define S_IWUSR 00200
#define S_IXUSR 00100

#define S_IRWXG 00070
#define S_IRGRP 00040
#define S_IWGRP 00020
#define S_IXGRP 00010

#define S_IRWXO 00007
#define S_IROTH 00004
#define S_IWOTH 00002
#define S_IXOTH 00001

#define S_IRWXUGO 00777

typedef int handle_t;

handle_t open(const char *name, int oflags, int mode);
handle_t creat(const char *name, int mode);
int close(handle_t h);
int fsync(handle_t f);
handle_t dup(handle_t h);
handle_t dup2(handle_t h1, handle_t h2);
int read(handle_t f, void *data, size_t size);
int write(handle_t f, const void *data, size_t size);
loff_t tell(handle_t f);
loff_t lseek(handle_t f, loff_t offset, int origin);
int access(const char *name, int mode);
int link(const char *oldname, const char *newname);
int unlink(const char *name);

#ifdef  __cplusplus
}
#endif

#endif
