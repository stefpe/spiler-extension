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

#ifndef SP_PROFILER
#define SP_PROFILER
#include "zend_smart_str.h"

spiler_item *spiler_item_new();
void spiler_item_free(spiler_item* item);
void spiler_free_global_memory();
double get_current_microtime();
double get_cpu_microtime();
spiler_item *start_function_profiling(zend_execute_data *data);
static spiler_item *build_profiler_item(char *function_name, zend_execute_data *data);
void stop_function_profiling(spiler_item *prof_item, zend_execute_data *data);
void set_filter_params(HashTable *ht);
void get_profiler_result_array(zval *result);
void get_profiler_result_json(zval *result);
char *get_sapi_name();
void append_float_to_smart_str(smart_str *buffer, const char* format, double fvalue);

#endif /* SP_PROFILER */
