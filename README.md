assault
-------

Summary
-------
A test suite to test socket delivery rates against web servers. 

Requirements
------------
- libcurl
- pthread

Usage
-----
Options for using `assault` are as follows:
<pre>
-c, --count <arg>      Run tests this many times.
-u, --url <arg>        Run against this URL.
-m, --memory           Do not use any local file storage ( WARNING: Large responses may cause problems... )
-d, --directory <arg>  Store the results here.
    --dry-run          Show what we would have done.

-m, --method <type>    Use <type> of method when making a request
                       (Available options are GET, POST, PATCH, PUT, DELETE, HEAD)
</pre>

