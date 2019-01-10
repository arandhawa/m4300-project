/*
 * Portfolio Optimization Project
 * Authors:
 *   Gabriel Etrata
 *   Liming Kang
 *   Tom Maltese
 *   Pav Singh
 *   Zeqi Wang
 * URL: https://github.com/tommalt/m4300-project
 * Synopsis: Reading stock data from Quandl
 */
#ifndef _GNU_SOURCE
  #define _GNU_SOURCE
#endif
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>    /* strncasecmp */

#include <algorithm>    /* std::rotate, std::find_if */
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <sys/stat.h>  /* see stat(2), mkdir(2) */
#include <sys/types.h>

#include <curl/curl.h>

/* date format used for file naming */
#define DATE_FMT "%Y-%m-%d"

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

void strip(std::string *s)
{
	lstrip(s);
	rstrip(s);
}

void lstrip(char *s)
{
	char *begin;
	for (begin = s; *begin != '\0' && isspace(*begin); begin++)
		;
	if (*begin != '\0') {
		memmove(s, begin, strlen(begin) + 1);
	} else {
		*s = '\0';
	}
}

void rstrip(char *s)
{
	char *end;
	for (end = s + strlen(s) - 1; end >= s && isspace(*end); end--)
		;
	end++;
	*end = '\0';
}

void strip(char *s)
{
	lstrip(s);
	rstrip(s);
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
	if (dbroot.back() != '/') {
		fname << '/';
	}
	fname << ticker;
	if (begin && end) {
		fname << '.' << begin << '.' << end;
	}
	fname << ".csv";
	return fname.str();
}

/* get paths to all files in database */
std::vector<std::string> get_db_files(std::string const & dbroot)
{
	char command[512];
	char buf[512];
	FILE *ls;
	std::vector<std::string> files;

	sprintf(command, "ls %s/*.csv 2>/dev/null", dbroot.c_str());
	ls = popen(command, "r");
	while (fgets(buf, sizeof buf, ls)) {
		strip(buf);
		files.emplace_back(buf);
	}
	pclose(ls);
	return files;
}

std::vector<std::string>::iterator
find_file_by_ticker(std::string const & ticker,
                    std::vector<std::string> & files)

{
	char const *t = ticker.c_str();
	auto it = files.begin();
	for ( ; it != files.end(); it++) {
		char const *fstr = it->c_str();
		if (strcasestr(fstr, t) != NULL) {
			return it;
		}
	}
	return it;
}

/* check if _filename includes the dates specified */
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

	begin_date = strchr(filename, '.');
	if (begin_date == NULL)
		return 0;
	if (strcmp(begin_date, ".csv") == 0) /* there are no dates in filename */
		return 0;
	begin_date++;

	end_date = strptime(begin_date, DATE_FMT, &begin_tm);
	if (end_date == NULL || *end_date == '\0') {
		/* and error occurred, or there is no ending date after the beginning date */
		return 0;
	}
	if (*end_date != '.')
		return 0;
	end_date++;
	if (strptime(end_date, DATE_FMT, &end_tm) == NULL)
		return 0;

	/* convert broken-down time (struct tm) to an integral type (time_t), parse user strings, and compare */
	begin = mktime(&begin_tm);
	end   = mktime(&end_tm);
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

/*
 * Curl Callback to write directly to a file.
 */
size_t curl_callback_fwrite(void *buf, size_t size, size_t nmemb, void *file)
{
	fwrite(buf, size, nmemb, (FILE *)file);
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
	"    All of the arguments are required\n"
	,argv0);
	exit(1);
}

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
	std::string dbroot;        /* root directory of the database (passed by the user via [-o] option) */
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
	if (!ac) {
		die("Must specify at least one stock symbol\n");
	}
	if (begin.empty() || end.empty()) {
		die("Must specify begin and end dates\n");
	}
	if (api_key_file.empty()) {
		die("API Key file missing\n");
	}

	api_key = slurp(api_key_file);
	strip(&api_key);

	if (api_key.empty()) {
		die("Failed to read api key from file: %s\n", api_key_file.c_str());
	}
	if (dbroot.empty()) {
		die("Database root is required\n");
	}

	/* remove any trailing '/' characters from the path */
	if (dbroot.size() > 1) {
		while (dbroot.size() > 1 && dbroot.back() == '/') {
			dbroot.pop_back();
		}
	}

	int status = database_init(dbroot.c_str());
	if (status == -1) {
		perror("database_init:");
		die("Failed to initialize the database\nAborting\n");
	}
	dbfiles = get_db_files(dbroot);

	printf("%s\n%s\n", begin.c_str(), end.c_str());
	curl = curl_easy_init();
	for ( ; ac && *av; ac--, av++) {
		auto ticker = upper(*av);
		auto found = find_file_by_ticker(ticker, dbfiles);
		std::string filename;
		FILE *file;
		if (found != dbfiles.end()) {
			filename = *found;
			if (has_data(filename, begin.c_str(), end.c_str())) {
				printf("%s\n", filename.c_str());
				continue;
			} else { /* remove this file, replace it with the new one */
				remove(filename.c_str());
				*found = filename = make_filename(dbroot, ticker, begin.c_str(), end.c_str());
			}
		} else {
			filename = make_filename(dbroot, ticker, begin.c_str(), end.c_str());
		}
		file = fopen(filename.c_str(), "w");
		auto url = make_url(ticker, api_key, begin.c_str(), end.c_str());
		char const *urlc = url.c_str();

		curl_easy_setopt(curl, CURLOPT_URL, urlc);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback_fwrite);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
		curl_easy_perform(curl);
		fclose(file);
		printf("%s\n", filename.c_str());
	}
	curl_easy_cleanup(curl);
}

/*
 * gdb
 * set follow-fork-mode parent
 * set detach-on-fork on
 */
