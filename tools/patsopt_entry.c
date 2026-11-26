#include "native_tool.h"
extern int ats2_patsopt_main(int argc,char **argv);
int main(int argc,char **argv) {
    char *bin=exe_dir(); int rc=configure_home(bin); free(bin);
    return rc ? rc : ats2_patsopt_main(argc,argv);
}
