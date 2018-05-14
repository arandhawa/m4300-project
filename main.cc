#ifndef _GNU_SOURCE
  #define _GNU_SOURCE
#endif
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include <map>
#include <vector>
#include <string>
#include <iostream>
#include <algorithm>/* std::stable_partition */
#include <utility>  /* std::move */
#include <random>   /* std::uniform_real_distribution */
#include <mutex>    /* for threadsafe printf */

#include <Eigen/Core>

using namespace Eigen;

/* DATE_FMT is a format string used to parse a date of the form YYYY-mm-dd
 * DATE_KEY is the column name of the 'date' field. used to get the index of the field
 * DATA_SEP is the separator in the CSV file
 */
#define DATE_FMT "%Y-%m-%d"
#define DATE_KEY "Date"
#define DATA_SEP ','

/* Default values when user input is omitted.
 */
#define DEFAULT_INITIAL_CAPITAL 100000.0
#define DEFAULT_MIN_RETURN 0.002
#define DEFAULT_TCOST_MODEL TCostPerTrade
#define DEFAULT_TCOST_MODEL_NAME "Per trade transaction costs"
#define DEFAULT_TCOST 10.0
#define SECONDS_IN_DAY 86400

#define MAX(x, y) ((x) > (y)) ? (x) : (y)
#define MIN(x, y) ((x) < (y)) ? (x) : (y)

static std::string upper(char const *s);
static std::string ticker_from_filename(char const *filename);
static std::map<std::string, std::vector<double> > read_stock_data(std::vector<std::string> const & filepaths,
                                                                   time_t start, time_t end);
static time_t strtotime(char const *s);  /* convert date time string to an integral representation (time_t) */
static void timetostr(time_t t, char *s);
static time_t read_until(FILE *file, time_t begin, int date_index);
/*^read from data source until we hit a certain point in time (begin) */
static int indexOf(char const *line, char const *field);
static VectorXd weeklyReturns(std::vector<double> const & prices);
static MatrixXd cov(MatrixXd const & m);
static void die(char const *fmt, ...);   /* print a message and kill the program */
static void warn(char const *fmt, ...);  /* print a message */
static void usage(char const * argv0);

template <typename Iter, typename Container>
typename Container::iterator index_remove(Iter ixbegin, Iter ixend, Container & C)
{
	int ix = 0;
	return std::stable_partition(C.begin(),C.end(),[&](typename Container::value_type const & unused) {
		return std::find(ixbegin,ixend,ix++) == ixend;
	});
}

/*
 * upper case a string
 */
std::string upper(char const *s)
{
	int size = (int) strlen(s);
	std::string ret;
	ret.resize(size);
	for (int i = 0; i < size; i++) {
		ret[i] = toupper(s[i]);
	}
	return ret;
}
/*
 * Given a filename of the form
 * TICKER.begin.end.csv
 * return TICKER
 */
std::string ticker_from_filename(char const *filename)
{
	char buf[256];
	char *pd;

	strcpy(buf, filename);
	pd = strchrnul(buf, '.');
	*pd = '\0';
	return upper(buf);
}
/*
 * read_stock_data
 *   return a map of ticker -> prices
 */
std::map<std::string, std::vector<double> >
read_stock_data(std::vector<std::string> & filepaths, time_t start, time_t end)
{
	/* it could be the case that the dates in the file do not match up.
	 * We synchronize the dates by first getting the latest available starting
	 * date, and reading all other files until we reach that point (or EOF).
	 * So when we read from the files and save data in some arrays, the dates will
	 * all be sync'd up by index (assuming there are no missing rows in the data)
	 */
	std::map<std::string, std::vector<double> > data;
	std::vector<double> prices;         /* temp variable */
	char buf[256];
	char *p; /* position in a line of the CSV file */
	std::vector<int> ixrm;  /* for index_remove - indices of any data sources to remove because they don't have correct data */

	for (int i = 0; i < (int) filepaths.size(); i++) {
		char const *f = filepaths[i].c_str();
		FILE *file = fopen(f, "r");
		if (!file) {
			perror("fopen:");
			die("Failed to open file %s aborting\n", f);
		}
		auto ticker = ticker_from_filename(f);
		/* get index of date, and Adj. Close */
		if (!fgets(buf, sizeof buf, file)) {
			warn("File %s is empty\n",f);
			ixrm.push_back(i);
			fclose(file);
			continue;
		}
		int close_index = indexOf(buf, "Adj. Close");
		if (close_index == -1 && (close_index == indexOf(buf, "Close")) == -1) {
			warn("Could not find closing price data for: %s\n", ticker.c_str());
			ixrm.push_back(i);
			fclose(file);
			continue;
		}
		int date_index = indexOf(buf, "date");
		if (date_index == -1) {
			warn("Could not find date field for: %s\n", ticker.c_str());
			ixrm.push_back(i);
			fclose(file);
			continue;
		}
		if (!read_until(file, start, date_index)) {
			/* read_until == 0, so no date >= start was found */
			warn("Data has no observations >= start date: %s\n", f);
			ixrm.push_back(i);
			fclose(file);
			continue;
		}
                while (fgets(buf, sizeof buf, file)) {
			/* check if date is past the end */
			p = buf;
			for (int i = 0; i < date_index; i++) {
				p = strchr(p, DATA_SEP);
				p++;
			}
			if (strtotime(p) > end)
				break;
			/* OK, read the price data */
			p = buf;
			for (int i = 0; i < close_index; i++) {
				p = strchr(p, DATA_SEP);
				p++;
			}
			char *endptr;
			double price = strtod(p, &endptr);
			if (price == 0.0 && endptr == p) { /* a parse error ocurred */
				perror("Parsing Adj. Close");
				die("Aborting\n");
			}
			prices.push_back(price);
		}
		data[ticker] = prices;
		prices.clear();
		fclose(file);
	}
	filepaths.erase(index_remove(ixrm.begin(),ixrm.end(), filepaths), filepaths.end());

	/*
	 * make sure that data have same dimensions
	 */
	int max_observations = 0;
	for (auto const & pair : data) {
		max_observations = MAX(max_observations, pair.second.size());
	}
	max_observations = max_observations - 2; /* add some slack */
	for (auto it = data.begin(); it != data.end(); ) {
		if (it->second.size() < max_observations) {
			warn("Not enough observations for %s: has %d of %d required\n", it->first.c_str(), it->second.size(), max_observations);
			data.erase(it++);
		} else {
			it->second.resize(max_observations);
			it++;
		}
	}
	return data;
}
/*
 * Given a string of the form
 * YYYY-mm-dd
 * parse it and save it as an integral type (time_t)
 * FYI: time_t is usually defined as an unsigned integer
 *      it represents the number of seconds elapsed since the Unix epoch (1970-01-01)
 */
time_t strtotime(char const *s)
{
	struct tm tm;
	memset(&tm, 0, sizeof tm);
	if (!strptime(s, DATE_FMT, &tm))
		return 0;
	return mktime(&tm);
}
void timetostr(time_t t, char *s)
{
	struct tm *tm;
	tm = gmtime(&t);
	strftime(s,64,"%Y-%m-%d",tm);
}
/*
 * read_until
 *   @file  := FILE stream
 *   @begin := the date we want to synchronize with
 * returns:
 *   the earliest time observation that is >= begin, in Unix time.
 *   or 0 (1970-01-01 00:00:00) if there is no date >= begin
 */
time_t read_until(FILE *file, time_t begin, int date_index)
{
	char buf[256];
	char *date;
	int nread;
	struct tm tm;
	time_t tmp;

	char bg[64];
	char ed[64];
	
	timetostr(begin, bg);

	/* iterate over dates in file until date >= begin */
	while (fgets(buf, sizeof buf, file)) {
		nread = strlen(buf);   /* remember how much to rewind by */
		date = buf;
		for (int i = 0; i < date_index; i++) {
			date = strchr(date, DATA_SEP);
			if (!date) {
				return 0;
				die("Date field not found in data\n");
			}
			date++;
		}
		memset(&tm, 0, sizeof tm);
		if (!strptime(date, DATE_FMT, &tm)) {
			return 0;
			die("Failed to parse date in file\n");
		}
		tmp = mktime(&tm);
		timetostr(tmp,ed);
		if (tmp >= begin) {
			/* the time for this line in the file is >= the specified start
			 * date, rewind so the caller can re-read this line in the future
			 */
			fseek(file, SEEK_CUR, -nread);
			return tmp;
		}
	}
	/* we reached EOF without finding a date >= begin
	 * let caller know by returning 0 */
	return 0;
}
/*
 * Find the index of a field in a comma-sperated line of text
 * ex)
 *      line = "Date,Open,High,Low,Close"
 *      indexOf(line, "Low") = 3
 */
int indexOf(char const *line, char const *field)
{
	int index;
	char const *begin, *end, *lineEnd;

	begin = line;
	end = strchr(begin, DATA_SEP);
	if (end == NULL) {
		if (strcmp(begin, field) == 0)
			return 0;
		return -1;
		die("Field (%s) not found in string: %s\n", field, line);
	}
	lineEnd = begin + strlen(begin);
	index = 0;
	while (begin < lineEnd) {
		int size = end - begin;
		if (strncasecmp(begin, field, size) == 0) {
			break;
		}
		begin = end + 1;
		end = strchrnul(begin, DATA_SEP);
		index++;
	}
	int nsep = 0;
	for (char const *tmp = line; *tmp != '\0'; tmp++) {
		if (*tmp == DATA_SEP)
			nsep++;
	}
	if (index > nsep) {
		return -1;
		die("Field not found in data: %s\n", field);
	}
	return index;
}
/*
 * Given a vector of prices for a given security
 * return the vector containing the weekly returns for that security
 * We compute weekly returns as:
 *    weeklyReturns = (p[i] - p[i-4]) / p[i-4],  4 <= i < n;
 * which is the change over a 5 day period
 * ex: let i = 4, at index 4 we are at the 5th day
 *                at index i - 4 = 0 we are at the first day
 * so we can imagine this is the change from monday's closing price
 * to friday's closing price.
 */
VectorXd weeklyReturns(std::vector<double> const & prices)
{
	VectorXd returns;
	int i, n;

	n = (int) prices.size();
	returns.resize(n / 5);
	for (i = 0; i < returns.size(); i++) {
		returns(i) = (prices[i+4] - prices[i]) / prices[i];
	}
	return returns;
}
MatrixXd cov(MatrixXd const & m)
{
	/* please see https://stats.stackexchange.com/a/100948
	 * here, each column of 'm' is a variable, for which each
	 * row represents an observation.
	 * the covariance matrix will be of dimension k-by-k
	 * where k = ncol(m)
	 *
	 * in the loop below, we use .array() because Eigen differentiates
	 * between 'Mathematical Vectors' and 'Arrays'.
	 * if we keep it as v1 * v2 without casting v1 and v2 to
	 * an Eigen 'ArrayXd' (instead of Eigen 'VectorXd')
	 * Eigen will perform a dot product instead of elementwise product
	 * (it does this by overloading operator* for each type)
	 * see: https://eigen.tuxfamily.org/dox/group__TutorialArrayClass.html
	 *
	 * We want to use an elementwise product.
	 */
	assert(m.rows() > 1 && "Rows must be greater than 1 for cov function");

	MatrixXd C;
	VectorXd means;
	int nrow, ncol, i, k;

	means = m.colwise().mean(); /* means[i] = mean of values in column 'i' */
	nrow = m.rows();
	ncol = m.cols();
	C.resize(ncol, ncol);

	for (k = 0; k < ncol; k++) {
		for (i = 0; i <= k; i++) {
			C(i, k) = ((m.col(i).array() - means(i)) *
			           (m.col(k).array() - means(k))).sum() /
				   (double (nrow - 1));
		}
	}
	/* the covariance matrix is symetrical. Above, we have only computed
	 * the upper right half of it.
	 * We just copy the data to the lower left half.
	 */
	for (k = 0; k < ncol; k++) {
		for (i = k + 1; i < ncol; i++) {
			C(i, k) = C(k, i);
		}
	}
	return C;
}
/*
 * print a message and kill the program
 */
void die(char const *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);
	exit(1);
}
/*
 * thread safe printf
 */
void tsprintf(char const *fmt, ...)
{
	static std::mutex m;
	std::lock_guard<std::mutex> lock(m);
	va_list args;
	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);
}
/*
 * thread safe cout
 */
template <typename T>
std::ostream & tscout(T const & t)
{
	static std::mutex m;
	std::lock_guard<std::mutex> lock(m);
	std::cout << t << '\n';
	return std::cout;
}
/*
 * print a message
 */
void warn(char const *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);
}
void usage(char const *argv0)
{
	printf(
	"Usage: %s [-h|--help] [-c $$$] [-t $$$]\n"
	"          [-r return]\n"
	"    -h,--help           show this help message\n"
	"    -c $$$              initial capital\n"
	"    -t $$$              transaction cost model. see below\n"
	"    -r return           Minimum portfolio mean return, in percentage form (decimal)\n"
	"See below for info on default values and input data\n"
	"\n"
	"Transaction Costs (-t)\n"
	"    ex)\n"
	"        -t 10.0 == transaction costs of 10 dollars per trade\n"
	"\n"
	"Returns (-r)\n"
	"    Specify in decimal notation; 0.10 == 10 percent.\n"
	"\n"
	"Default values\n"
	"    If the command options are not specified, the following defaults will be assumed:\n"
	"        Initial capital = %.1f\n"
	"        Mean return = %.2f\n"
	"        Transaction costs = %.2f per trade (i.e. per stock)\n"
	"\n"
	"Input Data\n"
	"    From its standard input, the program reads:\n"
	"        a starting date\n"
	"        an ending date\n"
	"        and a list of filenames\n"
	"    The files must be in CSV format, with column labels\n"
	"\n"
	"Example usage (using the getstock program to get the data)\n"
	"    $ ./getstock -k apikey -b 2018-01-01 -e 2018-04-01 -o data -- JPM BAC GS | %s -c 100000 -t 10.0 -m meanvar -r 0.07\n"
	,argv0
	,DEFAULT_INITIAL_CAPITAL
	,DEFAULT_MIN_RETURN
	,DEFAULT_TCOST
	,argv0);
	exit(1);
}

/*
 * R = returns matrix
 * C = covariance matrix
 */
int run(MatrixXd const & R, MatrixXd const & C, VectorXd mean_returns,
         int nsim, double min_return, double init_capital,
	 std::vector<VectorXd> *weights,
	 std::vector<double> *variances,
	 std::vector<double> *returns)
{
	if (init_capital < 0) {
		return -1;
	}
	int n;
	int ncol;
	ncol = C.cols(); /* number of columns, or stocks/variables in dataset */
#pragma omp parallel
	{
		if (omp_get_thread_num() == 0)
			n = omp_get_num_threads();
	}
#pragma omp barrier

#pragma omp parallel num_threads(n)
	{
		/* collecting stats, tl stands for 'thread-local'
		 * we will aggregate all these together in 3 vectors, and report
		 * our findings after all threads finish simulation
		 */ 
		std::vector<VectorXd> tl_weights;
		std::vector<double> tl_returns;
		std::vector<double> tl_variances;

		tl_weights.reserve(nsim / n);
		tl_returns.reserve(nsim / n);
		tl_variances.reserve(nsim / n);

		VectorXd w;
		w.resize(ncol);  /* one weight per security */

		std::mt19937 engine(time(NULL));
		std::uniform_real_distribution<double> dist(0.0, 1.0);

		for (int i = 0; i < nsim / n; i++) {
			// if (i % 50 == 0) {
			// 	tsprintf("Thread %d on trial %d of %d\n", omp_get_thread_num(), i, nsim / n);
			// }
			/* make some random weights, ensure they sum up to one */
			double sum = 0.0;
			for (int k = 0; k < ncol; k++) {
				double tmp = dist(engine);
				w[k] = tmp;
				sum += tmp;
			}
			for (int k = 0; k < ncol; k++) {
				w[k] /= sum;
			}
			double var = w.transpose() * C * w;
			double mu  = w.transpose() * mean_returns;
			if (((mu + 1) * init_capital) >= min_return) {
				tl_weights.push_back(w);
				tl_variances.push_back(var);
				tl_returns.push_back(mu);
			}
		}
		/* append thread local weights.
		 * 'move iterators' will call the move constructor when appending the thread_local
		 * weights to the 'master list' (avoids deep copy of data)
		 */
#pragma omp critical
		weights->insert(weights->end(), std::make_move_iterator(tl_weights.begin()),
		                                std::make_move_iterator(tl_weights.end()));
		variances->insert(variances->end(), tl_variances.begin(), tl_variances.end());
		returns->insert(returns->end(),     tl_returns.begin(), tl_returns.end());
	}
#pragma omp barrier
	// printf("Finished simulation with %d stocks\n", ncol);
	auto found = std::min_element(variances->begin(), variances->end());
	if (found == variances->end()) {
		return -1;
	}
	return found - variances->begin();
}

/*
 * remove a row (index == rm) from a matrix
 */
void rmrow(MatrixXd & matrix, int rm)
{
	int nrow = matrix.rows() - 1;
	int ncol = matrix.cols();

	if (rm < nrow) {
		matrix.block(rm, 0, nrow - rm, ncol) = matrix.block(rm + 1, 0, nrow - rm, ncol);
	}
	matrix.conservativeResize(nrow, ncol);
}
/*
 * remove a column (index == rm) from a matrix
 */
void rmcol(MatrixXd & matrix, int rm)
{
	int nrow = matrix.rows();
	int ncol = matrix.cols() - 1;

	if (rm < ncol) {
		matrix.block(0, rm, nrow, ncol - rm) = matrix.block(0, rm + 1, nrow, ncol - rm);
	}
	matrix.conservativeResize(nrow, ncol);
}
/*
 * Remove an element from an Eigen vector
 */
void eigen_vector_erase(VectorXd *v, int i)
{
	int size = v->size();
	if (i < size - 1) {
		v->segment(i, size - i - 1) = v->segment(i + 1, size - i - 1);
	}
	v->conservativeResize(size - 1);
}
int main(int argc, char **argv)
{
	double initial_capital;
	double min_return;   /* required rate of return */
	double tcost;        /* transaction cost, USD */

	initial_capital = 0.0;
	min_return = 0.0;
	tcost = 0.0;

	char const *argv0 = argv[0];
	int ac;
	char **av;
	for (ac = argc - 1, av = argv + 1; ac && *av
		&& av[0][0] == '-' && av[0][1]; ac--, av++) {
		if (av[0][1] == '-' && av[0][2] == '\0') {
			ac--; av++;
			break;
		}
		char *opt, *tmp, *endptr;  /* endptr for strtod(3) */
		int brk_ = 0;
		for (opt = (*av) + 1; *opt && !brk_; opt++) {
			switch (*opt) {
			case 'c':
				tmp = (opt[1] != '\0') ? (opt + 1) : (--ac, *(++av));
				initial_capital = strtod(tmp, &endptr);
				if (initial_capital == 0.0 && endptr == tmp) {
					perror("strtod:");
					die("Failed to parse initial capital: %s\n", tmp);
				}
				brk_ = 1;
				break;
			case 't':
				tmp = (opt[1] != '\0') ? (opt + 1) : (--ac, *(++av));
				tcost = strtod(tmp, &endptr);
				if (tcost == 0.0 && endptr == tmp) {
					perror("strtod:");
					die("Failed to parse transaction cost value: %s\n", tmp);
				}
				brk_ = 1;
				break;
				
			case 'r':
				tmp = (opt[1] != '\0') ? (opt + 1) : (--ac, *(++av));
				min_return = strtod(tmp, &endptr);
				if (min_return == 0.0 && endptr == tmp) {
					die("Failed to parse min_return: %s\n", tmp);
				}
				brk_ = 1;
				break;
			case 'h':
				usage(argv0);
			default:
				usage(argv0);
			};
		}
	}
	if (initial_capital == 0.0) {
		warn("Setting initial capital to default: %.1f\n", DEFAULT_INITIAL_CAPITAL);
		initial_capital = DEFAULT_INITIAL_CAPITAL;
	} else {
		printf("initial capital = %.1f\n", initial_capital);
	}
	if (min_return == 0.0) {
		warn("Mean return not specified. Using default value %.4f\n", DEFAULT_MIN_RETURN);
		min_return = DEFAULT_MIN_RETURN;
	} else {
		printf("Mean Return = %.4f\n", min_return);
	}

	/* begin_date, end_date are the periods to run the backtest on */
	std::string begin_date;
	std::string end_date;
	time_t begin, end;

	std::cin >> begin_date;
	std::cin >> end_date;
	begin = strtotime(begin_date.c_str());
	end = strtotime(end_date.c_str());
	if (begin == 0) {
		die("Error parsing date: %s\n", begin_date.c_str());
	}
	if (end == 0) {
		die("Error parsing date: %s\n", end_date.c_str());
	}
	/* gather a list of filenames from the standard input */
	std::vector<std::string> files;
	std::string tmp;
	while (std::cin >> tmp) {
		files.emplace_back(tmp);
	}

	/* here, data is a map of the tickers (string) to a vector (array) of prices.
	 * we compute the weekly returns of the assets and stick them in an Eigen Matrix.
	 * we need to keep an ordered list (an array) of the tickers which we can index into,
	 * so we know which column in the matrix corresponds with which security
	 */
	MatrixXd R;
	int nrow, colIndex;
	std::vector<std::string> tickers;
	std::vector<std::string> optimal_tickers;

	auto data = read_stock_data(files, begin, end);
	// printf("data.size = %zu\n",data.size());
	nrow = (*data.begin()).second.size();  /* the number of prices we have for each stock */
	// printf("nrow = %d\n", nrow);
	R.resize(nrow / 5, data.size());       /* divide by five b/c weekly returns... one column per stock */
	colIndex = 0;
	for (auto & d : data) {
		auto ticker = d.first;
		auto & prices = d.second;
		tickers.push_back(ticker);
		R.col(colIndex++) = weeklyReturns(prices);
	}
	MatrixXd C = cov(R);

	C = C.array() * 100.0;

	int optimal_nstocks;
	VectorXd optimal_weights;
	VectorXd exp_returns;
	double min_var = 10000000.0;
	optimal_nstocks = C.cols();

	VectorXd mean_returns = R.colwise().mean();
	/* FIXME: eliminate any variables with a negative mean-return */
	std::vector<VectorXd> weights;
	std::vector<double> variances;
	std::vector<double> returns;
	while (C.cols() > 2) {
		int i = run(R, C, mean_returns, 3000,
		           (initial_capital * (min_return + 1)), initial_capital - (C.cols() * tcost),
			   &weights, &variances, &returns);
		if (i == -1) {
			/* problem was infeasible, and no data recorded.
			 * remove stock with the lowest expected return and try again.
			 */
			i = std::min_element(mean_returns.data(),mean_returns.data() + mean_returns.size()) - mean_returns.data();
			eigen_vector_erase(&mean_returns, i);
			rmcol(R, i);
			rmrow(C, i);
			rmcol(C, i);
			continue;
		}
		/* we found a feasible solution, record if it was better than previous, and remove
		 * the variable with the lowest weight to see if that makes it better
		 */
		double newmin_var = variances[i];
		if (newmin_var < min_var) {
			optimal_nstocks = C.cols();
			optimal_weights = weights[i];
			exp_returns = mean_returns;
			min_var = variances[i];
			optimal_tickers.assign(tickers.begin(), tickers.end());
		}
		/* removing variable */
		i = std::min_element(optimal_weights.data(),optimal_weights.data()+optimal_weights.size()) - optimal_weights.data();
		rmcol(R, i);
		rmrow(C, i);
		rmcol(C, i);
		eigen_vector_erase(&mean_returns, i);
		tickers.erase(tickers.begin() + i);

		weights.clear();
		variances.clear();
		returns.clear();
	}
	printf("Optimal number of stocks: %d\n",optimal_nstocks);
	double test = 0;
	for (int i = 0; i < optimal_nstocks; i++) {
		printf("%s %10.6f\n", optimal_tickers[i].c_str(), optimal_weights[i]);
		test += optimal_weights[i];
	}
	printf("Expected return: %.6f\n", (exp_returns.array() * optimal_weights.array()).sum());
	printf("Min variance:    %.6f\n", min_var);
	printf("net weight: %.4f\n", test);
	return 0;
}
