#define moc_builder
#include "moc.h"

int main(int argc, char** argv) {
    Moc* yant_moc = moc_begin(KB(20), 50);
    moc_preset_standard_c(yant_moc, "./source", "./include", "./build", "yant");
    moc_add_flags(yant_moc, "-Wall", "-Wextra", "-std=c11", "-g", "-D_POSIX_C_SOURCE=200809L");
    moc_add_source(yant_moc, "./source/impl/*.c");
    moc_set_optimization(yant_moc, MOC_OPT_NONE);
    moc_watch_run_after_build(yant_moc, true);
    moc_dispatch(yant_moc, argc, argv);
    moc_end(&yant_moc);
    return 0;
}
