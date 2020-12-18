#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <tiff.h>

#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

char *buffer;
int pagesize;

static void
handler(int sig, siginfo_t *si, void *unused);

void allocate_memory();

uint64 pc;
uint64 rsp;
uint64 rbp;


/**
 * jvm safe-point working mechanism,
 * this code show how jvm suspend working-thread.
 * @param argc
 * @param argv
 * @return
 */
int main(int argc, char *argv[]) {
    /*
     * allocate memory and set memory access READ
     */
    allocate_memory();
    char *p = buffer;

    /*
     * save context like register pc rsp rbp,
     * we manually save those registers to c global variables,
     * jvm save those registers for recovery they, we simulate JVM's behavior too
     */
    uint64 local_pc = 0;
    uint64 local_rsp = 0;
    uint64 local_rbp = 0;
    __asm__(".intel_syntax;"
            "lea rax, [rip];"
            "mov [rbp-0x20], rax;"
            "mov [rbp-0x28], rsp;"
            "mov [rbp-0x30], rbp;"
    );
    pc = local_pc;
    rsp = local_rsp;
    rbp = local_rbp;


    for (int i = 0; i < 4; i++) {
        /*
         * we access the pointer p, current thread will handle signal
         * jvm will do
         */
        *(p) = 'a';
        p++;
    }
    *(p) = '\0';
    printf("p = %s\n", buffer);
    //printf("%x",local_pc);


    /*
     * if we didn't restore those registers,
     * it should not happen.
     */
    printf("Loop completed\n");
    exit(EXIT_SUCCESS);
}


void allocate_memory() {

    /*
     * register signal handle
     */
    struct sigaction sa;

    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = handler;
    if (sigaction(SIGSEGV, &sa, NULL) == -1)
        handle_error("sigaction");

    pagesize = sysconf(_SC_PAGE_SIZE);
    if (pagesize == -1)
        handle_error("sysconf");

    /*
     * allocate a buffer aligned on a page boundary,
     * initial protection is PROT_READ | PROT_WRITE
     */

    buffer = memalign(pagesize, 4 * pagesize);
    if (buffer == NULL)
        handle_error("memalign");

    printf("Start of region:        0x%lx\n", (long) buffer);

    //set allocated memory access as readonly
    if (mprotect(buffer, pagesize * 4,
                 PROT_READ) == -1)
        handle_error("mprotect");
}

static void handler(int sig, siginfo_t *si, void *unused) {
    /*
     * if memory access signal happen, this handler will be called,
     */
    printf("Got SIGSEGV at address: 0x%lx\n",
           (long) si->si_addr);

    //allow to write , otherwise operation system send signal again.
    if (mprotect(buffer, pagesize * 4,
                PROT_READ | PROT_WRITE) == -1)
        handle_error("mprotect");

    /*
     * jvm will suspend current thread, wait STW end.
     * other GC thread do GC()
     */

    //recovery those register
    //jmp to previous code , make current thread work
    uint64 local_rsp = rsp;
    uint64 local_pc = pc;
    uint64 local_rbp = rbp;
    __asm__(".intel_syntax;"
            "mov rsp,[rbp-0x20];"
            "mov rax,[rbp-0x28];"
            "mov rbp,[rbp-0x30];"
            "jmp rax;"
    );

    //never happen
    printf("rsp:%x", local_rsp);
    printf("pc:%x", local_pc);
    printf("rbp:%x", local_rbp);
    exit(EXIT_FAILURE);
}