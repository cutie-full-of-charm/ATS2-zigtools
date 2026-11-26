#ifndef ATS2_NATIVE_TOOL_H
#define ATS2_NATIVE_TOOL_H
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#include <io.h>
#define ATS2_EXE ".exe"
#else
#include <unistd.h>
#include <sys/wait.h>
#define ATS2_EXE ""
#endif
static inline void *xmalloc(size_t n) { void *p=malloc(n); if (!p) { fputs("ATS2: out of memory\n",stderr); exit(2); } return p; }
static inline char *join(const char *a,const char *b) { size_t n=strlen(a)+strlen(b)+2; char *p=xmalloc(n); snprintf(p,n,"%s/%s",a,b); return p; }
static inline void slash_path(char *p) {
#ifdef _WIN32
    for (;*p;p++) if (*p=='\\') *p='/';
#else
    (void)p;
#endif
}
static inline char *exe_dir(void) {
    size_t cap=512;
    for (;;) {
        char *p=xmalloc(cap);
#ifdef _WIN32
        DWORD n=GetModuleFileNameA(NULL,p,(DWORD)cap);
        if (!n) { free(p); return NULL; }
#else
        ssize_t n=readlink("/proc/self/exe",p,cap-1);
        if (n<0) { free(p); return NULL; }
#endif
        if ((size_t)n<cap-1) { p[n]=0; slash_path(p); char *s=strrchr(p,'/'); if (!s) { free(p); return NULL; } *s=0; return p; }
        free(p); if (cap>=65536) return NULL; cap*=2;
    }
}
static inline int env_set(const char *key,const char *value) {
#ifdef _WIN32
    return _putenv_s(key,value);
#else
    return setenv(key,value,1);
#endif
}
static inline int env_unset(const char *key) {
#ifdef _WIN32
    return _putenv_s(key,"");
#else
    return unsetenv(key);
#endif
}
static inline const char *env_nonempty(const char *key) { const char *v=getenv(key); return v && *v ? v : NULL; }
static inline int file_exists(const char *path) { FILE *f=fopen(path,"rb"); if (!f) return 0; fclose(f); return 1; }
static inline int configure_home(const char *bin) {
    const char *home=env_nonempty("PATSHOME");
    if (!home) {
        if (!bin) { fputs("ATS2: cannot locate executable; set PATSHOME\n",stderr); return 2; }
        char *prefix=xmalloc(strlen(bin)+1); strcpy(prefix,bin); char *last=strrchr(prefix,'/');
        if (!last) { free(prefix); return 2; }
        *last=0;
        char *p=join(prefix,"share/ats2"), *check=join(p,"prelude/params.hats"); free(prefix);
        if (!file_exists(check)) { fprintf(stderr,"ATS2: no runtime at %s; set PATSHOME\n",p); free(check); free(p); return 2; }
        free(check); int rc=env_set("PATSHOME",p); free(p); if (rc) return 2;
    }
    if (!env_nonempty("PATSHOMERELOC")) return env_set("PATSHOMERELOC",getenv("PATSHOME")) ? 2 : 0;
    return 0;
}
#ifdef _WIN32
/* Quote every argv item according to Microsoft CRT parsing rules. */
static inline char *win_command(char *const argv[]) {
    size_t cap=1; for (size_t i=0;argv[i];++i) cap+=2*strlen(argv[i])+4;
    char *cmd=xmalloc(cap),*q=cmd;
    for (size_t i=0;argv[i];++i) {
        if (i) *q++=' ';
        *q++='"'; const char *p=argv[i];
        while (*p) {
            size_t n=0; while (*p=='\\') { ++n; ++p; }
            size_t copies=(*p=='"'||!*p)?2*n:n;
            while (copies--) *q++='\\';
            if (*p=='"') *q++='\\';
            if (*p) *q++=*p++;
        }
        *q++='"';
    }
    *q=0; return cmd;
}
#endif
static inline int run_native_to(char *const argv[], const char *log) {
#ifdef _WIN32
    char *cmd=win_command(argv);
    STARTUPINFOA si; PROCESS_INFORMATION pi; memset(&si,0,sizeof si); memset(&pi,0,sizeof pi); si.cb=sizeof si;
    si.dwFlags=STARTF_USESTDHANDLES; si.hStdInput=GetStdHandle(STD_INPUT_HANDLE); si.hStdOutput=GetStdHandle(STD_OUTPUT_HANDLE); si.hStdError=GetStdHandle(STD_ERROR_HANDLE);
    HANDLE output=INVALID_HANDLE_VALUE;
    if (log) {
        SECURITY_ATTRIBUTES sa={sizeof sa,NULL,TRUE};
        output=CreateFileA(log,GENERIC_WRITE,FILE_SHARE_READ,&sa,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
        if (output==INVALID_HANDLE_VALUE) { free(cmd); return 127; }
        si.hStdOutput=si.hStdError=output;
    }
    BOOL ok=CreateProcessA(NULL,cmd,NULL,NULL,TRUE,0,NULL,NULL,&si,&pi);
    DWORD err=GetLastError(); free(cmd);
    if (output!=INVALID_HANDLE_VALUE) CloseHandle(output);
    if (!ok) { fprintf(stderr,"ATS2: cannot start %s (Win32 error %lu)\n",argv[0],(unsigned long)err); return 127; }
    DWORD code=1; if (WaitForSingleObject(pi.hProcess,INFINITE)==WAIT_OBJECT_0) GetExitCodeProcess(pi.hProcess,&code);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess); return code>255 ? 1 : (int)code;
#else
    pid_t pid=fork(); if (pid<0) { perror("fork"); return 127; }
    if (!pid) {
        if (log) { FILE *f=fopen(log,"wb"); if (!f || dup2(fileno(f),1)<0 || dup2(fileno(f),2)<0) _exit(127); fclose(f); }
        execvp(argv[0],argv); perror(argv[0]); _exit(127);
    }
    int status; while (waitpid(pid,&status,0)<0) { if(errno!=EINTR) { perror("waitpid"); return 127; } }
    return WIFEXITED(status)?WEXITSTATUS(status):128+WTERMSIG(status);
#endif
}
static inline int run_native(char *const argv[]) { return run_native_to(argv,NULL); }
#endif
