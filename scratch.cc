
static std::vector<std::vector<double> > read_stock_data(std::vector<std::string> const & filepaths);
static time_t strtotime(char const *s);               /* convert date time string to an integral representation (time_t) */
static time_t read_until(FILE *file, time_t begin);     /* read from data source until we hit a certain point in time (begin) */
static int indexOf(char const *line, char const *field);

std::vector<std::vector<double> >
read_stock_data(std::vector<std::string> const & filepaths)
{
	std::vector<std::vector<double> > data;
	std::vector<double> prices;
	char buf[256];
	char *p;
	for (auto const & fp : filepaths) {
		char const *f = fp.c_str();
		FILE *file = fopen(f, "r");
		if (!file) {
			perror("getstocks");
			die("Aborting");
		}
		fgets(buf, sizeof buf, file);
		int close_index = indexOf(buf, "Adj. Close");
		while (fgets(buf, sizeof buf, file)) {
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
		data.emplace_back(prices);
	}
	return data;
}
time_t strtotime(char const *s)
{
	struct tm tm;
	memset(&tm, 0, sizeof tm);
	if (!strptime(s, DATE_FMT, &tm))
		return 0;
	return mktime(&tm);
}
/*
 * read_until
 *   @file  := FILE stream
 *   @begin := the date we want to synchronize with
 * returns:
 *   the earliest time observation that is >= begin, in Unix time.
 *   or 0 (1970-01-01 00:00:00) if there is no date >= begin
 */
time_t read_until(FILE *file, time_t begin)
{
	char buf[256];
	char *date;
	int date_index;
	int nread;
	struct tm tm;
	time_t tmp;

	/* find 'date' field in the header */
	if (!fgets(buf, sizeof buf, file))
		die("File is empty\n");
	date_index = indexOf(buf, "date");

	/* iterate over dates in file until date >= begin */
	while (fgets(buf, sizeof buf, file)) {
		nread = strlen(buf);
		date = buf;
		for (int i = 0; i < date_index; i++) {
			date = strchr(date, DATA_SEP);
			if (!date) {
				die("Date field not found in data\n");
			}
			date++;
		}
		memset(&tm, 0, sizeof tm);
		if (!strptime(date, DDATE_FMT, &tm)) {
			die("Failed to parse date in file\n");
		}
		tmp = mktime(&tm);
		if (tmp >= begin) {
			fseek(file, SEEK_CUR, -nread);
			return tmp;
		}
	}
	/* we reached EOF without finding a date >= begin
	 * let caller know by returning 0 */
	return 0;
}
 * read_until
 *   @file  := FILE stream
 *   @begin := the date we want to synchronize with
 * returns:
 *   the earliest time observation that is >= begin, in Unix time.
 *   or 0 (1970-01-01 00:00:00) if there is no date >= begin
 */
time_t read_until(FILE *file, time_t begin)
{
	char buf[256];
	char *date;
	int date_index;
	int nread;
	struct tm tm;
	time_t tmp;

	/* find 'date' field in the header */
	if (!fgets(buf, sizeof buf, file))
		die("File is empty\n");
	date_index = indexOf(buf, "date");

	/* iterate over dates in file until date >= begin */
	while (fgets(buf, sizeof buf, file)) {
		nread = strlen(buf);
		date = buf;
		for (int i = 0; i < date_index; i++) {
			date = strchr(date, DATA_SEP);
			if (!date) {
				die("Date field not found in data\n");
			}
			date++;
		}
		memset(&tm, 0, sizeof tm);
		if (!strptime(date, DDATE_FMT, &tm)) {
			die("Failed to parse date in file\n");
		}
		tmp = mktime(&tm);
		if (tmp >= begin) {
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
	if (index > nsep)
		die("Field not found in data\n");
	return index;
}
