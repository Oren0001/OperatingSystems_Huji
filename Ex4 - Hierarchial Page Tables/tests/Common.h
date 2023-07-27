#pragma once

#include "MemoryConstants.h"
#include "PhysicalMemory.h"
#include "VirtualMemory.h"

#ifdef USE_SPEEDLOG
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#endif

#include <gtest/gtest.h>
#include <cstdio>
#include <cassert>
#include <map>
#include <random>
#include <regex>
#include <string>
#include <unordered_map>

/** This function is mostly for my convenience when debugging,
 *  you can use it if you have some way to enable/disable print statements at runtime.
 * @param doLog Should logging be enabled or disabled?
 */
void setLogging(bool doLog)
{
	(void)doLog;
#ifdef USE_SPEEDLOG
    if (!doLog) {
        spdlog::set_level(spdlog::level::off);
    } else {
        spdlog::set_level(spdlog::level::info);
    }
#endif
}

/** Maps between expected physical addresses to their values in memory. */
using PhysicalAddressToValueMap = std::unordered_map<uint64_t, word_t>;

/**
 * Reads all physical memory addresses in memory who are present as keys in 'map'
 * @param map Used for its keys, indicates which keys the output map will contain.
 * @return Maps read physical memory addresses to their actual values in RAM
 */
PhysicalAddressToValueMap readGottenPhysicalAddressToValueMap(const PhysicalAddressToValueMap& map)
{
    PhysicalAddressToValueMap gotten;
    for (const auto& kvp: map)
    {
        word_t gottenValue;
        PMread(kvp.first, &gottenValue);
        gotten[kvp.first] = gottenValue;
    }
    return gotten;
}

::testing::AssertionResult LinesContainedInTrace(Trace& trace, std::initializer_list<std::string> lines)
{
    std::string traceOut = trace.GetContents();
    std::stringstream regexMaker;
    int i=0;
    for (const auto& line: lines)
    {
        auto nextPos = traceOut.find(line);
        if (nextPos == std::string::npos) {
            return ::testing::AssertionFailure()
                << "Expected to encounter \"" << line << "\" after checking " << i << " elements, didn't find it.";
        }
        ++i;
        traceOut = traceOut.substr(nextPos);
    }
    return ::testing::AssertionSuccess();
}

// by default, use the same seed so that test results will be consistent with several runs.
const bool USE_DETERMINED_SEED = true;

/** Returns a random engine using either a known seed, or a seed determined at startup
 * @param useDeterminedSeed Use known seed?
 * @return Engine
 */
std::default_random_engine getRandomEngine(bool useDeterminedSeed = USE_DETERMINED_SEED)
{
   if (useDeterminedSeed)
   {
       std::seed_seq seed {
           static_cast<long unsigned int>(1337),
       };
       std::default_random_engine eng {seed};
       return eng;
   } else
   {
       std::random_device rd;
       std::seed_seq seed {
            rd()
       };
       std::default_random_engine eng {seed};
       return eng;
   }
}
typedef std::vector<word_t> page_t;

extern std::vector<page_t> RAM;
extern std::unordered_map<uint64_t, page_t> swapFile;


/** This is an interesting value: note that no page table can have NUM_FRAMES in its content,
 *  but actual pages(last layer in the hierarchy) can. Of course, when initializing RAM,
 *  there's no such thing as invalid values.
 */
const word_t SPECIFIC_FILL_VALUE = static_cast<word_t>(NUM_FRAMES);

/** How to initialize RAM memory at start of a test */
enum class InitializationMethod
{
    /** all values in RAM will be initialized to 0 */
    ZeroMemory = 0,

    /** All values outside the root table will have "SPECIFIC_FILL_VALUE" */
    FillWithSpecificValue = 1,


    /** All values outside the root table will have random values */
    RandomizeValues = 2
};

/** Initializes RAM according to given criteria, then calls VMinitialize
 *  A correct implementation should work with any initialization method.
 **/
void fullyInitialize(InitializationMethod option) {
    RAM.clear();

    RAM.resize(NUM_FRAMES, page_t(PAGE_SIZE));

    auto randomEngine = getRandomEngine();
    std::uniform_int_distribution<word_t> dist(0, std::numeric_limits<word_t>::max());

    for (page_t& page: RAM)
    {
        for (word_t& word: page)
        {
           if (option == InitializationMethod::ZeroMemory)
           {
               word = 0;
           } else if (option == InitializationMethod::FillWithSpecificValue)
           {
               word = SPECIFIC_FILL_VALUE;
           } else
           {
               word = dist(randomEngine);
           }
        }
    }

    // this should zero the root page table
    VMinitialize();
    for (uint64_t i=0; i < PAGE_SIZE; ++i)
    {
        word_t readVal;
        PMread(i, &readVal);
        // there should be no reason for this test to fail since VMinitialize
        // is already implemented for you - this is mostly a sanity check
        ASSERT_EQ(readVal, 0) << "Root page table should be zero after initializing";
    }
}
