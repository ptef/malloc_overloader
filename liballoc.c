#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <stdint.h>
//#include <gnu/libc-version.h>

//__asm__(".symver malloc,malloc@@GLIBC_2.34");
//__asm__(".symver __libc_malloc,__libc_malloc@@GLIBC_2.4");

static void *(*orig_malloc)(size_t) = NULL;
static void *(*orig_calloc)(size_t nmemb, size_t size) = NULL;
static void *(*orig_realloc)(void *ptr, size_t size);
static void  (*orig_free)(void*) = NULL;
static void *(*orig_memalign)(size_t blocksize, size_t bytes);

void *memset(void*,int,size_t);
void *memmove(void *to, const void *from, size_t size);
ssize_t write(int fd, const void *buf, size_t count);

/*
 * since we don't have malloc() ready yet and some libc functions might
 * request heap allocations, we use a temporary buffer to return to those
 * callers.
 */
#define MAX_TMP_BUFFER 1024 * 1

char tmp_buffer[MAX_TMP_BUFFER];
unsigned short tmp_allocs = 0;
unsigned short tmp_pos = 0;

/*
 * I'm defining this struct here to store the memory addresses of allocations
 * requested throughout and given by malloc(). One can iteract this list later
 * to test these pointers.
 */
typedef struct {
    uintptr_t addr;
    size_t size;
    size_t is_fred;
} alloc_t;

#define MAX_ALLOCS 1024 * 20

static alloc_t allocs[MAX_ALLOCS];
static size_t alloc_idx = 0;

/*
 * Do not use constructor i.e.: void __attribute__ ((constructor)) init(void);
 * it can cause stack exhaustion (recursion to the end of the stack), and
 * therefore SIGSEGV. I'll be using primitive functions to avoid memory
 * allocations during initialization phase. Actually, you may use allocation
 * AFTER orig_malloc was correctly initiated.
 */
static void init_everything(void)
{
    if (!orig_malloc) {
		orig_malloc = dlsym(RTLD_NEXT, "malloc");
		if (!orig_malloc) {
			write(2, "LD_PRELOAD: quiting because of dlsym failure on malloc()\n", 57);
			exit(EXIT_FAILURE);
		}
		write(1, "LD_PRELOAD: orig_malloc() initiated\n", 36);
	}

    if (!orig_calloc) {
		orig_calloc = dlsym(RTLD_NEXT, "calloc");
		if (!orig_calloc) {
			write(2, "LD_PRELOAD: quiting because of dlsym failure on calloc()\n", 57);
			exit(EXIT_FAILURE);
		}
		write(1, "LD_PRELOAD: orig_calloc() initiated\n", 36);
	}

    if (!orig_realloc) {
		orig_realloc = dlsym(RTLD_NEXT, "realloc");
		if (!orig_realloc) {
			write(2, "LD_PRELOAD: quiting because of dlsym failure on realloc()\n", 58);
			exit(EXIT_FAILURE);
		}
		write(1, "LD_PRELOAD: orig_realloc() initiated\n", 37);
	}

	if (!orig_free) {
		orig_free = dlsym(RTLD_NEXT, "free");
		if (!orig_free) {
			write(2, "LD_PRELOAD: quiting because of dlsym failure on free()\n", 55);
			exit(EXIT_FAILURE);
		}
		write(1, "LD_PRELOAD: orig_free() initiated\n", 34);
	}

	if (!orig_memalign) {
		orig_memalign = dlsym(RTLD_NEXT, "memalign");
		if (!orig_memalign) {
			write(2, "LD_PRELOAD: quiting because of dlsym failure on memalign()\n", 59);
			exit(EXIT_FAILURE);
		}
		write(1, "LD_PRELOAD: orig_memalign() initiated\n", 38);
	}

	//fprintf(stdout, "LD_PRELOAD: init() completed!\n");
}


void* malloc(size_t s)
{
	/* 
	 * should set initialization_block = 1 everytime we have some operation 
	 * to be done before the actual call to orig_malloc(), i.e. managing 
	 * allocations, print messages, etc. and then initialization_block = 0
	 * to stop using our temporary memory block.
	 */
	static unsigned int initialization_block = 0;

	if (!orig_malloc) {
		initialization_block = 1;
		init_everything();
		initialization_block = 0;
	}

	if (initialization_block > 0) {
		/* Now, if memory is requested before full initialization (before
		 * init_everything() return), then need to return our temporary 
		 * memory block. Although we could return an original malloc 
		 * pointer by now, if there's a second call to malloc then we'll
		 * be deadlocked on an infinite recursive call and a stack 
		 * exhaustion will occour.
		 * So we must create a small memory allocator to deal with these
		 * initialization moment.
		 * Do not use any function that requires memory allocation on the
		 * code block below!
		 */
		if ((&tmp_buffer[0] + tmp_pos + s) < (&tmp_buffer[0] + MAX_TMP_BUFFER)) {
			write(1, &tmp_buffer[tmp_pos-8], 8);
			void *tmp_ptr = &tmp_buffer[tmp_pos];
			tmp_pos += s;
			tmp_allocs++;
			//write(1, "LD_PRELOAD: return1()\n", 17);
			return tmp_ptr;
		} else {
			write(2, "LD_PRELOAD: max temporary allocations reached, please increase\n", 58);
			exit(EXIT_FAILURE);
		}
	}

	void *ptr = orig_malloc(s);
	//initialization_block = 1;
	//fprintf(stdout, "LD_PRELOAD: malloc3()\n");
	//initialization_block = 0;

	if (ptr) {
        if (alloc_idx < MAX_ALLOCS) {
            allocs[alloc_idx].addr = (uintptr_t)ptr;
            allocs[alloc_idx].size = s;
            allocs[alloc_idx].is_fred = 0;
			//initialization_block = 1;
			//fprintf(stdout, "LD_PRELOAD: allocs[%ld].addr = %p\n", alloc_idx, (void *)allocs[alloc_idx].addr);
			//initialization_block = 0;
            alloc_idx++;
        } else {
			//initialization_block = 1;
            //fprintf(stderr, "LD_PRELOAD: allocation successfull, but stop tracking because (probably) reached MAX_ALLOCS\n");
			//initialization_block = 0;
        }
    }

	return ptr;
}


void *calloc(size_t nmemb, size_t size)
{
	//write(1, "LD_PRELOAD: calloc\n", 14);
    if (orig_malloc == NULL) {
        void *ptr = malloc(nmemb*size);
        if (ptr)
            memset(ptr, 0, nmemb*size);
        return ptr;
    }

    void *ptr = orig_calloc(nmemb, size);
    return ptr;
}


void *realloc(void *ptr, size_t size)
{
	if (orig_malloc == NULL) {
		void *nptr = malloc(size);
		//write(1, "LD_PRELOAD: realloc1\n", 16);
		if (nptr && ptr) {
			memmove(nptr, ptr, size);
			free(ptr);
		}
		return nptr;
	}

	void *nptr = orig_realloc(ptr, size);
	//write(1, "LD_PRELOAD: realloc2\n", 16);
	return nptr;
}


void *memalign(size_t blocksize, size_t bytes)
{
	//write(1, "LD_PRELOAD: memalign\n", 16);
	void *ptr = orig_memalign(blocksize, bytes);
	return ptr;
}


void free(void *ptr)
{
	if (!orig_free) {
		init_everything();
	}

	if ( tmp_allocs > 0 && (ptr >= (void *)&tmp_buffer[0] && ptr <= (void *)&tmp_buffer[tmp_pos]) ) {
		/* 
		 * By now, we don't actually free nothing here, but pretend to do so
		 * by decreasing tmp_allocs. Later on I'll implement struct alloc_t
		 * for the temporary memory, then it'll be possible to know the block
		 * size allocated and free things properly.
		 */
		//write(1, "LD_PRELOAD: XXXXXXXXXXXXXXX\n", 23);
		//printf("LD_PRELOAD:                         ptr = %p\n", ptr);
		//printf("LD_PRELOAD:              &tmp_buffer[0] = %p\n", &tmp_buffer[0]);
		//printf("LD_PRELOAD:        &tmp_buffer[tmp_pos] = %p\n", &tmp_buffer[tmp_pos]);
		//printf("LD_PRELOAD: &tmp_buffer[MAX_TMP_BUFFER] = %p\n", &tmp_buffer[MAX_TMP_BUFFER]);
		tmp_allocs--;

		return;
	}

	//fprintf(stdout, "LD_PRELOAD: free(ptr) = %p\n", ptr);
	for (size_t i=0; i<alloc_idx; i++) {
		if (ptr == (void *)allocs[i].addr) {
			/*
			 * what do I want to do here?
			 */
			if (allocs[i].is_fred >= 1) {
				/*
				 * We've found a possible double-free here.
				 * But this is not working due to realloc() - I need to fix
				 * this so it doesn't interpret realloc() as a new allocation.
				 */
				//fprintf(stdout, "LD_PRELOAD: double-free: allocs[%ld].addr = %p, is_fred = %ld, size = %ld\n", i, (void *)allocs[i].addr, allocs[i].is_fred, allocs[i].size);
			}
			allocs[i].is_fred++;
			break;
		}
	}

	return orig_free(ptr);
}
