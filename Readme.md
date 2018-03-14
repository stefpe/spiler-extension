# SP⚡LER PROFILER EXTENSION

## Build the extension
    phpize
    ./configure
    make
    sudo make install

## Load in php.ini
    extension=spiler.so

## Load it temporary
    php -dextension=modules/spiler.so test.php

## Profile a symfony application
#### Wrap your FrontController(app.php) like this:
    spiler_start();
    $request = Request::createFromGlobals();
    $response = $kernel->handle($request);
    $response->send();
    $kernel->terminate($request, $response);
    spiler_stop();
    spiler_fire_result("localhost", 9001);

## Profiled data
### Global data
    sn: sapi(server api) name
    st: start profiler time -> microseconds
    et: end profiler time -> microseconds
    ct: cpu time -> microseconds
    ci: count of profiled function calls -> int
    fn: filename where the profiler started
    cs: the call stack
### Call stack
    fc: called function name
    mu: memory usage -> bytes
    pmu: peak memory usage -> bytes
    st: start time -> microseconds
    et: end time -> microseconds
    cl: call level -> int
## Requirements
- Mac OSX or Linux | Windows support is added but isnt tested
- PHP >= 7.0 | Tested with PHP 7.0, 7.1, 7.2

## Todos
    - fix segfault for sending data to the tcp daemon
