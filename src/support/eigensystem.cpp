/*
 * DIPlib 3.0
 * This file contains definitions for function that computes 3D surface area.
 *
 * (c)2016-2017, Cris Luengo.
 * Based on original DIPlib code: (c)1995-2014, Delft University of Technology.
 * Original code written by Jim Mullikin, Pattern Recognition Group.
 */

#include "diplib/library/numeric.h"

#include <Eigen/Eigenvalues>


namespace dip {


void SymmetricEigenValues2D( double const* input, double* lambdas ) {
   Eigen::Map< Eigen::Matrix2d const > matrix( input );
   Eigen::Map< Eigen::Vector2d > eigenvalues( lambdas );
   eigenvalues = matrix.selfadjointView< Eigen::Lower >().eigenvalues();
   if( lambdas[ 0 ] < lambdas[ 1 ] ) {
      std::swap( lambdas[ 0 ], lambdas[ 1 ]);
   }
}

void SymmetricEigenSystem2D( double const* input, double* lambdas, double* vectors ) {
   Eigen::Map< Eigen::Matrix2d const > matrix( input );
   Eigen::SelfAdjointEigenSolver< Eigen::Matrix2d > eigensolver( matrix );
   //if( eigensolver.info() != Eigen::Success ) { abort(); }
   Eigen::Map< Eigen::Vector2d > eigenvalues( lambdas );
   eigenvalues = eigensolver.eigenvalues();
   Eigen::Map< Eigen::Matrix2d > eigenvectors( vectors );
   eigenvectors = eigensolver.eigenvectors();
   if( lambdas[ 0 ] < lambdas[ 1 ] ) {
      std::swap( lambdas[ 0 ], lambdas[ 1 ]);
      std::swap( vectors[ 0 ], vectors[ 2 ]);
      std::swap( vectors[ 1 ], vectors[ 3 ]);
   }
}

void SymmetricEigenValues3D( double const* input, double* lambdas ) {
   Eigen::Map< Eigen::Matrix3d const > matrix( input );
   Eigen::Map< Eigen::Vector3d > eigenvalues( lambdas );
   eigenvalues = matrix.selfadjointView< Eigen::Lower >().eigenvalues();
   if( lambdas[ 0 ] < lambdas[ 1 ] ) {
      std::swap( lambdas[ 0 ], lambdas[ 1 ]);
   }
   if( lambdas[ 1 ] < lambdas[ 2 ] ) {
      std::swap( lambdas[ 1 ], lambdas[ 2 ]);
   }
   if( lambdas[ 0 ] < lambdas[ 1 ] ) {
      std::swap( lambdas[ 0 ], lambdas[ 1 ]);
   }
}

void SymmetricEigenSystem3D( double const* input, double* lambdas, double* vectors) {
   Eigen::Map< Eigen::Matrix3d const > matrix( input );
   Eigen::SelfAdjointEigenSolver< Eigen::Matrix3d > eigensolver( matrix );
   //if( eigensolver.info() != Eigen::Success ) { abort(); }
   Eigen::Map< Eigen::Vector3d > eigenvalues( lambdas );
   eigenvalues = eigensolver.eigenvalues();
   Eigen::Map< Eigen::Matrix3d > eigenvectors( vectors );
   eigenvectors = eigensolver.eigenvectors();
   if( lambdas[ 0 ] < lambdas[ 1 ] ) {
      std::swap( lambdas[ 0 ], lambdas[ 1 ]);
      std::swap( vectors[ 0 ], vectors[ 3 ]);
      std::swap( vectors[ 1 ], vectors[ 4 ]);
      std::swap( vectors[ 2 ], vectors[ 5 ]);
   }
   if( lambdas[ 1 ] < lambdas[ 2 ] ) {
      std::swap( lambdas[ 1 ], lambdas[ 2 ]);
      std::swap( vectors[ 3 ], vectors[ 6 ]);
      std::swap( vectors[ 4 ], vectors[ 7 ]);
      std::swap( vectors[ 5 ], vectors[ 8 ]);
   }
   if( lambdas[ 0 ] < lambdas[ 1 ] ) {
      std::swap( lambdas[ 0 ], lambdas[ 1 ]);
      std::swap( vectors[ 0 ], vectors[ 3 ]);
      std::swap( vectors[ 1 ], vectors[ 4 ]);
      std::swap( vectors[ 2 ], vectors[ 5 ]);
   }
}

#ifdef DIP__ENABLE_DOCTEST

DOCTEST_TEST_CASE("[DIPlib] testing the dip::SymmetricEigenXXX functions") {
   double matrix2[] = { 4, 0, 8 };
   double lambdas[ 3 ];
   double vectors[ 9 ];
   SymmetricEigenValues2DPacked( matrix2, lambdas );
   DOCTEST_CHECK( lambdas[ 0 ] == 8 );
   DOCTEST_CHECK( lambdas[ 1 ] == 4 );
   SymmetricEigenSystem2DPacked( matrix2, lambdas, vectors );
   DOCTEST_CHECK( lambdas[ 0 ] == 8 );
   DOCTEST_CHECK( lambdas[ 1 ] == 4 );
   DOCTEST_CHECK( vectors[ 0 ] == 0 );
   DOCTEST_CHECK( vectors[ 1 ] == 1 );
   DOCTEST_CHECK( vectors[ 2 ] == 1 );
   DOCTEST_CHECK( vectors[ 3 ] == 0 );
   matrix2[ 0 ] = 8;
   matrix2[ 2 ] = 4;
   SymmetricEigenValues2DPacked( matrix2, lambdas );
   DOCTEST_CHECK( lambdas[ 0 ] == 8 );
   DOCTEST_CHECK( lambdas[ 1 ] == 4 );
   SymmetricEigenSystem2DPacked( matrix2, lambdas, vectors );
   DOCTEST_CHECK( lambdas[ 0 ] == 8 );
   DOCTEST_CHECK( lambdas[ 1 ] == 4 );
   DOCTEST_CHECK( vectors[ 0 ] == 1 );
   DOCTEST_CHECK( vectors[ 1 ] == 0 );
   DOCTEST_CHECK( vectors[ 2 ] == 0 );
   DOCTEST_CHECK( vectors[ 3 ] == 1 );
   matrix2[ 0 ] = 3;
   matrix2[ 1 ] = -1;
   matrix2[ 2 ] = 3;
   SymmetricEigenValues2DPacked( matrix2, lambdas );
   DOCTEST_CHECK( lambdas[ 0 ] == 4 );
   DOCTEST_CHECK( lambdas[ 1 ] == 2 );
   SymmetricEigenSystem2DPacked( matrix2, lambdas, vectors );
   DOCTEST_CHECK( lambdas[ 0 ] == 4 );
   DOCTEST_CHECK( lambdas[ 1 ] == 2 );
   DOCTEST_CHECK( vectors[ 0 ] == doctest::Approx(  cos( M_PI/4 )) ); // signs might be different here...
   DOCTEST_CHECK( vectors[ 1 ] == doctest::Approx( -sin( M_PI/4 )) );
   DOCTEST_CHECK( vectors[ 2 ] == doctest::Approx(  sin( M_PI/4 )) );
   DOCTEST_CHECK( vectors[ 3 ] == doctest::Approx(  cos( M_PI/4 )) );

   double matrix3[] = { 4, 0, 0, 8, 0, 6 };
   SymmetricEigenValues3DPacked( matrix3, lambdas );
   DOCTEST_CHECK( lambdas[ 0 ] == 8 );
   DOCTEST_CHECK( lambdas[ 1 ] == 6 );
   DOCTEST_CHECK( lambdas[ 2 ] == 4 );
   SymmetricEigenSystem3DPacked( matrix3, lambdas, vectors );
   DOCTEST_CHECK( lambdas[ 0 ] == 8 );
   DOCTEST_CHECK( lambdas[ 1 ] == 6 );
   DOCTEST_CHECK( lambdas[ 2 ] == 4 );
   DOCTEST_CHECK( vectors[ 0 ] == 0 );
   DOCTEST_CHECK( vectors[ 1 ] == 1 );
   DOCTEST_CHECK( vectors[ 2 ] == 0 );
   DOCTEST_CHECK( vectors[ 3 ] == 0 );
   DOCTEST_CHECK( vectors[ 4 ] == 0 );
   DOCTEST_CHECK( vectors[ 5 ] == 1 );
   DOCTEST_CHECK( vectors[ 6 ] == 1 );
   DOCTEST_CHECK( vectors[ 7 ] == 0 );
   DOCTEST_CHECK( vectors[ 8 ] == 0 );
}

#endif


void EigenValues( dip::uint n, double const* input, dcomplex* lambdas ) {
   // TODO
}

void EigenValues( dip::uint n, dcomplex const* input, dcomplex* lambdas ) {
   // TODO
}

void EigenSystem( dip::uint n, double const* input, dcomplex* lambdas, dcomplex* vectors ) {
   // TODO
}

void EigenSystem( dip::uint n, dcomplex const* input, dcomplex* lambdas, dcomplex* vectors ) {
   // TODO
}


} // namespace dip
