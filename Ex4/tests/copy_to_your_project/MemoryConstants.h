#pragma once

#include <climits>
#include <stdint.h>



#ifdef TEST_CONSTANTS

#define OFFSET_WIDTH 1
#define PHYSICAL_ADDRESS_WIDTH 4
#define VIRTUAL_ADDRESS_WIDTH 5

#elif NORMAL_CONSTANTS

#define OFFSET_WIDTH 4
#define PHYSICAL_ADDRESS_WIDTH 10
#define VIRTUAL_ADDRESS_WIDTH 20

#elif OFFSET_DIFFERENT_FROM_INDEX

#define OFFSET_WIDTH 2
#define PHYSICAL_ADDRESS_WIDTH 5
#define VIRTUAL_ADDRESS_WIDTH 7


#elif SINGLE_TABLE_CONSTANTS

#define OFFSET_WIDTH 5
#define PHYSICAL_ADDRESS_WIDTH 6
#define VIRTUAL_ADDRESS_WIDTH 10

#elif UNREACHABLE_FRAMES_CONSTANTS

#define OFFSET_WIDTH 3
#define PHYSICAL_ADDRESS_WIDTH 9
#define VIRTUAL_ADDRESS_WIDTH 6

#elif NO_EVICTION_CONSTANTS

#define OFFSET_WIDTH 5
#define PHYSICAL_ADDRESS_WIDTH 5
#define VIRTUAL_ADDRESS_WIDTH 5

#else

#error "You didn't define which constants to use"

#endif


/* ----------- common to all constant configurations -------------- */

// word
typedef int word_t;

// number of bits in a word
#define WORD_WIDTH (sizeof(word_t) * CHAR_BIT)

// page/frame size in words
// in this implementation this is also the number of entries in a table
#define PAGE_SIZE (1LL << OFFSET_WIDTH)



// RAM size in words
#define RAM_SIZE (1LL << PHYSICAL_ADDRESS_WIDTH)


// virtual memory size in words
#define VIRTUAL_MEMORY_SIZE (1LL << VIRTUAL_ADDRESS_WIDTH)

// number of frames in the RAM
#define NUM_FRAMES (RAM_SIZE / PAGE_SIZE)

// number of pages in the virtual memory
#define NUM_PAGES (VIRTUAL_MEMORY_SIZE / PAGE_SIZE)

#define CEIL(VARIABLE) ( (VARIABLE - (int)VARIABLE)==0 ? (int)VARIABLE : (int)VARIABLE+1 )
#define TABLES_DEPTH CEIL((((VIRTUAL_ADDRESS_WIDTH - OFFSET_WIDTH) / (double)OFFSET_WIDTH)))
