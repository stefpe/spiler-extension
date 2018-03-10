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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_network.h"//socket support
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_spiler.h"
#include "spiler_profiler.h"

ZEND_DECLARE_MODULE_GLOBALS(spiler)
static PHP_GINIT_FUNCTION(spiler);
static PHP_GSHUTDOWN_FUNCTION(spiler);

//temporary function pointer to keep zend engine addresses
static void (*tmp_zend_execute_ex) (zend_execute_data *execute_data);
static void (*tmp_zend_execute_internal) (zend_execute_data *execute_data, zval *return_value);

/**
* Override for external function calls
*/
static void sp_execute_ex(zend_execute_data *execute_data) {
    spiler_item *prof_item = NULL;

    if(SPILER_G(profiler_enabled)){
        //@todo -> check invalid read of size 4 when whitelist is passed.
        prof_item = start_function_profiling(execute_data);
    }
    //exec php function
    tmp_zend_execute_ex(execute_data);

    if(SPILER_G(profiler_enabled)){
        stop_function_profiling(prof_item, execute_data);
    }
    return;
}

/**
* Override for internal php function calls
*/
static void sp_execute_internal(zend_execute_data *execute_data, zval *return_value) {

    spiler_item *prof_item = NULL;

    if(SPILER_G(profiler_enabled) && !SPILER_G(ignore_internal_calls)){
        prof_item = start_function_profiling(execute_data);
    }

    //exec internal php function
    if (!tmp_zend_execute_internal) {
        execute_internal(execute_data, return_value);
    }else{
        tmp_zend_execute_internal(execute_data, return_value);
    }

    if(SPILER_G(profiler_enabled) && !SPILER_G(ignore_internal_calls)){
        stop_function_profiling(prof_item, execute_data);
    }

    return;
}

/* True global resources - no need for thread safety here */
//static int le_spiler;

/* {{{ PHP_INI
 */
/* Remove comments and fill if you need to have entries in php.ini
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("spiler.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_spiler_globals, spiler_globals)
    STD_PHP_INI_ENTRY("spiler.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_spiler_globals, spiler_globals)
PHP_INI_END()
*/
/* }}} */


/**
* Returns true or false if enabling the profiler was successful
*/
PHP_FUNCTION(spiler_start)
{
    zend_bool ignore_internals = 1;//default ignore internal php functions

    //parse params, both optional
    ZEND_PARSE_PARAMETERS_START(0, 2)
        Z_PARAM_OPTIONAL
        Z_PARAM_BOOL(ignore_internals)
    ZEND_PARSE_PARAMETERS_END();

    //if already enabled stop
    if (SPILER_G(profiler_enabled)) {
        RETURN_FALSE;
    }

    //if spiler_start was already called, kill the previous memory.
    spiler_free_global_memory();

    //alloc filter params
    ALLOC_HASHTABLE(SPILER_G(filter_params));
    zend_hash_init(SPILER_G(filter_params), 64, NULL, NULL, 0);
    set_filter_params(SPILER_G(filter_params));

    //init global variables
    SPILER_G(ignore_internal_calls) = ignore_internals;
    SPILER_G(profiler_enabled) = 1;

    //set filename
    const char *filename = zend_get_executed_filename();//since php7.0
    SPILER_G(start_filename) = (char *)emalloc(strlen(filename) + 1);
    strcpy(SPILER_G(start_filename), filename);

    SPILER_G(prof_start_time) = get_current_microtime();
    SPILER_G(cpu_time) = get_cpu_microtime();

    RETURN_TRUE;
}

/**
* Returns true or false if disabling the profiler was successful
* disables only the global active state, nothing else.
*/
PHP_FUNCTION(spiler_stop)
{
    if (!SPILER_G(profiler_enabled)) {
        RETURN_FALSE;
    }

    SPILER_G(profiler_enabled) = 0;
    SPILER_G(prof_stop_time) = get_current_microtime();
    SPILER_G(cpu_time) = get_cpu_microtime() - SPILER_G(cpu_time);
    RETURN_TRUE;
}

/**
    Get profiler result as php array
*/
PHP_FUNCTION(spiler_get_result_array)
{
    if(SPILER_G(profiler_enabled) == 0 && SPILER_G(cnt_prof_items) > 0){
        get_profiler_result_array(return_value);
    }
}

/**
    Get profiler result directly as json result
*/
PHP_FUNCTION(spiler_get_result_json)
{
    if(SPILER_G(profiler_enabled) == 0 && SPILER_G(cnt_prof_items) > 0){
        get_profiler_result_json(return_value);
    }
}

/**
    Sends profiler data to the daemon
*/
PHP_FUNCTION(spiler_fire_result)
{
    zend_string *host;
    zend_long port;
    zend_long timeout_ms = 0;
    zval *profiler_result;
    struct timeval tv;
    php_socket_t sock;

	ZEND_PARSE_PARAMETERS_START(2, 3)
		Z_PARAM_STR(host)
		Z_PARAM_LONG(port)
		Z_PARAM_OPTIONAL
		Z_PARAM_LONG(timeout_ms)
	ZEND_PARSE_PARAMETERS_END();

    if(SPILER_G(profiler_enabled) == 1 || SPILER_G(cnt_prof_items) == 0){
        RETVAL_FALSE;
    }

    if(ZSTR_LEN(host) == 0){
        RETVAL_FALSE;
    }

    tv.tv_sec = 0;

    if(timeout_ms <= 0){
        timeout_ms = 10000;//10ms
    }else{
        timeout_ms *= 1000;//multiply ms to microseconds
    }
	tv.tv_usec = timeout_ms;//microseconds
    sock = php_network_connect_socket_to_host(ZSTR_VAL(host), port, SOCK_STREAM,
    			0, &tv, NULL, NULL, NULL, 0, STREAM_SOCKOP_NONE);

    if(sock == -1){
        closesocket(sock);
        RETVAL_FALSE;
    }

    get_profiler_result_json(profiler_result);
    int amount = send(sock, Z_STRVAL(*profiler_result), Z_STRLEN(*profiler_result), 0);
    closesocket(sock);
    zval_dtor(profiler_result);//decrement refcount if data is refcountable and free the memory if needed
    RETVAL_TRUE;
}

/* {{{ php_spiler_init_globals
 */
/* Uncomment this function if you have INI entries
static void php_spiler_init_globals(zend_spiler_globals *spiler_globals)
{
	spiler_globals->profiler_enabled = 0;
}*/

/* }}} */

/**
* Init globals
*/
static PHP_GINIT_FUNCTION(spiler)
{
    spiler_globals->profiler_enabled = 0;
    spiler_globals->ignore_internal_calls = 1;
    spiler_globals->filter_params = NULL;
    spiler_globals->current_prof_item = NULL;
    spiler_globals->cnt_prof_items = 0;
    spiler_globals->cpu_time = 0.0;//double
    spiler_globals->prof_start_time = 0.0;//double
    spiler_globals->prof_stop_time = 0.0;//double
    spiler_globals->start_filename = NULL;//check free
    spiler_globals->list_prof_items = NULL;//free memory
}

static PHP_GSHUTDOWN_FUNCTION(spiler)
{
    //last point where the whole process is killed.(killing webserver process)
}

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(spiler)
{
	/* If you have INI entries, uncomment these lines
	REGISTER_INI_ENTRIES();
	*/
    //hook into php execution stack
    tmp_zend_execute_ex = zend_execute_ex;
    zend_execute_ex = sp_execute_ex;
    tmp_zend_execute_internal = zend_execute_internal;
    zend_execute_internal = sp_execute_internal;
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(spiler)
{
	/* uncomment this line if you have INI entries
	UNREGISTER_INI_ENTRIES();
	*/
	//reset php execution stack
	zend_execute_ex = tmp_zend_execute_ex;
	zend_execute_internal = tmp_zend_execute_internal;
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(spiler)
{
#if defined(COMPILE_DL_SPILER) && defined(ZTS)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(spiler)
{
    //called in cli and web sapi
    //disable profiler, free memory
    SPILER_G(profiler_enabled) = 0;
    spiler_free_global_memory();
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(spiler)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "SPILER support", "enabled");
	php_info_print_table_row(2, "Version", PHP_SPILER_VERSION);
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}
/* }}} */

/* {{{ spiler_functions[]
 *
 * Every user visible function must have an entry in spiler_functions[].
 */
const zend_function_entry spiler_functions[] = {
	PHP_FE(spiler_start, NULL)
	PHP_FE(spiler_stop, NULL)
	PHP_FE(spiler_get_result_array, NULL)
	PHP_FE(spiler_get_result_json, NULL)
	PHP_FE(spiler_fire_result, NULL)
	PHP_FE_END	/* Must be the last line in spiler_functions[] */
};
/* }}} */

/* {{{ spiler_module_entry
 */
zend_module_entry spiler_module_entry = {
	STANDARD_MODULE_HEADER,
	"SPILER",
	spiler_functions,
	PHP_MINIT(spiler),
	PHP_MSHUTDOWN(spiler),
	PHP_RINIT(spiler),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(spiler),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(spiler),
	PHP_SPILER_VERSION,
  PHP_MODULE_GLOBALS(spiler),
  PHP_GINIT(spiler),
  PHP_GSHUTDOWN(spiler),
  NULL,
  STANDARD_MODULE_PROPERTIES_EX
};
/* }}} */

#ifdef COMPILE_DL_SPILER
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(spiler)
#endif
