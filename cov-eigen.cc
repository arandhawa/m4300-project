/*
 * Computing the covariance matrix
 */
#include <assert.h>
#include <iostream>

#include <Eigen/Core>

using namespace Eigen;

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
int main()
{
	MatrixXd R;

	R.resize(4,2);
	R << 0.4, 0.10,
	     0.5, 0.20,
	     0.55, 0.18,
	     0.88, 0.05;

	std::cout << R << '\n';

	VectorXd mean = R.colwise().mean();

	std::cout << "Mean:\n" << mean << '\n';
	
	MatrixXd C = cov(R);

	std::cout << "Covariance matrix:\n" << C << '\n';
}
