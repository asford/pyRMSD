#include "RMSD.h"
#include <iostream>
using namespace std;

RMSD::RMSD(int numberOfConformations, int atomsPerConformation, double* allCoordinates){
	this->numberOfConformations = numberOfConformations;
	this->atomsPerConformation = atomsPerConformation;
	this->allCoordinates = allCoordinates;
	this->coordinatesPerConformation = atomsPerConformation*3;
}
/*
 * Sets a different set of coordinates for RMSD calculation and fit.
 */
void RMSD::setRMSDCoordinates(int atomsPerRMSDConformation, int coordinatesPerRMSDConformation,
		double* const allRMSDCoordinates){
	this->atomsPerRMSDConformation = atomsPerRMSDConformation;
	this->coordinatesPerRMSDConformation = coordinatesPerRMSDConformation;
	this->allRMSDCoordinates = allRMSDCoordinates;
}

void RMSD::iterativeSuperposition(double rmsd_diff_to_stop = 1e-4){
	cout<<"Iterative superposition not implemented yet for this algorithm."<<flush<<endl;
}
