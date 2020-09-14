
#define NSEC_PER_SEC 1000000000L

//10k rpm is 166Hz, so 400Hz sampling might get us there, but we need to get our 10k samples asap so go much faster 
#define POLL_FREQ 2000
#define CAP_SECS 5

typedef struct {
    int64_t ts;
    short  val;
} samp_t;

typedef struct {
    samp_t ch1;
    samp_t ch2;
    samp_t rpm;
} frame_t;


void init_3008(void);
void deinit_3008(void);
void poll_3008(void);
void dump_data_3008(char* filename);
int cap_done_3008();

