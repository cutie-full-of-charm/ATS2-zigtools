#include "native_tool.h"
#include <sys/stat.h>
#ifdef _WIN32
#define MAKE_DIR(p) _mkdir(p)
#define CHANGE_DIR(p) _chdir(p)
#else
#define MAKE_DIR(p) mkdir(p,0700)
#define CHANGE_DIR(p) chdir(p)
#endif
static int check(int condition,const char *label) {
    printf("%s: %s\n",condition?"PASS":"FAIL",label); fflush(stdout); return !condition;
}
static int contains(const char *file,const char *text) {
    FILE *f=fopen(file,"rb"); if(!f) return 0;
    char line[8192]; int found=0; while(fgets(line,sizeof line,f)) if(strstr(line,text)) { found=1; break; } fclose(f); return found;
}
static int copy_file(const char *src,const char *dst) {
    FILE *a=fopen(src,"rb"); if(!a) return 0; FILE *b=fopen(dst,"wb"); if(!b) { fclose(a); return 0; }
    char buf[8192]; size_t n; int ok=1; while((n=fread(buf,1,sizeof buf,a))) if(fwrite(buf,1,n,b)!=n) { ok=0; break; }
    if(ferror(a)) ok=0;
    fclose(a); if(fclose(b)) ok=0; return ok;
}
int main(int argc,char **argv) {
    if(argc!=6) { fputs("usage: test_installed <prefix> <tests-dir> <output-dir> <zig-executable> <legacy|core>\n",stderr); return 2; }
    char *prefix=argv[1],*tests=argv[2],*out=argv[3]; slash_path(prefix); slash_path(tests); slash_path(out);
    if(MAKE_DIR(out)&&errno!=EEXIST) { perror(out); return 2; }
    char *work=join(out,"work with spaces"); if(MAKE_DIR(work)&&errno!=EEXIST) { perror(work); return 2; }
    if(CHANGE_DIR(work)) { perror(work); return 2; }
    const char *unset[]={"PATSHOME","PATSHOMERELOC","PATSHOMELOCS","PATSOPT","PATSCCOMP","ATSCCOMP",NULL};
    for(int i=0;unset[i];i++) if(env_unset(unset[i])) return 2;
    if(env_set("ATS2_ZIG",argv[4])) return 2;
    char *opt=join(prefix,"bin/patsopt" ATS2_EXE),*cc=join(prefix,"bin/patscc" ATS2_EXE);
    int fail=0;
    char *hello=join(tests,"paths/hello.dats"),*relative=join(tests,"paths/nested/relative.dats");
    if(!copy_file(hello,"hello world.dats")) { perror("copy hello"); return 2; }
    char *absolute_test[]={opt,"--output","absolute.c","--dynamic",hello,NULL};
    fail+=check(run_native(absolute_test)==0&&file_exists("absolute.c"),"installed-absolute-path");
    char *relative_test[]={opt,"--output","relative.c","--dynamic",relative,NULL};
    fail+=check(run_native(relative_test)==0&&file_exists("relative.c"),"installed-relative-imports");
    char *compile[]={cc,"-cleanaft","-o","hello world" ATS2_EXE,"hello world.dats",NULL};
    int rc=run_native(compile);
    fail+=check(rc==0&&file_exists("hello world" ATS2_EXE)&&!file_exists("hello world_dats.c"),"installed-patscc-compile");
    if(!rc) { char *exe[]={"./hello world" ATS2_EXE,NULL}; fail+=check(run_native_to(exe,"hello.log")==0&&contains("hello.log","ATS2 installed toolchain ok"),"installed-patscc-run"); }
    else fail++;
    char *typecheck[]={cc,"-tcats","hello world.dats",NULL};
    fail+=check(run_native(typecheck)==0,"installed-patscc-typecheck");
    char *generate[]={cc,"-ccats","-o","hello generated.c","hello world.dats",NULL};
    fail+=check(run_native(generate)==0&&file_exists("hello generated.c"),"installed-patscc-ccats");
    /* Verify a consumer actually links the installed libatslib and socket header. */
    char *socket_source=join(tests,"stdlib/socket_typed_smoke.dats");
    char *socket_compile[]={cc,"-cleanaft","-o","socket smoke" ATS2_EXE,socket_source,NULL};
    rc=run_native(socket_compile); fail+=check(rc==0,"installed-socket-link");
    if(!rc) { char *exe[]={"./socket smoke" ATS2_EXE,NULL}; fail+=check(run_native_to(exe,"socket.log")==0&&contains("socket.log","socket typed ok"),"installed-socket-run"); }
    else fail++;
    char *dirent=join(tests,"stdlib/dirent_portable.c");
    char *dirent_compile[]={cc,"-o","dirent smoke" ATS2_EXE,dirent,NULL};
    rc=run_native(dirent_compile); fail+=check(rc==0,"installed-dirent-link");
    if(!rc) { char *exe[]={"./dirent smoke" ATS2_EXE,NULL}; fail+=check(run_native_to(exe,"dirent.log")==0&&contains("dirent.log","dirent portable ok"),"installed-dirent-run"); }
    else fail++;
    if(!strcmp(argv[5],"legacy")) {
        char *source=join(tests,"stdlib/socket_legacy_ats_smoke.dats");
        char *command[]={cc,"-cleanaft","-o","legacy smoke" ATS2_EXE,source,NULL};
        rc=run_native(command); fail+=check(rc==0,"installed-legacy-socket-link");
        if(!rc) { char *exe[]={"./legacy smoke" ATS2_EXE,NULL}; fail+=check(run_native_to(exe,"legacy.log")==0&&contains("legacy.log","socket legacy ATS ok"),"installed-legacy-socket-run"); }
        else fail++;
    }
    const char *negative[]={"net_closed_local","net_double_close","net_free_live_as_null","net_tcp_send_unconnected","net_udp_send_oversize","socket_double_close",NULL};
    for(int i=0;negative[i];i++) {
        char *base=join(tests,"negative"),*stem=join(base,negative[i]); free(base);
        size_t n=strlen(stem)+6; char *source=xmalloc(n); snprintf(source,n,"%s.dats",stem); free(stem);
        if(!file_exists(source)) { fail+=check(0,negative[i]); free(source); continue; }
        char *reject[]={opt,"--typecheck","--dynamic",source,NULL};
        char log[128]; snprintf(log,sizeof log,"%s.log",negative[i]);
        int r=run_native_to(reject,log);
        int expected=contains(log,"no longer available")||contains(log,"unsolved constraint");
        int unexpected=contains(log,"not available for staloading")||contains(log,"not available for inclusion")||contains(log,"fopen(");
        fail+=check(r!=0&&r!=127&&expected&&!unexpected,negative[i]); free(source);
    }
    /* Keep the previous public-ABI opacity checks in the unified suite. */
    const char *bodies[]={"ats2_net_socket *p;", "ats2_net_socket s;",
        "void bad(ats2_net_socket *s) { s->handle = 0; }",
        "void bad(ats2_net_socket *s) { ats2_net_socket_init(s); }"};
    const char *labels[]={"public-opaque-pointer", "reject-public-stack-allocation",
        "reject-public-field-access", "reject-hidden-init"};
    for(int i=0;i<4;i++) {
        char source[128], object[128], log[128];
        snprintf(source,sizeof source,"%s.c",labels[i]);
        snprintf(object,sizeof object,"%s.o",labels[i]);
        snprintf(log,sizeof log,"%s.log",labels[i]);
        if(remove(object) && errno!=ENOENT) { perror(object); return 2; }
        FILE *f=fopen(source,"wb"); if(!f) { perror(source); return 2; }
        fprintf(f,"#include \"ats2_net_socket.h\"\n%s\n",bodies[i]);
        if(fclose(f)) { perror(source); return 2; }
        /* Compile without linking. Avoid -fsyntax-only and blanket -Werror:
         * Zig's injected target flags can trigger unused-argument diagnostics.
         * Only implicit function declarations need explicit promotion. */
        char *compile_c[]={cc,"-c","-Werror=implicit-function-declaration","-o",object,source,NULL};
        int r=run_native_to(compile_c,log);
        int expected=0;
        if(i==1 || i==2) {
            expected=contains(log,"ats2_net_socket") &&
                (contains(log,"incomplete") || contains(log,"never completed") ||
                 contains(log,"storage size") || contains(log,"undefined type"));
        } else if(i==3) {
            expected=contains(log,"ats2_net_socket_init") &&
                (contains(log,"undeclared function") || contains(log,"implicit declaration"));
        }
        int ok=i==0 ? (r==0 && file_exists(object)) :
            (r!=0 && r!=127 && !file_exists(object) && expected);
        fail+=check(ok,labels[i]);
        if(!ok) {
            fprintf(stderr,"%s: compiler exit=%d; diagnostics from %s:\n",labels[i],r,log);
            FILE *diag=fopen(log,"rb");
            if(diag) { char buf[4096]; size_t n; while((n=fread(buf,1,sizeof buf,diag))) fwrite(buf,1,n,stderr); fclose(diag); }
            else perror(log);
            fputc('\n',stderr); fflush(stderr);
        }
    }
    printf("Installed toolchain suite: %d failed\n",fail); return fail?1:0;
}
