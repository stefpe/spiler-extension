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
    
## Requirements
- Mac OSX or Linux
- PHP >= 7.0

    
