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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "SAPI.h"
#include "php_spiler.h"
#include "spiler_profiler.h"

//include sys resource to get cpu usage
#ifdef PHP_WIN32
#include "win32/time.h"
#include "win32/getrusage.h"
#else
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

//macro to register special functions for profiling
#define add_filter_param(ht, fcn) zend_hash_str_add_empty_element(ht, fcn, strlen(fcn));

/*
* Function:  spiler_item_new
* --------------------------
* create new allocated spiler_item and return a pointer to it.
*
* returns: spiler_item *
*/
spiler_item *spiler_item_new(){
     spiler_item *item = (spiler_item *)emalloc(sizeof(spiler_item));
     item->function_name = NULL;
     item->mem_usage = 0;
     item->peak_mem_usage = 0;
     item->call_level = 0;
     item->list_index = 0;
     item->wall_start_time = 0.0;
     item->wall_stop_time = 0.0;
     item->cpu_time = 0.0;
     item->parent_prof_item = NULL;
     item->params = NULL;
     item->cnt_param = 0;
     return item;
}

/*
 * Function:  spiler_item_free
 * ---------------------------
 * remove the memory of an profiled item
 *
 * returns: void
 *
 */
void spiler_item_free(spiler_item* item){
    if(item == NULL){
        return;
    }

    if(item->function_name != NULL){
        efree(item->function_name);
    }

    if(item->params != NULL){//free string function params
        size_t i = 0;
        for(i = 0; i < item->cnt_param; i++){
            efree(item->params[i]);
        }
        efree(item->params);
    }

    efree(item);
}

/*
 * Function:  spiler_free_global_memory
 * -------------------------------------
 * cleans the profiled item list completely.
 *
 * returns: void
 *
 */
void spiler_free_global_memory()
{
    int i;

    if(SPILER_G(start_filename) != NULL){
        efree(SPILER_G(start_filename));
        SPILER_G(start_filename) = NULL;
    }

    if(SPILER_G(filter_params) != NULL){
        //zend_hash_destroy(SPILER_G(filter_params));
        //FREE_HASHTABLE(SPILER_G(filter_params));
        zend_array_destroy(SPILER_G(filter_params));
        SPILER_G(filter_params) = NULL;
    }

    if(SPILER_G(list_prof_items) == NULL){
        return;
    }

    //remove all allocated profiler items
    for(i=0; i<SPILER_G(cnt_prof_items); i++) {
        spiler_item *prof_item = NULL;
        prof_item = SPILER_G(list_prof_items)[i];
        spiler_item_free(prof_item);
    }

    efree(SPILER_G(list_prof_items));//remove allocated pointer list
    SPILER_G(list_prof_items) = NULL;
    SPILER_G(current_prof_item) = NULL;//item was also freed -> set back to null pointer otherwise next request crashes
    SPILER_G(cnt_prof_items) = 0;
}


/*
 * Function:  get_current_microtime
 * --------------------------------
 * get current time in microseconds.
 *
 * returns: double
*/
double get_current_microtime(){
    //#ifdef __APPLE__
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec * MICRO_SEC_FACTOR + tv.tv_usec;
}

/*
 * Function:  get_cpu_microtime
 * ----------------------------
 * get cpu time in microseconds.
 *
 * returns: double
*/
double get_cpu_microtime(){
    struct rusage ru;
    if(getrusage(RUSAGE_SELF, &ru) == 0){
        return (double)ru.ru_utime.tv_sec * MICRO_SEC_FACTOR + ru.ru_utime.tv_usec + (ru.ru_stime.tv_sec * MICRO_SEC_FACTOR + ru.ru_stime.tv_usec);
    }
    return 0.0;
}


/*
 * Function:  start_function_profiling
 * ------------------------------------
 * builds a string including the single function name or combined with classname:
 * e.g. test or Test::test
 *
 *  data: execution data of the zend engine
 *
 *  returns: pointer or NULL if the function is not in the white list
 *
 */
spiler_item *start_function_profiling(zend_execute_data *data){
    const char *class_name = NULL;
    char *converted_function_name = NULL;
    zend_class_entry *called_scope;
    zval *tmp;
    size_t final_length;


    zend_string *func_name = data->func->common.function_name;

    if (!data || !func_name) {
        return NULL;
    }

    //called_scope = zend_get_called_scope(data);//since php7.0
    called_scope = data->func->common.scope;//class name

    //build function name for further process...
    if (called_scope != NULL) {//check if class_name given
        class_name = ZSTR_VAL(called_scope->name);
        final_length = strlen(class_name) + 2 + ZSTR_LEN(func_name)+1;
        converted_function_name = (char *)emalloc(final_length);
        snprintf(converted_function_name, final_length, "%s::%s", class_name, ZSTR_VAL(func_name));
    }else{
        converted_function_name = (char *)emalloc(ZSTR_LEN(func_name) +1);
        strcpy(converted_function_name, ZSTR_VAL(func_name));
    }


    return build_profiler_item(converted_function_name, data);
}

/*
 * Function:  build_profiler_item
 * ------------------------------
 * builds a spiler_item with the necessary data
 *
 * function_name: execution data of the zend engine
 *
 * returns: pointer to an spiler item or null if no new item could be allocated.
 *
 */
static spiler_item *build_profiler_item(char *function_name, zend_execute_data *data){
    spiler_item *prof_item = NULL;

    prof_item = spiler_item_new();//alloc new spiler_item
    if(prof_item == NULL){
        efree(function_name);
        return NULL;
    }
    //adjust current item in global list to be the parent for the next item
    prof_item->parent_prof_item = SPILER_G(current_prof_item);
    SPILER_G(current_prof_item) = prof_item;

    if(prof_item->parent_prof_item == NULL){
        prof_item->call_level = 0;
    }else{
        prof_item->call_level = prof_item->parent_prof_item->call_level + 1;
    }

    //find key in filter params hashtable and store string params
    if(zend_hash_str_exists(SPILER_G(filter_params), function_name, strlen(function_name))){
        size_t i;
        size_t found_params = 0;
        for(i = 0; i <= ZEND_CALL_NUM_ARGS(data); i++){
          zval *param = ZEND_CALL_ARG(data, i+1);
          if(Z_TYPE_P(param) == IS_STRING){
              found_params++;
              prof_item->params = erealloc(prof_item->params, found_params * sizeof(char*));
              prof_item->params[found_params-1] = estrdup(Z_STRVAL_P(param));//memory needs to be freed.
          }
        }
        prof_item->cnt_param = found_params;
    }

    //adjust list size if no memory is left
    if(SPILER_G(cnt_prof_items)%PROF_LIST_MEM_SIZE == 0) {
        SPILER_G(list_prof_items) = erealloc(
            SPILER_G(list_prof_items),
            (SPILER_G(cnt_prof_items) + PROF_LIST_MEM_SIZE) * sizeof(spiler_item *)
        );
    }

    prof_item->function_name = function_name;
    prof_item->list_index = SPILER_G(cnt_prof_items);

    SPILER_G(list_prof_items)[SPILER_G(cnt_prof_items)] = prof_item;
    SPILER_G(cnt_prof_items)++;

    //memory and time stored right before hook call.
    prof_item->wall_start_time = get_current_microtime();
    prof_item->mem_usage = zend_memory_usage(0);
    prof_item->peak_mem_usage = zend_memory_peak_usage(0);
    return prof_item;
}

/*
 * Function:  stop_function_profiling
 * ----------------------------------
 *  finishes the profiling process for an spiler item
 *
 *  prof_item: pointer to the current profiled item
 *  data: zend engine data for current executable
 *
 *  returns: void
 *
 */
void stop_function_profiling(spiler_item *prof_item, zend_execute_data *data){
    if(prof_item == NULL){
        return;
    }

    //memory usage
    prof_item->mem_usage = zend_memory_usage(0) - prof_item->mem_usage;
    prof_item->peak_mem_usage = zend_memory_peak_usage(0) - prof_item->peak_mem_usage;
    //duration
    prof_item->wall_stop_time = get_current_microtime();

    //set parent back as currently profiled item
    SPILER_G(current_prof_item) = prof_item->parent_prof_item;
    return;
}

/*
 * Function:  set_filter_params
 * ----------------------------
 *  adds special functions that passed params are getting stored
 *
 *  ht: pointer to a HashTable to set the function names
 *
 *  returns: void
 *
 */
void set_filter_params(HashTable *ht){
    add_filter_param(ht, "Symfony\\Component\\EventDispatcher\\EventDispatcher::dispatch");
    add_filter_param(ht, "Symfony\\Component\\EventDispatcher\\ContainerAwareEventDispatcher::lazyLoad");
    add_filter_param(ht, "Symfony\\Bundle\\TwigBundle\\TwigEngine::render");
}

/*
 * Function:  get_profiler_result_array
 * ------------------------------------
 * Build final profiling result as php array.
 */
void get_profiler_result_array(zval *result){
    int i;
    zval items;
    char *sapi_name = get_sapi_name();
    array_init(result);
    array_init(&items);//allocates zend_array and HashTable memory

    for(i=0; i<SPILER_G(cnt_prof_items); i++) {

        zval prof_result;
        spiler_item *prof_item;

        prof_item = SPILER_G(list_prof_items)[i];
        if(prof_item == NULL){
            continue;
        }

        array_init(&prof_result);//dynamic size, because real size is depending from conditions below
        //add_ doesnt copy the values, so keep them allocated
        if(prof_item->function_name != NULL)
            add_assoc_string(&prof_result, "fc", prof_item->function_name);
        add_assoc_long(&prof_result, "mu", prof_item->mem_usage);
        add_assoc_long(&prof_result, "pmu", prof_item->peak_mem_usage);
        add_assoc_double(&prof_result, "st", prof_item->wall_start_time);
        add_assoc_double(&prof_result, "et", prof_item->wall_stop_time);
        add_assoc_long(&prof_result, "cl", prof_item->call_level);
        if(prof_item->parent_prof_item != NULL)
            add_assoc_long(&prof_result, "pi", prof_item->parent_prof_item->list_index);

        //add subarray to result
        add_next_index_zval(&items, &prof_result);
    }
    add_assoc_string(result, "sn", sapi_name);
    efree(sapi_name);
    add_assoc_double(result, "st", SPILER_G(prof_start_time));
    add_assoc_double(result, "et", SPILER_G(prof_stop_time));
    add_assoc_double(result, "ct", SPILER_G(cpu_time));
    add_assoc_long(result, "ci", SPILER_G(cnt_prof_items));
    if(SPILER_G(start_filename) != NULL)
        add_assoc_string(result, "fn", SPILER_G(start_filename));
    add_assoc_zval(result, "cs", &items);
}

/*
 * Function:  get_profiler_result_json
 * -----------------------------------
 *  builds json result by using a string buffer applied to the php zval result
 *
 *  result: pointer to the zval result container
 *
 *  returns: void
 *
 */
void get_profiler_result_json(zval *result){
    int i;
    char *sapi_name = get_sapi_name();
    smart_str buf = {0};
    smart_str_appends(&buf, "{");
    smart_str_appends(&buf, "\"sn\":\"");
    smart_str_appends(&buf, sapi_name);
    efree(sapi_name);
    append_float_to_smart_str(&buf, "\",\"st\":%.2f", SPILER_G(prof_start_time));
    append_float_to_smart_str(&buf, ",\"et\":%.2f", SPILER_G(prof_stop_time));
    append_float_to_smart_str(&buf, ",\"ct\":%.2f", SPILER_G(cpu_time));
    smart_str_appends(&buf, ",\"ci\":");
    smart_str_append_long(&buf, SPILER_G(cnt_prof_items));
    if(SPILER_G(start_filename) != NULL){
        smart_str_appends(&buf, ",\"fn\":\"");
        smart_str_appends(&buf, SPILER_G(start_filename));
        smart_str_appends(&buf, "\"");
    }

    smart_str_appends(&buf, ",\"cs\":[");

    for(i=0; i<SPILER_G(cnt_prof_items); i++) {
            spiler_item *prof_item;

            prof_item = SPILER_G(list_prof_items)[i];
            if(prof_item == NULL){
                continue;
            }

            smart_str_appends(&buf, "{\"fc\":\"");
            smart_str_appends(&buf, prof_item->function_name);
            smart_str_appends(&buf, "\",\"mu\":");
            smart_str_append_long(&buf, prof_item->mem_usage);
            smart_str_appends(&buf, ",\"pmu\":");
            smart_str_append_long(&buf, prof_item->peak_mem_usage);
            append_float_to_smart_str(&buf, ",\"st\":%.2f", prof_item->wall_start_time);
            append_float_to_smart_str(&buf, ",\"et\":%.2f", prof_item->wall_stop_time);
            smart_str_appends(&buf, ",\"cl\":");
            smart_str_append_long(&buf, prof_item->call_level);

            if(prof_item->parent_prof_item != NULL){
                smart_str_appends(&buf, ",\"pi\":");
                smart_str_append_long(&buf, prof_item->parent_prof_item->list_index);
            }

            if(prof_item->params != NULL){
                smart_str_appends(&buf, ",\"pa\":[");
                size_t j;
                for(j = 0; j < prof_item->cnt_param; j++){
                    smart_str_appends(&buf, "\"");
                    smart_str_appends(&buf, prof_item->params[j]);
                    if(j == prof_item->cnt_param -1){
                        smart_str_appends(&buf, "\"");
                    }else{
                        smart_str_appends(&buf, "\",");
                    }
                }
                smart_str_appends(&buf, "]");
            }

            smart_str_appends(&buf, "}");

            if(i < SPILER_G(cnt_prof_items) - 1){//add sep for next item
                smart_str_appends(&buf, ",");
            }
    }

    smart_str_appends(&buf, "]}");//close call_stack list

    //build zval and free allocated memory
    smart_str_0(&buf);//terminate string
    ZVAL_STRINGL(result, ZSTR_VAL(buf.s), ZSTR_LEN(buf.s));//zval will be allocated
    smart_str_free(&buf);
}

/*
 * Function:  get_sapi_name
 * -----------------------------------
 *  gets the short form of the sapi name from the sapi globals
 *
 *  result: pointer to a c string
 *
 *  returns: char *
 *
 */
char *get_sapi_name(){
    if (sapi_module.name) {
        return estrdup(sapi_module.name);
    }else{
        return estrdup("na");
    }
}


/*
 * Function:  helper to append float value to an zend smart_str
 * -----------------------------------
 *  builds json result by using a string buffer applied to the php zval result
 *
 *  result: pointer to the zval result container
 *
 *  returns: void
 *
 */
void append_float_to_smart_str(smart_str *buffer, const char* format, double fvalue){
    char *str;
    spprintf(&str, 0, format, fvalue);
    smart_str_appends(buffer, str);//copies whole memory of str
    efree(str);//free allocated memory of spprintf
}
