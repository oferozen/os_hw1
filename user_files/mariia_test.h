#ifndef M_TEST_H
#define M_TEST_H

#include <stdbool.h>
#include <stdio.h>


#define TEST(b) do { \
        if (!(b)) { \
                printf("\nFailed at %s:%d %s\n",__FILE__,__LINE__,#b); \
                return false; \
        } \
} while (0)
	
#define ERRNO(b) do { \
        if (errno != (b)) { \
                printf("\nFailed at %s:%d ERRNO(%s)\n",__FILE__,__LINE__,#b); \
                return false; \
        } \
} while (0)

#define SUCCESS(b) do { \
        if ((b) != 0) { \
                printf("\nFailed at %s:%d SUCCESS(%s)\n",__FILE__,__LINE__,#b); \
                return false; \
        } \
} while (0)
	
#define FAILURE(b) do { \
        if ((b) != -1) { \
                printf("\nFailed at %s:%d FAILURE(%s)\n",__FILE__,__LINE__,#b); \
                return false; \
        } \
} while (0)


#define RUN_TEST(name)  printf("Running %s... \n", #name); \
						if (!name()) { \
							printf("%s [FAILED]\n", #name);		\
							return -1;\
						}								\
						printf("%s [SUCCESS]\n", #name)



#endif
