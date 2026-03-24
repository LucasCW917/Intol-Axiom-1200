#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm.h"
#include "parser.h"

static void print_usage(const char *argv0) {
    fprintf(stderr,
        "Intol Axiom 1200 Emulator\n"
        "Usage: %s [options] <program.apo>\n"
        "\n"
        "Options:\n"
        "  -h, --help       Show this help message\n"
        "  -v, --verbose    Print VM status on exit\n"
        "  -s, --source     Print parsed instruction count before running\n"
        "\n"
        "The program file must be a valid .apo assembly source.\n",
        argv0);
}

int main(int argc, char *argv[]) {
    const char *program_file = NULL;
    bool verbose = false;
    bool show_source = false;

    for (int a = 1; a < argc; a++) {
        if (strcmp(argv[a], "-h") == 0 || strcmp(argv[a], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[a], "-v") == 0 || strcmp(argv[a], "--verbose") == 0) {
            verbose = true;
        } else if (strcmp(argv[a], "-s") == 0 || strcmp(argv[a], "--source") == 0) {
            show_source = true;
        } else if (argv[a][0] != '-') {
            program_file = argv[a];
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[a]);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (!program_file) {
        print_usage(argv[0]);
        return 1;
    }

    /* ── Parse ──────────────────────────────────────────────── */
    Program *prog = program_parse_file(program_file);
    if (!prog) {
        fprintf(stderr, "Failed to parse '%s'\n", program_file);
        return 1;
    }

    if (show_source) {
        fprintf(stderr, "[INFO] Parsed %zu instructions, %zu labels from '%s'\n",
                prog->count, prog->label_count, program_file);
    }

    /* ── Create VM and run ──────────────────────────────────── */
    VM *vm = vm_create();
    if (!vm) {
        fprintf(stderr, "Failed to create VM\n");
        program_free(prog);
        return 1;
    }

    VMStatus status = vm_run(vm, prog);

    if (verbose) {
        const char *status_str = "UNKNOWN";
        switch (status) {
            case VM_OK:        status_str = "OK";        break;
            case VM_FLAGGED:   status_str = "FLAGGED";   break;
            case VM_EXCEPTION: status_str = "EXCEPTION"; break;
            case VM_CRASH:     status_str = "CRASH";     break;
            case VM_HALTED:    status_str = "HALTED";    break;
        }
        fprintf(stderr, "[VM] Exit status: %s\n", status_str);
    }

    vm_destroy(vm);
    program_free(prog);

    /* Map VM status to UNIX exit codes */
    switch (status) {
        case VM_OK:
        case VM_HALTED:  return 0;
        case VM_FLAGGED: return 2;
        default:         return 1;
    }
}