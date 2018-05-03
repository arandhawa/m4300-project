/*
 * Portfolio Optimization Project
 * Author(s)      : Tom Maltese (...)
 * Project URL    : <url>
 * Synopsis       : Reading stock data from Quandl
 */
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>    /* std::rotate, std::find_if */
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <sys/stat.h>  /* see stat(2), mkdir(2) */
#include <sys/types.h>

#include <curl/curl.h>

#define DATE_FMT "%Y-%m-%d"

static int database_init(char const *path);          /* initializes the database if it does not exist */
static std::string slurp(std::string filename);       /* read entire file into a string */
static void strip(std::string *s);                    /* remove whitespace, newlines from '*s' */
static void lstrip(std::string *s);
static void rstrip(std::string *s);
static std::string upper(std::string const & s);      /* upper case a string */
static std::string upper(char const * s);
static std::string make_url(std::string const & ticker,
                            std::string const & token,
                            char const *begin,
                            char const *end);
static std::string make_filename(std::string const & dbroot, std::string const & ticker,
                                 char const *begin, char const *end);
static int has_data(std::string const & filename,
                   char const *begin, char const *end);
static void writef(std::string const & buffer, FILE *file);
static size_t curl_callback(void *buf, size_t size, size_t nmemb, void *cbuf); /* CURL callback writer */
static void die(char const *fmt, ...);               /* print a message and kill the program */
static void usage(char const * argv0);

static const std::string urlbase = "https://www.quandl.com/api/v3/datasets/WIKI/";

int database_init(char const *path)
{
	struct stat buf;      /* $ man 2 stat */
	int status;

	errno = 0;
	memset(&buf, 0, sizeof buf);
	status = mkdir(path, 0755); /* 0755 = drwxr-xr-x */
	if (status == -1) {
		if (errno == EEXIST) {    /* 'path' already exists. Is it a directory? */
			if ((stat(path, &buf) == 0) && S_ISDIR(buf.st_mode))
				return 0;
		}
	}
	return status;
}
std::string slurp(std::string filename)
{
	std::ifstream file{filename};
	if (!file.is_open()) {
		return {};
	}
	std::stringstream ss;
	ss << file.rdbuf();
	return ss.str();
}
void strip(std::string *s)
{
	lstrip(s);
	rstrip(s);
}
void lstrip(std::string *s) /* left-hand side strip */
{
	auto begin = std::find_if(s->begin(), s->end(), [](char c) { return !isspace(c); });
	if (begin != s->end()) {
		auto newend = std::rotate(s->begin(), begin, s->end());
		s->erase(newend, s->end());
	}
}
void rstrip(std::string *s) /* right-hand side strip */
{
	auto end = std::find_if(s->rbegin(), s->rend(), [](char c) { return !isspace(c); }).base();
	s->erase(end, s->end());
}
/* for more info on iterators .begin(), and reverse iterators .rbegin() see:
 * https://www.cs.northwestern.edu/~riesbeck/programming/c++/stl-iterators.html
 * http://en.cppreference.com/w/cpp/iterator/reverse_iterator
 */
std::string upper(std::string const & s)
{
	std::string ret;
	ret.resize(s.size());
	for (int i = 0; i < (int) s.size(); i++) {
		ret[i] = toupper(s[i]);   /* toupper declared in ctype.h */
	}
	return ret;
}
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
 * make_url("TICKER","api_token", "begin", "end")
 * will form a proper URL for communicating with the Quandl API
 * begin and end dates are optional (use NULL or nullptr to omit)
 */
std::string make_url(std::string const & ticker,
                     std::string const & token,
                     char const *begin,
                     char const *end)
{
	auto url = urlbase + ticker + ".csv?order=asc&api_key=" + token;
	if (begin) {
		url.append("&start_date=");
		url.append(begin);
	}
	if (end) {
		url.append("&end_date=");
		url.append(end);
	}
	return url;
}
/*
 * make_filename("/path/to/dir","TICKER","begin","end") = "/path/to/dir/TICKER.begin.end.csv"
 * where:
 *   begin, end are of the form YYYY-mm-dd
 */
std::string make_filename(std::string const & dbroot, std::string const & ticker,
                          char const *begin, char const *end)
{
	std::stringstream fname;
	fname << dbroot;
	if (dbroot.back() != '/') fname << '/';
	fname << ticker;
	if (begin && end) {
		fname << '.' << begin << '.' << end;
	}
	fname << ".csv";
	return fname.str();
}
std::vector<std::string> get_db_files(std::string const & dbroot)
{
	char command[256];
	char buf[512];
	FILE *ls;
	std::vector<std::string> files;

	sprintf(command, "ls %s/*.csv", dbroot.c_str());
	ls = popen(command, "r");
	while (fgets(buf, sizeof buf, ls)) {
		files.emplace_back(buf);
	}
	pclose(ls);
	return files;
}
std::string find_file_by_ticker(std::string const & dbroot,
                                std::string const & ticker,
                                std::vector<std::string> const *files) /* files is optional argument */
{
	std::vector<std::string> m_files;
	if (!files) {
		m_files = get_db_files(dbroot);
		files = &m_files;
	}
	char const *t = ticker.c_str();
	for (auto const & file : *files) {
		char const *fstr = file.c_str();
		if (strcasestr(fstr, t) != NULL) {
			std::string retval = fstr;
			strip(&retval);
			return retval;
		}
	}
	return {};
}
int has_data(std::string const & _filename, char const *a_begin, char const *a_end)
{
	struct stat buf;
	char const *filename;
	char const *begin_date, *end_date;
	struct tm begin_tm, end_tm;
	time_t begin, end, argbegin, argend;

	memset(&buf, 0, sizeof buf);
	memset(&begin_tm, 0, sizeof begin_tm);
	memset(&end_tm, 0, sizeof end_tm);

	filename = _filename.c_str();
	if (stat(filename, &buf) == -1)
		return 0;
	char const *last_slash = strrchr(filename, '/');
	if (last_slash != NULL)
		filename = last_slash + 1;
#ifdef DEBUG
	printf("Trace 1\n");
#endif

	begin_date = strchr(filename, '.');
	if (begin_date == NULL)
		return 0;
#ifdef DEBUG
	printf("Trace 2\n");
#endif
	if (strcmp(begin_date, ".csv") == 0) /* there are no dates in filename */
		return 0;
#ifdef DEBUG
	printf("Trace 3\n");
#endif
	begin_date++;
	end_date = strptime(begin_date, DATE_FMT, &begin_tm);
	if (end_date == NULL || *end_date == '\0') {
		/* and error occurred, or there is no ending date after the beginning date */
		return 0;
	}
#ifdef DEBUG
	printf("Trace 4\n");
#endif
	if (*end_date != '.')
		return 0;
#ifdef DEBUG
	printf("Trace 5\n");
#endif
	end_date++;
	if (strptime(end_date, DATE_FMT, &end_tm) == NULL)
		return 0;
#ifdef DEBUG
	printf("Trace 6\n");
#endif
	
	/* convert broken-down time (struct tm) to an integral type (time_t) */
	begin = mktime(&begin_tm);
	end   = mktime(&end_tm);
	
	/* parse user strings, and compare */
	memset(&begin_tm, 0, sizeof begin_tm);
	memset(&end_tm,   0, sizeof end_tm);
	if (strptime(a_begin, DATE_FMT, &begin_tm) == NULL)
		return 0;
	if (strptime(a_end,   DATE_FMT, &end_tm) == NULL)
		return 0;
	argbegin = mktime(&begin_tm);
	argend   = mktime(&end_tm);
	if (begin <= argbegin && end >= argend)
		return 1;
	return 0;
}
/* write all content of 'buffer' to file */
void writef(std::string const & buffer, FILE *file)
{
	char const *buf;
	int bufsize;

	buf = buffer.c_str();
	bufsize = buffer.size();
	fwrite(buf, bufsize, 1, file);
}
/* when CURL reads data from a URL, it puts it in its own memory buffer.
 * we copy that data from CURL's internal buffers to OUR buffer with this
 * 'callback' function.
 * We provide this function to the CURL library and whenever it has data to give
 * is it will call this function. Hence the name 'callback'.
 *
 * parameters:
 * 	@buf:   CURL's buffer
 * 	@size:  the size of one element in the buffer
 * 	@nmemb: the number of elements in the buffer
 * 	        (so size * nmemb is the total size of the memory pointed to by buf)
 * 	@cbuf:  'client buffer' this is a pointer to our object (which is of type std::string)
 *
 * we have to cast (void *cbuf) back to (std::string *) so that the compiler knows what type
 * it is and then we can call std::string::append
 */
size_t curl_callback(void *buf, size_t size, size_t nmemb, void *cbuf)
{
	((std::string *)cbuf)->append((char*)buf, size * nmemb);
	return size * nmemb;
}
void die(char const *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);
	exit(1);
}
void usage(char const *argv0)
{
	printf(
	"Usage: %s [-h|--help] [-k FILE] [-b DATE]\n"
	"          [-e DATE] [-o DIR] -- [TICKER...]\n"
	"    -h,--help             show this help message\n"
	"    -k                    file containing a Quandl api key (required)\n"
	"    -b                    Beginning date, YYYY-mm-dd\n"
	"    -e                    Ending date, YYYY-mm-dd\n"
	"    -o                    Output directory. If this is omitted\n"
	"                          default behavior is to print to stdout\n"
	"    TICKER...             One or more stock symbols.\n"
	"\n"
	"Notes:\n"
	"    Only the API key and the stock symbols are required arguments\n"
	,argv0);
	exit(1);
}
/*
void test()
{
	char const *begin = "2018-05-01";
	char const *end   = "2018-02-01";
	char const *tickers[] = {
		"Bar", "Baz", "Foo",
	};
	char const *dbroot = ".";
	auto dbfiles = get_db_files(dbroot);
	for (char const *ticker : tickers) {
		auto path = find_file_by_ticker(dbroot, ticker, &dbfiles);
		if (path.empty()) {
			continue;
		} else {
			int s = has_data(path, begin, end);
			if (s) {
				printf("%s has data between %s and %s\n", path.c_str(), begin, end);
			} else {
				printf("%s does not have data between %s and %s\n", path.c_str(), begin, end);
			}
		}
	}
}
int main()
{
	test();
}
*/
int main(int argc, char **argv)
{
	char const *argv0 = argv[0];
	if (argc < 2) {
		usage(argv0);
	} else if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
		usage(argv0);
	}
	CURL *curl;                /* libcurl state */
	std::string api_key_file;
	std::string api_key;
	std::string begin;         /* beginning and ending dates */
	std::string end;
	std::string dbroot;        /* root directory of the database passed by the user via [-o] option */
	std::vector<std::string> dbfiles;     /* all .csv files found in dbroot */
	int ac;
	char **av;
	std::string buffer;        /* memory buffer containing the stock data */

	/* parsing command line options */
	for (ac = argc - 1, av = argv + 1;
	       ac && *av && av[0][0] == '-' && av[0][1]; ac--, av++) {
		if (av[0][1] == '-' && av[0][2] == '\0') {
			ac--; av++;
			break;
		}
		char *opt, *tmp;
		int brk_ = 0;
		for (opt = (*av) + 1; *opt && !brk_; opt++) {
			switch (*opt) {
			case 'k':
				tmp = (opt[1] != '\0') ? (opt + 1) : (--ac, *(++av));
				api_key_file = tmp;
				brk_ = 1;
				break;
			case 'b':
				tmp = (opt[1] != '\0') ? (opt + 1) : (--ac, *(++av));
				begin = tmp;
				brk_ = 1;
				break;
			case 'e':
				tmp = (opt[1] != '\0') ? (opt + 1) : (--ac, *(++av));
				end = tmp;
				brk_ = 1;
				break;
			case 'o':
				tmp = (opt[1] != '\0') ? (opt + 1) : (--ac, *(++av));
				dbroot = tmp;
				brk_ = 1;
				break;
			case 'h':
				usage(argv0);
			default:
				usage(argv0);
			}
		}
	}
	if (!ac)                  die("Must specify at least one stock symbol\n");
	if (api_key_file.empty()) die("API Key file missing\n");

	api_key = slurp(api_key_file);
	strip(&api_key);
	if (api_key.empty())      die("Failed to read api key from file: %s\n", api_key_file.c_str());

	if (!dbroot.empty()) {
		int status = database_init(dbroot.c_str());
		if (status == -1) {
			perror("database_init: ");
			die("Failed to initialize the database\nAborting\n");
		}
		/* remove any trailing '/' characters from the path */
		if (dbroot.size() > 1)
			while (dbroot.size() > 1 && dbroot.back() == '/')
				dbroot.pop_back();
		dbfiles = get_db_files(dbroot);
	}

	curl = curl_easy_init();
	for ( ; ac && *av; ac--, av++) {
		auto ticker = upper(*av);
		if (!dbfiles.empty()) {
			auto fpath = find_file_by_ticker(dbroot, ticker, &dbfiles);
			if (has_data(fpath, begin.c_str(), end.c_str()))
				continue;
		}
		auto url = make_url(ticker, api_key, begin.c_str(), end.c_str());
		char const *urlc = url.c_str();

		curl_easy_setopt(curl, CURLOPT_URL, urlc);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
		curl_easy_perform(curl);

		if (!dbroot.empty()) {
			auto filename = make_filename(dbroot, ticker, begin.c_str(), end.c_str());
			FILE *file = fopen(filename.c_str(), "w");
			if (!file) {
				die("Failed to open file: %s\nAborting\n", filename.c_str());
			}
			writef(buffer, file);
			fclose(file);
		} else /* print everything to stdout */ {
			/* here, we delimit the data by 'begin' and 'end' markers, so when we process it with another program
			 * that program can tell which data belongs to which stock
			 * sprintf will fill 'msgbuf' with begin:TICKER (or end:TICKER) where 'TICKER' is replaced with the
			 * acutal stock symbol.
			 * See also:
			 *  $ man sprintf
			 *  $ man fwrite
			 */
			char msgbuf[64];
			sprintf(msgbuf, "begin:%s\n", ticker.c_str());
			fwrite(msgbuf, strlen(msgbuf), 1, stdout);
			writef(buffer, stdout);
			sprintf(msgbuf, "end:%s\n", ticker.c_str());
			fwrite(msgbuf, strlen(msgbuf), 1, stdout);
		}
		buffer.clear();   /* clear the memory buffer for next iteration (next stock data .csv) */
	}
	curl_easy_cleanup(curl);
}
