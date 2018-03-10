#PHP internals

##using smart_str api
typedef struct {
	zend_string *s;
	size_t a;
} smart_str;

### get c string from zend_string(value)
char *c_str = ZSTR_VAL(zend_string *s);
### get length from zend_string
size_t len = ZSTR_LEN(zend_string *s);

##zval

### get char * from zval ptr
Z_STRVAL_P()

## HashTable

### copy a HashTable
zend_array_dup(ht);

### HashTable as basic array param
Z_PARAM_ARRAY_HT
