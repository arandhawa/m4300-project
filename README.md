# MTH 4300 Portfolio Optimization Project

## Overview

The Goal of the project is to determine an optimal weighting of stocks in
a long-only portfolio, given some historical data for those stocks.

## Data Retrieval

`getstock.cc` downloads .csv files containing historical price data from
Quandl and saves them to the filesystem.

You will need a Quandl API key to do this. Place your API key in a file
named `apikey` in this directory.

the programs `getstock.cc` and `main.cc` are intended to be used together; the output of getstock
can be piped directly to main. They can also be used separately.

## Using

Compile both programs with:

```
$ make
```

use ```getstock -h and main -h``` to get help on using the programs

```
Usage: ./getstock [-h|--help] [-k FILE] [-b DATE] [-e DATE] [-o DIR] -- [TICKER...]
    -h,--help             show this help message
    -k                    file containing a Quandl api key (required)
    -b                    Beginning date, YYYY-mm-dd
    -e                    Ending date, YYYY-mm-dd
    -o                    Output directory. If this is omitted
                          default behavior is to print to stdout
    TICKER...             One or more stock symbols.

    All of the arguments are required
```

```
Usage: ./main [-h|--help] [-c <float>] [-t <float>] [-r <float>]
    -h,--help           show this help message
    -c float            initial capital
    -t float            transaction cost per trade
    -r float            Minimum portfolio mean return, in percentage form (decimal)

Default values
    -c 100000.0
    -t 0.00
    -r 10.00

Input Data
    From its standard input, the program reads:
        a starting date
        an ending date
        and a list of filenames
    The files must be in CSV format, with column labels

Example usage (using the getstock program to get the data)
    $ ./getstock -k apikey -b 2018-01-01 -e 2018-04-01 -o data -- JPM BAC GS | ./main -c 100000 -t 10.0 -r 0.07

```

## Notes

getstock will **output** 3 things: the start date, the end date, and a list of the filenames associated
with the queried stocks.

```
$ ./getstock -k apikey -b 2018-01-01 -e 2018-04-01 -o data -- JPM BAC GS
2018-01-01
2018-04-01
data/JPM.2018-01-01.2018-04-01.csv
data/BAC.2018-01-01.2018-04-01.csv
data/GS.2018-01-01.2018-04-01.csv
```

main **reads** 3 things: the start date, the end date, and a list of the filenames associated
with the stocks to use for the backtest/analysis.

These the input data can also be typed manually into main's standard input, or by some other program/script besides getstock.
