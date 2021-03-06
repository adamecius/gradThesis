#include <iostream>
#include <fstream>
#include <string>
#include <complex>
#include <Eigen/Eigen>
#include <vector>
#include "matplotlibcpp.h"

using namespace std;
using namespace Eigen;
namespace plt = matplotlibcpp;


SparseMatrix<complex<double>> csrFile2Eigen (string csrFileName)
{
  ifstream csrFile(csrFileName);
  
  if (csrFile.is_open()) {
    string line;
    getline (csrFile, line);
    string::size_type sz0;

    int dim = stoi(line, &sz0);
    int nnz = stoi(line.substr(sz0));

    getline (csrFile, line);
    complex<double> values[nnz];

    sz0 = 0;
    string::size_type sz1 = 0;
    for (int i=0; i < nnz; i++) {
      line = line.substr(sz0+sz1);
      values[i] = {stod(line, &sz0), stod(line.substr(sz0), &sz1)}; 
    }

    int innerIndices[nnz];
    
    getline(csrFile, line);
    sz0 = 0;
    for (int i=0; i < nnz; i++) {
      line = line.substr(sz0);
      innerIndices[i] = stoi(line, &sz0);
    }

    int outerIndexPtr[dim+1];
    
    getline(csrFile, line);
    sz0 = 0;
    for (int i=0; i <= dim; i++) {
      line = line.substr(sz0);
      outerIndexPtr[i] = stoi(line, &sz0);
    }
    
    csrFile.close();

    Map<SparseMatrix<complex<double>>> sm(dim, dim, nnz, outerIndexPtr, innerIndices, values);

    return sm;
  }
  else {cout << "Could not open " << csrFileName << endl;}
}


int main(int argc, char** argv) {

  SparseMatrix<complex<double>> hamiltonianMatrix = csrFile2Eigen(argv[1]);
  int M_moments = 750;
  float
    eps = 0.075,
    deltaE =
    (hamiltonianMatrix * VectorXcd::Ones(hamiltonianMatrix.rows())).cwiseAbs().maxCoeff() * 2 / (2.0 - eps),
    meanE = 0;

  auto rescaledHamOp = [&](VectorXcd vec) {return (hamiltonianMatrix * vec - meanE * vec) / deltaE;};
  auto randomVector = [&]() {
    return exp(complex<double>(0, 2) * M_PI * ArrayXcd::Random(hamiltonianMatrix.rows())).matrix();};
  
  ArrayXcd moments(M_moments);

  VectorXcd
    alpha = randomVector(),
    alpha_next = rescaledHamOp(alpha),
    alpha_temp;

  moments(0) = alpha.squaredNorm();
  moments(1) = alpha.dot(alpha_next);
  
  for (int m = 2/2; m < M_moments/2; m++) {
    alpha_temp = alpha_next;
    alpha_next = 2. * rescaledHamOp(alpha_next) - alpha;
    alpha = alpha_temp;

    moments(2*m) = 2. * alpha.squaredNorm() - moments(0);
    moments(2*m+1) = 2. * alpha_next.dot(alpha) - moments(1);
  }

  if (M_moments % 2) {moments(M_moments-1) = 2. * alpha_next.dot(alpha) - moments(0);}

  ArrayXd lorentz_kernel(M_moments);
  for (int m = 0; m < M_moments; m++) {
    lorentz_kernel(m) = ((M_moments + 1 - m) * cos(M_PI * m / (M_moments + 1)) +
			 sin(M_PI * m / (M_moments + 1)) / tan(M_PI / (M_moments + 1))) / (M_moments + 1);
  }

  moments *= lorentz_kernel / (deltaE * M_PI);

  ArrayXd
    energies = ArrayXd::LinSpaced(M_moments, -deltaE/2, deltaE/2),
    bound_energy = (energies - meanE) / deltaE;

  ArrayXXcd chebyshevE(bound_energy.size(), M_moments);
  chebyshevE.col(0) = ArrayXd::Ones(bound_energy.size());
  chebyshevE.col(1) = bound_energy;
  for (int m = 2; m < M_moments; m++) {
    chebyshevE.col(m) = 2. * bound_energy * chebyshevE.col(m-1) - chebyshevE.col(m-2);
  }
  chebyshevE.rightCols(M_moments-1) *= 2. ;

  ArrayXcd densities = (deltaE * M_PI * (1. - bound_energy.square()).sqrt()).inverse() *
    (chebyshevE.rowwise() * moments.transpose()).rowwise().sum();

  vector<double>
    Energies(energies.data(), energies.data() + energies.size()),
    Densities(densities.real().data(), densities.real().data() + densities.real().size());
  
  plt::plot(Energies, Densities);
  plt::show();
}
