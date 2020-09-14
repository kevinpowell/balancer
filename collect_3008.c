#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <time.h>
#include <sys/time.h>
#include <getopt.h>
#include <errno.h>

#include "collect_3008.h"

#define MAX(a,b) (((a)>(b))?(a):(b))

#define maxframes MAX(10000, (POLL_FREQ*CAP_SECS))

int64_t get_counter();  //declared in main_timer
extern const long cpufreq; //defined in main_timer

static int fds[3];
static volatile int framecount=0;
static frame_t data[maxframes];

char *sysfs_names[] = {"/sys/bus/iio/devices/iio:device0/in_voltage1-voltage0_raw", "/sys/bus/iio/devices/iio:device0/in_voltage3-voltage2_raw", "/sys/bus/iio/devices/iio:device0/in_voltage7_raw"};

void init_3008()
{
   for(int i=0; i<3;i++)
   {
       fds[i] = open(sysfs_names[i], O_RDONLY);
       if(fds[i]<0) {
           perror("open()");
           printf("%s\n", sysfs_names[i]);
       }
   }
}

void deinit_3008()
{

}

int fast_atoi(const char *buff)
{
    int c = 0, sign = 0, x = 0;
    const char *p = buff;

    for(c = *(p++); (c < 48 || c > 57); c = *(p++)) {if (c == 45) {sign = 1; c = *(p++); break;}}; // eat whitespaces and check sign
    for(; c > 47 && c < 58; c = *(p++)) x = (x << 1) + (x << 3) + c - 48;

    return sign ? -x : x;
}

int read_adc(int fd) {
   char sval[10];
   int val=0;
   memset(sval, 0, 10);
   if(read(fd, sval, 10)<0) {
       perror("read()");
   } else {
       val = fast_atoi(sval);
       lseek(fd, 0, SEEK_SET);
   }
   return val;
}


void poll_3008()
{
    int val;
    int64_t ts=0;

    if(framecount >= maxframes) return;
    val = read_adc(fds[0]);
    ts = get_counter();
    data[framecount].ch1.val = val;
    data[framecount].ch1.ts = ts;

    val = read_adc(fds[1]);
    ts = get_counter();
    data[framecount].ch2.val = val;
    data[framecount].ch2.ts = ts;

    val = read_adc(fds[2]);
    ts = get_counter();
    data[framecount].rpm.val = val;
    data[framecount].rpm.ts = ts;

    framecount++;

}

int cap_done_3008()
{
 return framecount >= maxframes;
}

int64_t ts_to_nano(int64_t ts)
{
    double fracsec = (double)ts/(double)cpufreq;
    return (int64_t)(fracsec * (double)NSEC_PER_SEC);
}

void dump_data_3008(char* filename)
{
   //filename is a stem, we create 3 files based on it.
   char fnamec1[80], fnamec2[80], fnamerpm[80];
   sprintf(fnamec1, "%s-ch1.csv", filename);
   sprintf(fnamec2, "%s-ch2.csv", filename);
   sprintf(fnamerpm, "%s-rpm.csv", filename);
   FILE* fc1 = fopen(fnamec1, "w");
   FILE* fc2 = fopen(fnamec2, "w");
   FILE* frpm = fopen(fnamerpm, "w");
   int64_t c1_l=data[0].ch1.ts, c2_l=data[0].ch2.ts, rpm_l=data[0].rpm.ts;
   int64_t c1_offset=0, c2_offset=0, rpm_offset=0;
   for(int i=0; i<framecount; i++) {
       //convert cycle-count timpestamps to nanos here 
       int64_t tmp = data[i].ch1.ts;
       if(c1_l-tmp>2147483647) c1_offset+=4294967295;
       fprintf(fc1,"%lld,%d\n", ts_to_nano(data[i].ch1.ts+c1_offset), data[i].ch1.val);
       c1_l = tmp;

       tmp = data[i].ch2.ts;
       if(c2_l-tmp>2147483647) c2_offset+=4294967295;
       fprintf(fc2,"%lld,%d\n", ts_to_nano(data[i].ch2.ts+c2_offset), data[i].ch2.val);
       c2_l = tmp;

       tmp = data[i].rpm.ts;
       if(rpm_l-tmp>2147483647) rpm_offset+=4294967295;
       fprintf(frpm,"%lld,%d\n", ts_to_nano(data[i].rpm.ts+rpm_offset), data[i].rpm.val);
       rpm_l = tmp;
   }
}

