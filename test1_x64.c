/*
 * $ cc -O3 -Wall -pedantic -Wextra test1_x64.c -o test1_x64 -pthread
 * $ ./test1_x64
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <x86intrin.h>
#include <unistd.h>
#include <sched.h>
#include <pthread.h>
#include <stdatomic.h>

#define CPU_ID_1 0
#define CPU_ID_2 1

#define N 100
#define STEP 8
#define START 0
#define END (START + STEP * N)

#define TEST_TIME 2

struct tsc_data
{
	atomic_uint_fast64_t tsc1;
	char gap[64];
	atomic_uint_fast64_t tsc2;

	uint64_t n1[N];
	uint64_t over1;
	int64_t tsc1diff_min, tsc1diff_max;

	uint64_t n2[N];
	uint64_t over2;
	int64_t tsc2diff_min, tsc2diff_max;
};

static void *
thread1(void *arg)
{
	struct tsc_data *data = arg;

	atomic_store_explicit(&data->tsc1, __rdtsc(), memory_order_relaxed);

	for (;;) {
		uint64_t old_tsc, new_tsc;

		old_tsc = atomic_load_explicit(&data->tsc2,
			memory_order_relaxed);

		do {
			new_tsc = atomic_load_explicit(&data->tsc2,
				memory_order_relaxed);
		} while (new_tsc == old_tsc);

		atomic_store_explicit(&data->tsc1, __rdtsc(),
			memory_order_relaxed);

		if (old_tsc != 0) {
			int64_t diff = new_tsc - old_tsc;

			if (diff > END) {
				data->over1++;
			} else {
				data->n1[(diff - START) / STEP]++;
			}

			if (diff < data->tsc1diff_min) {
				data->tsc1diff_min = diff;
			}
			if (diff > data->tsc1diff_max) {
				data->tsc1diff_max = diff;
			}
		}
	}

	return NULL;
}

static void *
thread2(void *arg)
{
	struct tsc_data *data = arg;

	atomic_store_explicit(&data->tsc2, __rdtsc(), memory_order_relaxed);

	for (;;) {
		uint64_t old_tsc, new_tsc;

		old_tsc = atomic_load_explicit(&data->tsc1,
			memory_order_relaxed);

		do {
			new_tsc = atomic_load_explicit(&data->tsc1,
				memory_order_relaxed);
		} while (new_tsc == old_tsc);

		atomic_store_explicit(&data->tsc2, __rdtsc(),
			memory_order_relaxed);

		if (old_tsc != 0) {
			int64_t diff = new_tsc - old_tsc;

			if (diff > END) {
				data->over2++;
			} else {
				data->n2[(diff - START) / STEP]++;
			}

			if (diff < data->tsc2diff_min) {
				data->tsc2diff_min = diff;
			}
			if (diff > data->tsc2diff_max) {
				data->tsc2diff_max = diff;
			}
		}
	}

	return NULL;
}


int
main()
{
	struct tsc_data data;
	pthread_t tid1, tid2;
	cpu_set_t cpuset;
	int i;
	int rc;

	memset(&data, 0, sizeof(struct tsc_data));

	data.tsc1diff_min = data.tsc2diff_min = INT64_MAX;
	data.tsc1diff_max = data.tsc2diff_max = INT64_MIN;

	pthread_create(&tid1, NULL, &thread1, &data);
	CPU_ZERO(&cpuset);
	CPU_SET(CPU_ID_1, &cpuset);
	rc = pthread_setaffinity_np(tid1, sizeof(cpu_set_t), &cpuset);
	if (rc != 0) {
		fprintf(stderr, "pthread_setaffinity_np() failed: %s\n",
			strerror(rc));
		return EXIT_FAILURE;
	}

	pthread_create(&tid2, NULL, &thread2, &data);
	CPU_ZERO(&cpuset);
	CPU_SET(CPU_ID_2, &cpuset);
	rc = pthread_setaffinity_np(tid2, sizeof(cpu_set_t), &cpuset);
	if (rc != 0) {
		fprintf(stderr, "pthread_setaffinity_np() failed: %s\n",
			strerror(rc));
		return EXIT_FAILURE;
	}

	sleep(TEST_TIME);

	pthread_cancel(tid1);
	pthread_cancel(tid2);

	for (i=0; i<N; i++) {
		printf("n1[%d] == %lu\n", i * STEP / 2, data.n1[i]);
	}
	printf("over1 == %lu\n", data.over1);
	printf("tsc1diff_min == %ld, tsc1diff_max == %ld\n",
		data.tsc1diff_min / 2, data.tsc1diff_max / 2);
	printf("\n");

	for (i=0; i<N; i++) {
		printf("n2[%d] == %lu\n", i * STEP / 2, data.n2[i]);
	}
	printf("tsc2diff_min == %ld, tsc2diff_max == %ld\n",
		data.tsc2diff_min / 2, data.tsc2diff_max / 2);
	printf("over2 == %lu\n", data.over2);

	return EXIT_SUCCESS;
}

