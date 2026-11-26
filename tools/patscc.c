#include "native_tool.h"
struct args { char **v; size_t n,cap; };
static void push(struct args *a,const char *s) {
    if(a->n+1>=a->cap) { a->cap=a->cap?2*a->cap:32; char **p=realloc(a->v,a->cap*sizeof *p); if(!p) exit(2); a->v=p; }
    a->v[a->n++]=(char*)s; a->v[a->n]=NULL;
}
static int eq(const char *a,const char *b) { return strcmp(a,b)==0; }
static int suffix(const char *s,const char *x) { size_t n=strlen(s),m=strlen(x); return n>=m && eq(s+n-m,x); }
static int starts(const char *s,const char *x) { return strncmp(s,x,strlen(x))==0; }
static void usage(void) { puts("Usage: patscc [ATS options] [zig cc options] file.dats ...\n"
"  -ccats  generate C only     -tcats  typecheck only\n"
"  -fsats FILE / -fdats FILE   explicitly classify ATS input\n"
"  -DATS / -IATS VALUE         ATS definition / include path\n"
"  -DDATS / -IIATS VALUE       pass definition / include to ATS and C\n"
"  -cleanaft  remove generated C after successful compile\n"
"  -verbose   print executed argv; -vats/-hats query patsopt\n"
"  --version  show zig cc version; --help show this help\n"
"  ATS2_ZIG and PATSOPT override executable paths (not command strings).\n"
"  --zig-exe FILE overrides the Zig executable for this invocation.\n"
"  -atsccomp accepts 'zig cc' or a single C compiler executable.\n"
"  Normal C flags, objects and libraries are forwarded to zig cc."); }
static int launch(struct args *a,int verbose) { if(verbose) { fputs("+",stderr); for(size_t i=0;i<a->n;i++) fprintf(stderr," [%s]",a->v[i]); fputc('\n',stderr); } return run_native(a->v); }
static char *outname(const char *path,int static_input) {
    const char *b=path; for(const char *p=path;*p;p++) if(*p=='/' || *p=='\\') b=p+1;
    size_t n=strlen(b); if(suffix(b,".dats")||suffix(b,".sats")) n-=5;
    const char *ext=static_input?"_sats.c":"_dats.c"; char *out=xmalloc(n+strlen(ext)+1); memcpy(out,b,n); strcpy(out+n,ext); return out;
}
static int takes_cc_arg(const char *s) {
    const char *opts[]={"-o","-I","-L","-D","-U","-include","-imacros","-isystem","-iquote","-idirafter","-isysroot","--sysroot","-target","--target","-x","-Xclang","-Xlinker","-MF","-MT","-MQ","-B","-arch","-l",NULL};
    for(int i=0;opts[i];i++) if(eq(s,opts[i])) return 1;
    return 0;
}
int main(int argc,char **argv) {
    if(argc==1 || (argc==2 && (eq(argv[1],"--help")||eq(argv[1],"-h")))) { usage(); return 0; }
    char *bin=exe_dir(); if(!bin || configure_home(bin)) return 2;
    const char *home=getenv("PATSHOME"); const char *opt=env_nonempty("PATSOPT"); if(!opt) opt=join(bin,"patsopt" ATS2_EXE);
    const char *zig=env_nonempty("ATS2_ZIG"); if(!zig) zig="zig" ATS2_EXE;
    const char *cc=env_nonempty("PATSCCOMP"); if(!cc) cc="zig cc";
    struct args ats={0}, cflags={0}, inputs={0}, generated={0};
    int tc=0, ccats=0, clean=0, verbose=0, link=1, alloc=0, explicit_target=0, version=0;
    const char *output=NULL;
    /* Pass 1: parse global mode and ATS options, preserving C argument order in pass 2. */
    for(int i=1;i<argc;i++) {
        const char *a=argv[i];
        if(eq(a,"-tcats")) tc=1; else if(eq(a,"-ccats")) ccats=1;
        else if(eq(a,"-cleanaft")) clean=1; else if(eq(a,"-verbose")) verbose=1;
        else if(eq(a,"-vats")||eq(a,"-hats")) { char *q[]={(char*)opt,eq(a,"-vats")?"--version":"--help",NULL}; return run_native(q); }
        else if(eq(a,"--version")) version=1;
        else if(eq(a,"-atsccomp")) { if(++i==argc) goto missing; cc=argv[i]; }
        else if(eq(a,"--zig-exe")) { if(++i==argc) goto missing; zig=argv[i]; }
        else if(eq(a,"-DATS")||eq(a,"-DDATS")||eq(a,"-IATS")||eq(a,"-IIATS")) { push(&ats,starts(a,"-D")?"-DATS":"-IATS"); if(++i==argc) goto missing; push(&ats,argv[i]); }
        else if(eq(a,"--gline")||eq(a,"--tlcalopt-disable")||eq(a,"--constraint-ignore")) push(&ats,a);
        else if(eq(a,"-fsats")||eq(a,"-fdats")) { if(++i==argc) goto missing; }
        else if(takes_cc_arg(a)) { if(++i==argc) goto missing; if(eq(a,"-o")) output=argv[i]; if(eq(a,"-target")||eq(a,"--target")) explicit_target=1; if(eq(a,"-D")&&starts(argv[i],"ATS_MEMALLOC_")) alloc=1; }
        else if(eq(a,"-c")||eq(a,"-S")||eq(a,"-E")||eq(a,"-fsyntax-only")||eq(a,"-M")||eq(a,"-MM")) link=0;
        else if(starts(a,"-DATS_MEMALLOC_")) alloc=1;
        else if(starts(a,"--target=")) explicit_target=1;
    }
    if(tc&&ccats) { fputs("patscc: -tcats and -ccats are mutually exclusive\n",stderr); return 2; }
    int zig_mode=eq(cc,"zig cc")||eq(cc,"zig.exe cc");
    if(!zig_mode && (strchr(cc,'\n')||strchr(cc,'\r'))) return 2;
    push(&cflags,zig_mode?zig:cc); if(zig_mode) push(&cflags,"cc");
    if(version) { push(&cflags,"--version"); return launch(&cflags,verbose); }
    if(explicit_target && link) { fputs("patscc: installed libatslib is native-target only; use a matching toolchain or -c\n",stderr); return 2; }
    push(&cflags,"-std=gnu11"); push(&cflags,"-D_XOPEN_SOURCE=700"); if(!alloc) push(&cflags,"-DATS_MEMALLOC_LIBC");
    push(&cflags,"-I"); push(&cflags,home); push(&cflags,"-I"); push(&cflags,join(home,"ccomp/runtime"));
    if(link) { push(&cflags,"-L"); push(&cflags,join(bin,"../lib")); }
    for(int i=1;i<argc;i++) {
        const char *a=argv[i]; int stat=0, is_ats=0;
        if(eq(a,"-tcats")||eq(a,"-ccats")||eq(a,"-cleanaft")||eq(a,"-verbose")||eq(a,"--gline")||eq(a,"--tlcalopt-disable")||eq(a,"--constraint-ignore")) continue;
        if(eq(a,"-atsccomp")||eq(a,"--zig-exe")||eq(a,"-DATS")||eq(a,"-IATS")) { i++; continue; }
        if(eq(a,"-DDATS")||eq(a,"-IIATS")) { push(&cflags,eq(a,"-DDATS")?"-D":"-I"); push(&cflags,argv[++i]); continue; }
        if(takes_cc_arg(a)) { push(&cflags,a); push(&cflags,argv[++i]); continue; }
        if(eq(a,"-fsats")||eq(a,"-fdats")) { stat=eq(a,"-fsats"); a=argv[++i]; is_ats=1; }
        else if(a[0]!='-'&&(suffix(a,".dats")||suffix(a,".sats"))) { stat=suffix(a,".sats"); is_ats=1; }
        if(!is_ats) { push(&cflags,a); continue; }
        push(&inputs,a);
        struct args cmd={0}; push(&cmd,opt);
        for(size_t j=0;j<ats.n;j++) push(&cmd,ats.v[j]);
        char *out=NULL;
        if(tc) push(&cmd,"--typecheck"); else {
            out=outname(a,stat);
            if(ccats&&output) { if(inputs.n>1) { fputs("patscc: -ccats -o requires one ATS input\n",stderr); return 2; } out=(char*)output; }
            for(size_t j=0;j<generated.n;j++) if(eq(generated.v[j],out)) { fprintf(stderr,"patscc: duplicate generated C filename: %s\n",out); return 2; }
            push(&cmd,"--output"); push(&cmd,out);
        }
        push(&cmd,stat?"--static":"--dynamic"); push(&cmd,a);
        int rc=launch(&cmd,verbose); free(cmd.v); if(rc) return rc;
        if(out) { push(&generated,out); push(&cflags,out); }
    }
    if(tc||ccats) { if(!inputs.n) { fputs("patscc: no ATS inputs\n",stderr); return 2; } return 0; }
    if(link) { push(&cflags,"-latslib");
#ifdef _WIN32
        push(&cflags,"-lws2_32"); push(&cflags,"-ladvapi32"); push(&cflags,"-lkernel32");
#else
        push(&cflags,"-lm");
#endif
    }
    int rc=launch(&cflags,verbose);
    if(!rc&&clean) for(size_t j=0;j<generated.n;j++) if(remove(generated.v[j])) { perror(generated.v[j]); rc=1; }
    return rc;
missing: fputs("patscc: option requires a value\n",stderr); return 2;
}
