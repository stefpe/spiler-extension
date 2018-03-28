--TEST--
Simple profiling test
--FILE--
<?php
spiler_start();
test();
spiler_stop();
$result = spiler_get_result_array();
var_dump(count($result['cs']));
var_dump($result['ci']);
var_dump($result['sn']);
var_dump(gettype($result['st']));
var_dump(gettype($result['et']));
var_dump(gettype($result['fn']));
var_dump($result['cs'][0]['fc']);
var_dump($result['cs'][0]['cl']);
function test()
{
  $a = range(0, 100);
}
?>
--EXPECT--
int(1)
int(1)
string(3) "cli"
string(6) "double"
string(6) "double"
string(6) "string"
string(4) "test"
int(0)
