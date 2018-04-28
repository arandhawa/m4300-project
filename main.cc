/*
 * Portfolio Optimization
 * Author(s)      : Tom Maltese (...)
 * Project URL    : <url>
 */
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>   /* std::rotate, std::find_if */
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <curl/curl.h>

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
static std::string make_filename(std::string const & ticker,
                                 char const *begin, char const *end);
static void write_buf(std::string const & buffer, std::string const & filename);
static size_t curl_callback(void *buf, size_t size, size_t nmemb, void *cbuf); /* CURL callback writer */
static void die(char const *fmt, ...);               /* print a message and kill the program */
static void usage(char const * argv0);


/* global variables */
static const std::string urlbase = "https://www.quandl.com/api/v3/datasets/WIKI/";

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
void lstrip(std::string *s)
{
	auto begin = std::find_if(s->begin(), s->end(), [](char c) { return !isspace(c); });
	if (begin != s->end()) {
		auto newend = std::rotate(s->begin(), begin, s->end());
		s->erase(newend, s->end());
	}
}
void rstrip(std::string *s)
{
	auto end = std::find_if(s->rbegin(), s->rend(), [](char c) { return !isspace(c); }).base();
	s->erase(end, s->end());
}
std::string upper(std::string const & s)
{
	std::string ret;
	ret.resize(s.size());
	for (int i = 0; i < (int) s.size(); i++) {
		ret[i] = toupper(s[i]);
	}
	return ret;
}
std::string upper(char const *s)
{
	int size = (int) strlen(s);
	std::string ret;
	ret.resize(size);
	for (int i = 0; i < size; i++) {
		ret[i] = s[i];
	}
	return ret;
}
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
std::string make_filename(std::string const & ticker,
                          char const *begin, char const *end)
{
	std::stringstream fname;
	fname << ticker;
	if (begin && end) {
		fname << '-' << begin << '-' << end;
	}
	fname << ".csv";
	return fname.str();
}
void write_buf(std::string const & buffer, std::string const & filename)
{
	FILE *file;
	char const *buf;
	int bufsize;

	buf     = buffer.c_str();
	bufsize = buffer.size();
	file    = fopen(filename.c_str(), "w");
	fwrite(buf, bufsize, 1, file);
	fclose(file);
}
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
	"Usage: %s [--help | -h] [-k] [-b] [-e] -- [TICKER...]\n"
	"    -h,--help             show this help message\n"
	"    -k                    file containing a Quandl api key\n"
	"    -b                    Beginning date, YYYY-mm-dd\n"
	"    -e                    Ending date, YYYY-mm-dd\n"
	"    TICKER...             One or more stock symbols.\n"
	"\n"
	"Notes:\n"
	"    All arguments are required\n"
	"    Arguments passed to -b and -e are inclusive\n"
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
	CURL *curl;
	std::vector<std::string> tickers;
	std::vector<std::string> data;
	std::string api_key_file;
	std::string api_key;
	std::string begin;
	std::string end;
	int ac;
	char **av;
	std::string buffer;

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
			case 'h':
				usage(argv0);
			default:
				usage(argv0);
			}
		}
	}
	if (api_key_file.empty())
		die("API Key file missing\n");
	if (begin.empty())
		die("Beginning date missing\n");
	if (end.empty())
		die("Ending date missing\n");
	if (!ac)
		die("Must specify at least one symbol\n");

	api_key = slurp(api_key_file);
	strip(&api_key);
	if (api_key.empty())
		die("Failed to read api key from file: %s\n", api_key_file.c_str());

	curl = curl_easy_init();
	for ( ; ac && *av; ac--, av++) {
		// TODO: write code to detect if a file is already found,
		// instead of going to the NIC.
		auto ticker = upper(*av);
		auto url = make_url(ticker, api_key, begin.c_str(), end.c_str());
		char const *urlc = url.c_str();
		auto filename = make_filename(ticker, begin.c_str(), end.c_str());

		curl_easy_setopt(curl, CURLOPT_URL, urlc);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
		curl_easy_perform(curl);

		write_buf(buffer, filename);
	}
	curl_easy_cleanup(curl);
	
}
