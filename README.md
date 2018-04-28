# MTH 4300 Portfolio Optimization Project

## Overview

getstock.cc downloads .csv files from
Quandl and saves them to a local filesystem database (or prints
them to stdout).

You will need a Quandl API key to use the program.

you can compile it with:

```
$ make
```

and run with

```
$ ./getstock [OPTIONS] [TICKERS]
```

use ```getstock -h``` to get help on using the program

```
Usage: ./getstock [-h|--help] [-k FILE] [-b DATE]
          [-e DATE] [-o DIR] -- [TICKER...]
    -h,--help             show this help message
    -k                    file containing a Quandl api key (required)
    -b                    Beginning date, YYYY-mm-dd
    -e                    Ending date, YYYY-mm-dd
    -o                    Output directory. If this is omitted
                          default behavior is to print to stdout
    TICKER...             One or more stock symbols.

Notes:
    Only the API key and the stock symbols are required arguments
```
