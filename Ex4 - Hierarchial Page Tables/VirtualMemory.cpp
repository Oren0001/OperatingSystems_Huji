#include "VirtualMemory.h"
#include "PhysicalMemory.h"

#define OFFSET_MASK (PAGE_SIZE - 1)


/*
 * Sets all the entries of the given frame to zeros.
 */
void clearFrame(uint64_t frame) {
    for (long long i = 0; i < PAGE_SIZE; ++i) {
        PMwrite(frame + i, 0);
    }
}

/*
 * Traverses the entire page tables tree in the physical memory using DFS.
 */
void traverseTables(uint64_t virtualAddr, word_t preFrame, uint64_t *data, word_t *newFrame,
                    word_t *maxFrameIndex, int depth = 0, word_t curFrameVal = 0,
                    uint64_t preAddr = 0, uint64_t curPage = 0) {
    if (depth == TABLES_DEPTH) {
        uint64_t dist = (virtualAddr >> OFFSET_WIDTH) - curPage;
        uint64_t absDist = (dist > 0) ? dist : -dist;
        uint64_t a = NUM_PAGES - absDist;
        uint64_t cyclicDist = (absDist < a) ? absDist : a;
        if (cyclicDist > data[0]) {
            data[0] = cyclicDist;
            data[1] = preAddr;
            data[2] = curFrameVal;
            data[3] = curPage;
        }
    } else {
        if ((curFrameVal != preFrame) && (depth > 0)) {
            word_t val;
            for (int i = 0; i < PAGE_SIZE; ++i) {
                PMread(curFrameVal * PAGE_SIZE + i, &val);
                if (val != 0) {
                    break;
                }
                if (i + 1 == PAGE_SIZE) {
                    *newFrame = curFrameVal;
                    data[4] = preAddr;
                    return;
                }
            }
        }
        for (int i = 0; i < PAGE_SIZE; ++i) {
            uint64_t nextFrameAddr = curFrameVal * PAGE_SIZE + i;
            word_t nextFrameVal;
            PMread(nextFrameAddr, &nextFrameVal);
            if (nextFrameVal != 0) {
                uint64_t nextPage = curPage * PAGE_SIZE + i;
                ++(*maxFrameIndex);
                traverseTables(virtualAddr, preFrame, data, newFrame, maxFrameIndex,
                               depth + 1, nextFrameVal, nextFrameAddr,
                               nextPage);
            }
        }
    }
}

/*
 * We call this function after encountering a page fault, in order to find the exact
 * frame to put our page in and evicting another page if necessary.
 */
void fetchNewFrame(uint64_t virtualAddr, word_t curFrame, word_t *newFrame) {
    uint64_t data[5] = {0};
    word_t maxFrameIndex = 0;
    traverseTables(virtualAddr, curFrame, data, newFrame, &maxFrameIndex);
    if (*newFrame != 0) {
        PMwrite(data[4], 0);
    } else if (maxFrameIndex + 1 < NUM_FRAMES) {
        *newFrame = maxFrameIndex + 1;
    } else {
        PMwrite(data[1], 0);
        PMevict(data[2], data[3]);
        *newFrame = data[2];
    }
}

/*
 * Translates the given virtual address to it's corresponding frame.
 */
word_t getFrame(uint64_t virtualAddr) {
    word_t curFrameVal = 0, nextFrameVal;
    uint64_t index, nextFrameAddr, tablesWidth;
    for (int depth = 0; depth < TABLES_DEPTH; ++depth) {
        tablesWidth = OFFSET_WIDTH * (TABLES_DEPTH - depth);
        index = (virtualAddr >> tablesWidth) & OFFSET_MASK;
        nextFrameAddr = curFrameVal * PAGE_SIZE + index;
        PMread(nextFrameAddr, &nextFrameVal);

        if (nextFrameVal == 0) { // handle page fault
            nextFrameVal = 0;
            fetchNewFrame(virtualAddr, curFrameVal, &nextFrameVal);
            PMwrite(nextFrameAddr, nextFrameVal);
            if (depth < TABLES_DEPTH - 1) {
                clearFrame(nextFrameVal * PAGE_SIZE);
            } else {
                PMrestore(nextFrameVal, virtualAddr >> OFFSET_WIDTH);
            }
        }
        curFrameVal = nextFrameVal;
    }
    return curFrameVal;
}


/**
 * Initialize the virtual memory.
 */
void VMinitialize() {
    clearFrame(0);
}

/**
 * Reads a word from the given virtual address
 * and puts its content in *value.
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical address for any reason)
 */
int VMread(uint64_t virtualAddress, word_t *value) {
    if (virtualAddress >= VIRTUAL_MEMORY_SIZE || value == nullptr) {
        return 0;
    }
    word_t frame = getFrame(virtualAddress);
    uint64_t offset = virtualAddress & OFFSET_MASK;
    PMread(frame * PAGE_SIZE + offset, value);
    return 1;
}

/**
 * Writes a word to the given virtual address.
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical address for any reason)
 */
int VMwrite(uint64_t virtualAddress, word_t value) {
    if (virtualAddress >= VIRTUAL_MEMORY_SIZE) {
        return 0;
    }
    word_t frame = getFrame(virtualAddress);
    uint64_t offset = virtualAddress & OFFSET_MASK;
    PMwrite(frame * PAGE_SIZE + offset, value);
    return 1;
}
