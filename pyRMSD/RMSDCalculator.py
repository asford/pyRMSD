import numpy.linalg
import pyRMSD.calculators
from pyRMSD.utils.proteinReading import flattenCoords
from pyRMSD.availableCalculators import availableCalculators


class RMSDCalculator(object):
    def __init__(self, coordsets, calculatorType):
        """
        Class constructor
        
        @param coordsets: An array containing the used coordinates of each conformation. It has the following form:
            coordsets: [Conformation 1, Conformation 2, ..., Conformation N]  
            Conformation: [Atom 1, Atom 2,..., Atom M]  
            Atom: [x,y,z]
        
        @param calculatorType: One of the calculators returned by 'availableCalculators()'. i.e. OMP_CALCULATOR
         
        @author: vgil
        @date: 26/11/2012
        """
        if not calculatorType in availableCalculators():
            print "Calculator ",calculatorType, " is not an available calculator."
            raise KeyError
        else:
            self.calculatorType = calculatorType
            self.coordsets = coordsets
            self.number_of_conformations = len(coordsets)
            self.number_of_atoms = len(coordsets[0])
            self.__threads_per_block = 32
            self.__blocks_per_grid = 8
            self.__number_of_threads = 8
    
    def pairwise(self, first_conformation_number, second_conformation_number):
        """
        Calculates the rmsd of two conformations.
        
        @param first_conformation_number: Id of the reference conformation.
        
        @param second_conformation_number: Id of the conformation that will be superposed into the reference conformation.
        
        @return: The RMSD value.
        
        @author: vgil
        @date: 26/11/2012
        """
        first_coords = self.coordsets[first_conformation_number]
        second_coords = self.coordsets[second_conformation_number]
        tmp_coordsets = numpy.array([first_coords,second_coords])
        return RMSDCalculator(tmp_coordsets, self.calculatorType).oneVsFollowing(0)[0]
    
    def oneVsTheOthers(self, conformation_number):
        """
        Calculates the RMSD between a reference conformation and all other conformations in the set.
        
        @param conformation_number: The id of the reference structure.
        
        @return: A numpy array of RMSD values.
        
        @author: vgil
        @date: 26/11/2012
        """
        previous_coords = self.coordsets[:conformation_number]
        following_coords = self.coordsets[conformation_number+1:]
        rearranged_coords_list = [self.coordsets[conformation_number]]
                
        for coords in previous_coords:
            rearranged_coords_list.append(coords)
        
        for coords in following_coords:
            rearranged_coords_list.append(coords)
        
        rearranged_coords = numpy.array(rearranged_coords_list)
        
        return RMSDCalculator(rearranged_coords, self.calculatorType).oneVsFollowing(0)
    
    def oneVsFollowing(self, conformation_number):
        """
        Calculates the RMSD between a reference conformation and all other conformations with an id 
        greater than it.
        
        @param conformation_number: The id of the reference structure.
        
        @return: A numpy array of RMSD values.
        
        @author: vgil
        @date: 26/11/2012
        """     
        if "PYTHON" in self.calculatorType:
            target = self.coordsets[conformation_number]
            return p_oneVsFollowing(target, self.coordsets[conformation_number+1:])
        else:
            np_coords = flattenCoords(self.coordsets)
            return pyRMSD.calculators.oneVsFollowing(availableCalculators()[self.calculatorType], np_coords, 
                                                     self.number_of_atoms, conformation_number, self.number_of_conformations,
                                                     self.__number_of_threads, self.__threads_per_block, self.__blocks_per_grid)
        return []
        
    def pairwiseRMSDMatrix(self):
        """
        Calculates the pairwise RMSD matrix for all conformations in the coordinates set.
        
        @return: A numpy array with the upper triangle of the matrix, in row major format.
        
        @author: vgil
        @date: 26/11/2012
        """
        if "PYTHON" in self.calculatorType :
            return p_calculateRMSDCondensedMatrix(self.coordsets)
        else:
            np_coords = flattenCoords(self.coordsets)
            return pyRMSD.calculators.calculateRMSDCondensedMatrix(availableCalculators()[self.calculatorType], np_coords, 
                                                                   self.number_of_atoms, self.number_of_conformations,
                                                                   self.__number_of_threads, self.__blocks_per_grid, self.__blocks_per_grid)
    
    def setNumberOfOpenMPThreads(self, number_of_threads):
        """
        Sets the number of threads to be used by an OpenMP calculator.
        
        @param number_of_threads: The number of threads to be used by OpenMP runtime.
        
        @author: vgil
        @date: 26/11/2012
        """
        if ("OMP" in self.calculatorType):
            self.__number_of_threads = number_of_threads
        else:
            print "Cannot set any OpenMP related parameter using this calculator: ", self.calculatorType
            raise KeyError
    
    def setCUDAKernelThreadsPerBlock(self, number_of_threads, number_of_blocks):
        """
        Sets the number of threads per block and blocks per grid in CUDA calculators.
        
        @param number_of_threads: Number of threads per block to be used when launching CUDA Kernels.
        
        @param number_of_blocks: Number of blocks per grid to be used when launching CUDA Kernels.
        
        @author: vgil
        @date: 26/11/2012
        """
        if ("CUDA" in self.calculatorType):
            self.__threads_per_block = number_of_threads
            self.__blocks_per_grid = number_of_blocks
        else:
            print "Cannot set any CUDA related parameter using this calculator: ", self.calculatorType
            raise KeyError
    
def p_calculateRMSDCondensedMatrix(coordsets):
    """
    Calculates the pairwise RMSD matrix for all conformations in the coordinates set.
    
    @param coordsets: The array containing the coordinates of all conformations.
    
    @return: A numpy array with the upper triangle of the matrix, in row major format.
        
    @author: vgil 
    @date: 26/11/2012
    """
    rmsd_data = []
    for i, coords in enumerate(coordsets[:-1]):
        rmsd_data.extend(p_oneVsFollowing(coords,coordsets[i+1:]))
    return rmsd_data

def p_oneVsFollowing(reference_conformation, coordsets):
    """
    Slightly modified Prody's code for  Kabsch's superposition algorithm.
    
    @param reference_conformation: The id of the reference conformation.
    
    @param coordsets: The array containing the coordinates of all conformations.
    
    @return: A numpy array with the upper triangle of the matrix, in row major format.
        
    @author: Ahmet Bakan
    @author: vgil (small changes)
    @date: 26/11/2012
    """
    rmsd_data = []
    divByN = 1.0 / reference_conformation.shape[0]
    tar_com = reference_conformation.mean(0)
    tar_org = (reference_conformation - tar_com)
    mob_org = numpy.zeros(tar_org.shape, dtype=coordsets.dtype)
    tar_org = tar_org.T

    for i, mob in enumerate(coordsets):    
        mob_com = mob.mean(0)        
        matrix = numpy.dot(tar_org, numpy.subtract(mob, mob_com, mob_org))
            
        u, s, v = numpy.linalg.svd(matrix)
        Id = numpy.array([ [1, 0, 0], [0, 1, 0], [0, 0, numpy.sign(numpy.linalg.det(matrix))] ])
        rotation = numpy.dot(v.T, numpy.dot(Id, u.T))

        coordsets[i] = numpy.dot(mob_org, rotation) 
        numpy.add(coordsets[i], tar_com, coordsets[i]) 
        rmsd_data.append(numpy.sqrt(((coordsets[i]-reference_conformation) ** 2).sum() * divByN))
    
    return rmsd_data