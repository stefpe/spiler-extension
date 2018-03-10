/*
+----------------------------------------------------------------------+
| SP⚡LER                                                              |
+----------------------------------------------------------------------+
| Copyright (c) 2017-2018 Stefan Pöltl                                 |
+----------------------------------------------------------------------+
| This source file is subject to version 1.0 of the Spiler license,    |
| that is bundled with this package in the file LICENSE, and is        |
| available as a file in this source.                                  |
|                                                                      |
| If you did not receive a copy of the Spiler license and are unable   |
| to obtain it through the world-wide-web, please send a note to       |
| stefan.poeltl@yahoo.de so we can mail you a copy immediately.        |
+----------------------------------------------------------------------+
| Authors:  Stefan Pöltl <stefan.poeltl@yahoo.de>                      |
+----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef PHP_SPILER_H
#define PHP_SPILER_H

#include "php.h"

extern zend_module_entry spiler_module_entry;
#define phpext_spiler_ptr &spiler_module_entry
//project constants
#define PHP_SPILER_VERSION "0.1.0"
#define MICRO_SEC_FACTOR 1000000.0
#define PROF_LIST_MEM_SIZE 1000

#ifdef PHP_WIN32
#	define PHP_SPILER_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_SPILER_API __attribute__ ((visibility("default")))
#else
#	define PHP_SPILER_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

//item with profiling information
typedef struct spiler_item{
    char *function_name;
    long int mem_usage;//bytes
    long int peak_mem_usage;//bytes
    uint32_t call_level;//at which level a function called was called
    uint32_t list_index;//key in list of profiled items
    double wall_start_time;//start time in micro seconds
    double wall_stop_time;//stop time in micro seconds
    double cpu_time;
    struct spiler_item *parent_prof_item;//pointer to parent profiled item
    char **params;//store string params if its a callback
    size_t cnt_param;//count of params stored
} spiler_item;


//### define module globals ###
ZEND_BEGIN_MODULE_GLOBALS(spiler)
	zend_bool profiler_enabled;//typedef unsigned char zend_bool;
	zend_bool ignore_internal_calls;//ignore tracking internal php function calls
	HashTable *filter_params;//filter which functions are storing params
	struct spiler_item *current_prof_item;//save current item for linking parent items
	size_t cnt_prof_items;//number of profiled items
	struct spiler_item **list_prof_items;//list of profiled items(array of pointers)
	double cpu_time;//cpu time
	double prof_start_time;//timestamp in microseconds when profiling starts
	double prof_stop_time;//timestamp in microseconds when profiling ends
	char *start_filename;//filname where the profiling starts
ZEND_END_MODULE_GLOBALS(spiler)

/* Always refer to the globals in your function as SPILER_G(variable).
   You are encouraged to rename these macros something shorter, see
   examples in any other php module directory.
*/
#define SPILER_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(spiler, v)

#if defined(ZTS) && defined(COMPILE_DL_SPILER)
ZEND_TSRMLS_CACHE_EXTERN()
#endif

//This will produce an allusion that will allow you to access the globals from every compile unit that includes php_spiler.h
ZEND_EXTERN_MODULE_GLOBALS(spiler)

#endif	/* PHP_SPILER_H */
