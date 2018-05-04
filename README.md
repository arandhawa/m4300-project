# MTH 4300 Portfolio Optimization Project

## Overview

getstock.cc downloads .csv files from
Quandl and saves them to a local filesystem database

You will need a Quandl API key to use getstock.

main.cc performs optimization and modeling

They are intended to be used together; the output of getstock
can be piped directly to main. They can also be used separately.

Compile both programs with:

```
$ make
```

and run with

use ```getstock -h and main -h``` to get help on using the programs

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

    All of the arguments are required
```
```
Usage: ./main [-h|--help] [-c $$$] [-t <ps|pt> $$$] [-m models...]
          [-v variance] [-r return]
    -h,--help           show this help message
    -c $$$              initial capital
    -t pt|ps $$$        transaction cost model. see below
    -m models           names of the models to use. See list below
    -v variance         Maximum portfolio variance, in percentage form (decimal)
    -r return           Minimum portfolio mean return, in percentage form (decimal)
See below for info on default values and input data

Transaction Costs (-t)
    'pt' means 'per trade' and 'ps' means 'per share'
    when specifying transaction costs with -t, the first argument should be
    one of these two abbreviations. The second argument should be the value to
    use for the transaction costs.
    ex)
        -t pt 10.0 == transaction costs of 10 dollars per trade
        -t ps 0.05 == transaction costs of 5 cents per share

Models (-m)
    The models currently implemented are:
        Markowitz Mean-Variance = meanvar
    example:
        -m meanvar
    Specifies that the program should do Mean-Variance optimization
    Multiple models can be specified

Variance (-v)
    The value shall be specified in decimal notation.
    For example, a variance of 8 percent should be specified as 0.08

Returns (-r)
    Just like variance, specify in decimal notation.

Default values
    If the command options are not specified, the following defaults will be assumed:
        Variance = 0.10
        Mean return = 0.05
        Transaction cost model = Per trade transaction costs
        Transaction costs = 10.00

Input Data
    From its standard input, the program reads:
        a starting date
        an ending date
        and a list of filenames
    The files must be in CSV format, with column labels

Example usage (using the getstock program to get the data)
    $ ./getstock -k apikey -b 2018-01-01 -e 2018-04-01 -o data -- JPM BAC GS | ./main -c 100000 -t pt 10.0 -m meanvar -v 0.04 -r 0.07
```

## Notes

getstock will output 3 things: the start date, the end date, and a list of the filenames associated
with the queried stocks.

```
$ ./getstock -k apikey -b 2018-01-01 -e 2018-04-01 -o data -- JPM BAC GS
2018-01-01
2018-04-01
data/JPM.2018-01-01.2018-04-01.csv
data/BAC.2018-01-01.2018-04-01.csv
data/GS.2018-01-01.2018-04-01.csv
```

main reads 3 things: the start date, the end date, and a list of the filenames associated
with the stocks to use for the backtest/analysis.

These can also be typed manually into the standard input, or by use of another program/script
besides getstock.
