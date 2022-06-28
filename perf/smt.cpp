// 来源：https://github.com/HongweiQin/smt_test/blob/master/src/smt_test.c

// i7 8700测试（6核12线程）
// 结论：在计算密集型中超线程确实有用

// 疑问：那为什么玩游戏会这么拉？负优化是怎么来的？
// 可能的原因：观察user，SMT下单个线程的效率其实是降低的，不充分使用多线程并发的应用可能就会变慢

/*
每次执行 time ./smt -f 20000000000 -g 4096
threads_num=12, fibonacci_max=20000000000, should_set_affinity=0, should_inline=1, alloc_granularity=4096

turn off smt:
real	0m54.958s
user	5m27.746s
sys 	0m0.036s

turn on smt:
real	0m33.432s
user	6m39.876s
sys 	0m0.012s
*/

// TODO 使用taskset限制核心数，对比真3核和伪6核的情况

#define DEFAULT_NR_THREAD (12)
#define DEFAULT_FIBONACCI_MAX (100)
#define DEFAULT_SHOULD_SET_AFFINITY (0)
#define DEFAULT_SHOULD_INLINE (1)
#define DEFAULT_ALLOC_GRANULARITY (0)

#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

int num_threads = DEFAULT_NR_THREAD;
unsigned long fibonacci_max = DEFAULT_FIBONACCI_MAX;
int should_set_affinity = DEFAULT_SHOULD_SET_AFFINITY;
int should_inline = DEFAULT_SHOULD_INLINE;
int alloc_granularity = DEFAULT_ALLOC_GRANULARITY;
int run;

#define barrier()	__asm__ __volatile__("":::"memory")

#define handle_error_en(en, msg) \
       do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

#define handle_error(msg) \
       do { perror(msg); exit(EXIT_FAILURE); } while (0)

struct thread_info {    /* Used as argument to thread_run() */
   pthread_t thread_id;        /* ID returned by pthread_create() */
   int thread_num;       /* Application-defined thread # */
   unsigned long res[2];
};

static void inline wait_to_run_inline()
{
	int myrun;

	do {
		barrier();
		myrun = run;
	}while(!myrun);
}

static void __attribute__((noinline)) wait_to_run()
{
	int myrun;

	do {
		barrier();
		myrun = run;
	}while(!myrun);
}

static void __attribute__((noinline)) calculate(int i,
						struct thread_info *tinfo)
{
	tinfo->res[i&1] = tinfo->res[0] + tinfo->res[1];
}

static void *
thread_run(void *arg)
{
	struct thread_info *tinfo = static_cast<thread_info*>(arg);
	unsigned long i;
	cpu_set_t cpuset;
	pthread_t thread;
	int ret;
	long number_of_processors;

	if (should_set_affinity) {
		thread = pthread_self();
		CPU_ZERO(&cpuset);

		number_of_processors = sysconf(_SC_NPROCESSORS_ONLN);
		if (number_of_processors <= 0) {
			printf("thread %d, nr_cpu=%ld, terminate thread\n",
				tinfo->thread_num, number_of_processors);
			return NULL;
		}
		CPU_SET(tinfo->thread_num % number_of_processors, &cpuset);
		//printf("thread %d, nr_cpu=%ld, affinity=%ld\n",
		//		tinfo->thread_num,
		//		number_of_processors,
		//		tinfo->thread_num % number_of_processors);
		ret = pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset);
		if (ret)
			printf("thread %d set affinity error, ret=%d\n",
					tinfo->thread_num, ret);
	}

	//printf("Thread %d\n",
	//   tinfo->thread_num);
	tinfo->res[0] = 1;
	tinfo->res[1] = 1;

	if (should_inline) {
		wait_to_run_inline();
		for (i = 2; i <= fibonacci_max; i++)
			tinfo->res[i&1] = tinfo->res[0] + tinfo->res[1];
	} else {
		wait_to_run();
		for (i = 2; i <= fibonacci_max; i++)
			calculate(i, tinfo);
	}

	return NULL;
}

int
main(int argc, char *argv[])
{
	int s, opt;
	pthread_attr_t attr;
	struct thread_info *tinfo;
	void *array;

	while ((opt = getopt(argc, argv, "ht:f:b:i:g:")) != -1) {
		switch (opt) {
		case 't':
			num_threads = atoi(optarg);
			break;
		case 'f':
			fibonacci_max = atol(optarg);
			break;
		case 'b':
			should_set_affinity = !!atoi(optarg);
			break;
		case 'i':
			should_inline = !!atoi(optarg);
			break;
		case 'g':
			alloc_granularity = atoi(optarg);
			break;
		case 'h':
		default:
			printf("Usage: ./smt_test [-t number_of_threads] [-f fibonacci_max] [-b should_set_cpu_affinity] [-i should_inline] [-g alloc_granularity]\n");
			return 0;
		}
	}
	if (num_threads <= 0) {
		printf("Wrong arg: num_threads=%d\n (needs to be greater than 0)\n", num_threads);
		return 0;
	}
	if (fibonacci_max <= 10) {
		printf("Wrong arg: fibonacci_max = %ld\n (needs to be greater than 10)\n", fibonacci_max);
		return 0;
	}
	if (alloc_granularity < sizeof(*tinfo))
		alloc_granularity = sizeof(*tinfo);
	printf("threads_num=%d, fibonacci_max=%ld, should_set_affinity=%d, should_inline=%d, alloc_granularity=%d\n",
		num_threads, fibonacci_max, should_set_affinity, should_inline, alloc_granularity);

	/* Initialize thread creation attributes. */
	s = pthread_attr_init(&attr);
	if (s != 0)
		handle_error_en(s, "pthread_attr_init");

	/* Allocate memory for pthread_create() arguments. */
	array = calloc(num_threads, alloc_granularity);
	if (array == NULL)
		handle_error("calloc");

	/* Create one thread for each command-line argument. */
	run = 0;
	barrier();
	for (int tnum = 0; tnum < num_threads; tnum++) {
		tinfo = (struct thread_info *)(array + tnum*alloc_granularity);
		tinfo->thread_num = tnum;

		s = pthread_create(&tinfo->thread_id, &attr,
			  &thread_run, tinfo);
		if (s != 0)
			handle_error_en(s, "pthread_create");
	}
	run = 1;
	barrier();

	/* Destroy the thread attributes object, since it is no
	longer needed. */
	s = pthread_attr_destroy(&attr);
	if (s != 0)
		handle_error_en(s, "pthread_attr_destroy");

	for (int tnum = 0; tnum < num_threads; tnum++) {
		tinfo = (struct thread_info *)(array + tnum*alloc_granularity);
		s = pthread_join(tinfo->thread_id, NULL);
		if (s != 0)
			handle_error_en(s, "pthread_join");

		//printf("Joined with thread %d\n",
		//	tinfo->thread_num);
	}

	free(array);
	return 0;
}
