#include "native_tool.h"
int main(int argc,char **argv) {
    if(argc<4) { fputs("usage: ats2-run-tool <executable> <absolute-ats2-root> [args...]\n",stderr); return 2; }
    slash_path(argv[2]);
    for(int i=3;i+1<argc;i++) {
        if(!strcmp(argv[i],"--output")||!strcmp(argv[i],"--dynamic")||!strcmp(argv[i],"--static")) slash_path(argv[++i]);
    }
    if(env_set("PATSHOME",argv[2])||env_set("PATSHOMERELOC",argv[2])) { fputs("ats2-run-tool: setting PATSHOME failed\n",stderr); return 2; }
    char **child=xmalloc((size_t)(argc-1)*sizeof *child);
    child[0]=argv[1]; for(int i=3;i<argc;i++) child[i-2]=argv[i]; child[argc-2]=NULL;
    int rc=run_native(child); free(child); return rc;
}
