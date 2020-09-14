#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <sched.h>
//#include <semaphore.h>

#include "collect_3008.h"
int64_t get_counter();

#define MAX_TESTS 10000

int not_counting = 0;
int not_permitted = 0;

const long nsec = NSEC_PER_SEC/POLL_FREQ;
const double period = (double)nsec/(double)NSEC_PER_SEC;
const long cpufreq = 1500345728;
const long p_cyc = period*(double)cpufreq;
const long cyc_var = p_cyc/10;
const long cyc_ul = p_cyc + cyc_var;
const long cyc_ll = p_cyc - cyc_var;
volatile sig_atomic_t keep_running = 1;
volatile int deleted = 0;

void null_hdlr(int signo)
{
    (void)signo;
    return;
}

struct sigaction null_sa = {.sa_handler = null_hdlr};

//sem_t sem;

timer_t t_id;

int64_t get_counter()
{
  int64_t virtual_timer_value;
  volatile u_int32_t pmccntr;
  u_int32_t pmuseren;
  u_int32_t pmcntenset;
  // Read the user mode perf monitor counter access permissions.
  asm volatile("mrc p15, 0, %0, c9, c14, 0" : "=r"(pmuseren));
  if (pmuseren & 1) {  // Allows reading perfmon counters for user mode code.
    asm volatile("mrc p15, 0, %0, c9, c12, 1" : "=r"(pmcntenset));
    if (pmcntenset & 0x80000000ul) {  // Is it counting?
      asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r"(pmccntr));
      virtual_timer_value = ((int64_t)(pmccntr)); 
      return virtual_timer_value;  // Should optimize to << 6
    } else {
      not_counting = 1;
    }
  } else {
    not_permitted = 1;
  }
  return 0; //not great
}

int64_t prev;
int64_t diff_times[MAX_TESTS];
static volatile int count = MAX_TESTS;

void handler( int signo )
{
    (void)signo;
    int64_t now;

    if(!cap_done_3008()) //if(count >= 0)
    {
        poll_3008();
        now = get_counter();
        if(count >=0)diff_times[count]= now - prev;
        prev = now;
        count--;
        //sem_post(&sem);
    } else {
        signal(SIGALRM, SIG_IGN);
        sigaction(SIGALRM, &null_sa, NULL);
        keep_running = 0;
        //sem_post(&sem);
        //exit(0);
    }
}

int main(int argc, char *argv[])
{
    int i = 0,correct=0;
    char* fstem;  //filename stem for output
    //if(sem_init(&sem, 0, 0)) {
    //    perror("semaphore fail");
    //}

    if(argc > 1)
        fstem = argv[1];  //the filename stem is the first arg
    else
        fstem = "dummy";

    init_3008();

    struct itimerspec tim_spec = {.it_interval= {.tv_sec=0,.tv_nsec=nsec},
                    .it_value = {.tv_sec=1,.tv_nsec=0}};

    struct sigaction act;
    sigset_t set;

    sigemptyset( &set );
    sigaddset( &set, SIGALRM );

    act.sa_flags = 0;
    act.sa_mask = set;
    act.sa_handler = &handler;

    null_sa.sa_flags = 0;
    null_sa.sa_mask = set;

    sigaction( SIGALRM, &act, NULL );

    if (timer_create(CLOCK_MONOTONIC, NULL, &t_id))
        perror("timer_create");

    prev = get_counter();

    if (timer_settime(t_id, 0, &tim_spec, NULL))
        perror("timer_settime");

    while(keep_running) {
        //sem_wait(&sem);
        //pause();
        sched_yield();
    }
    
    //if(!deleted){
    //    timer_delete(t_id);
    //    deleted = 1;
    //}

        for(i=0; i<MAX_TESTS; ++i)
        {
            if(diff_times[i] < cyc_ul && diff_times[i] > cyc_ll)
            {
                printf("%d->\t", i);
                correct++;
            }
            printf("%llu\n", diff_times[i]);
        }
        printf("-> %d\n", correct);
        printf("cyc per meas: %ld\n", p_cyc );
        if(not_permitted) printf("not permitted\n");
        if(not_counting) printf("not counting\n");
        
        dump_data_3008(fstem);
    return 0;
}

