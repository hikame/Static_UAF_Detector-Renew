#ifndef _CONFIG_H
#define _CONFIG_H


#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>

//
// Configurations for compilation.
//
//#define VERBOSE_SA 1
//#define DEBUG_SA 1
#define SOUND_MODE 1

#define VERBOBE 1

#define FUNC_MAX_ANALYZED_TIME 6

static std::string HeapAllocFN[] = {
	"__kmalloc",
	"__vmalloc",
	"vmalloc",
	"krealloc",
	"devm_kzalloc",
	"vzalloc",
	"malloc",
	"kmem_cache_alloc",
	"__alloc_skb",
};

// static std::string ManualInitedGlobal[] = {
// 		"futex_queues"
// };

#endif
