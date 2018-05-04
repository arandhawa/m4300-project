#include <stdio.h>
#include <vector>

double mean(std::vector<double> const & x)
{
	double s;
	int size;

	s = 0.0;
	size = x.size();
	for (int i = 0; i < size; i++) {
		s += x[i];
	}
	return s / (double)size;
}
double var(std::vector<double> const & x,
	   double *_mx1)
{
	double mx1;
	if (!_mx1)
		mx1 = mean(x);
	else
		mx1 = *_mx1;
	
	std::vector<double> v;
	int size;
	size = x.size();
	v.resize(size);
	for (int i = 0; i < size; i++) {
		double tmp = x[i] - mx1;
		v.push_back(tmp);
	}
	return mean(v);

}
double cov(std::vector<double> const & x1,
           std::vector<double> const & x2,
	   double *_mx1, double *_mx2)
{
	double mx1, mx2;
	if (!_mx1)
		mx1 = mean(x1);
	else
		mx1 = *_mx1;
	if (!_mx2)
		mx2 = mean(x2);
	else
		mx2 = *_mx2;
	
	std::vector<double> c;
	int size;
	size = x1.size();
	c.resize(size);
#ifdef DEBUG
	assert(size == (int) x2.size() && "Invalid dimensions");
#endif
	for (int i = 0; i < size; i++) {
		double tmp = (x1[i] - mx1) * (x2[i] - mx2);
		c[i] = tmp;
	}
	return mean(c);
}
int main()
{
	std::vector<double> x1 = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
	std::vector<double> x2 = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
	//std::vector<double> x2 = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

	double c1 = cov(x1, x2, NULL, NULL);
	double mx1 = mean(x1);
	double mx2 = mean(x2);
	double c2 = cov(x1, x2, &mx1, &mx2);

	printf("%.4f %.4f\n", c1, c2);
	printf("%.4f %.4f\n", var(x1, NULL), var(x2, NULL));


}
