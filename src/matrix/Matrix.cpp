#include "Statistics.h"
#include <Python.h>
#include <structmember.h>
#include <numpy/arrayobject.h>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <vector>
using namespace std;

#define calc_vector_pos(i,j,matrix) (i*(matrix->row_length_minus_one) - i - ((( i-1)*i) >> 1 ) + j - 1)

typedef struct {
    PyObject_HEAD
    int row_length;
    int row_length_minus_one;
    int data_size;
    float* data;
    StatisticsCalculator* statisticsCalculator;
    bool statistics_already_calculated;
    PyObject* zero;
} CondensedMatrix;

/*
 * Object destructor. Only has to free memory used for data storage and the statistics calculator.
 */
static void condensedMatrix_dealloc(CondensedMatrix* self){

	// Special for this object
	delete [] self->data;
	delete self->statisticsCalculator;

	// Python ops
	Py_XDECREF(self->zero);
    self->ob_type->tp_free((PyObject*)self);
}

/*
 *
 */
static PyObject* condensedMatrix_new(PyTypeObject *type, PyObject *args, PyObject *kwds){
	CondensedMatrix* self;
	self = (CondensedMatrix*) type->tp_alloc(type, 0);
    if (self != NULL) {
    	self->row_length = 0;
    	self->data_size = 0;
    	self->data = NULL;
    	self->zero =  Py_BuildValue("d", 0.); // To be returned always that a 0 is needed
    	self->statisticsCalculator = NULL;
    	self->statistics_already_calculated = false;
    }
    return (PyObject *) self;
}

static int condensedMatrix_init(CondensedMatrix *self, PyObject *args, PyObject *kwds){
	PyObject* input = NULL;
	PyArrayObject* rmsd_numpy_array = NULL;
	bool numpy = false;
	if (! PyArg_ParseTuple(args, "O!",&PyArray_Type,&rmsd_numpy_array)){
		//cout<<"[CondensedMatrix] Getting matrix data from sequence."<<endl;
		if (! PyArg_ParseTuple(args, "O",&input)){
			PyErr_SetString(PyExc_RuntimeError, "Error parsing parameters.");
			return -1;
		}

		rmsd_numpy_array = (PyArrayObject *) PyArray_ContiguousFromObject(input, PyArray_DOUBLE, 1, 1);
	}
	else{
		//cout<<"[CondensedMatrix] Getting matrix data from numpy array."<<endl;
		numpy = true;
	}

    if (rmsd_numpy_array == NULL){
    	PyErr_SetString(PyExc_RuntimeError, "Impossible to create intermediary data.\n"
    										  "Check that the parameter is a sequence and there's memory available.");
    	return -1;
    }

    self->data_size = rmsd_numpy_array->dimensions[0];
    self->row_length = (int) (1 + sqrt(1+8*self->data_size))/2;
    self->row_length_minus_one = self->row_length - 1;
    self->data = new float[self->data_size];

    double * inner_data = (double*)(rmsd_numpy_array->data);

	for(int i = 0;i < self->data_size; ++i){
		self->data[i] = (float) (inner_data[i]);
	}

	// Let's alloc the statistics object
	self->statisticsCalculator =  new StatisticsCalculator(self->data, self->data_size);

	if(!numpy){
    	Py_DECREF(rmsd_numpy_array);
    }

	return 0;
}

char row_length_text[] = "row_length";
char data_size_text[] = "data_size";
char bogus_description_text[] = "TODO";

static PyMemberDef condensedMatrix_members[] = {
    {row_length_text, T_INT, offsetof(CondensedMatrix, row_length), READONLY,	PyDoc_STR(bogus_description_text)},
    {data_size_text, T_INT, offsetof(CondensedMatrix, data_size), READONLY, PyDoc_STR(bogus_description_text)},
    {NULL}  /* Sentinel */
};

static PyObject* condensedMatrix_get_number_of_rows(CondensedMatrix* self, PyObject *args){
	return Py_BuildValue("i", self->row_length);
}

static PyObject* condensedMatrix_get_data(CondensedMatrix* self, PyObject *args){
	npy_intp dims[] = {self->data_size};
	return  PyArray_SimpleNewFromData(1, dims, NPY_FLOAT, self->data);
}

#include "Matrix.Statistics.cpp"

#include "Matrix.Neighbours.cpp"

static PyMethodDef condensedMatrix_methods[] = {
	// Basic field access
	{"get_number_of_rows", (PyCFunction) condensedMatrix_get_number_of_rows, METH_NOARGS,PyDoc_STR("description")},
	{"get_data", (PyCFunction) condensedMatrix_get_data, METH_NOARGS,PyDoc_STR("description")},

	// Statistics
	{"recalculateStatistics", (PyCFunction) condensedMatrix_calculate_statistics, METH_NOARGS,PyDoc_STR("description")},
	{"calculateMean", 		(PyCFunction) condensedMatrix_get_mean, METH_NOARGS,PyDoc_STR("description")},
	{"calculateVariance", 	(PyCFunction) condensedMatrix_get_variance, METH_NOARGS,PyDoc_STR("description")},
	{"calculateSkewness", 	(PyCFunction) condensedMatrix_get_skewness, METH_NOARGS,PyDoc_STR("description")},
	{"calculateKurtosis", 	(PyCFunction) condensedMatrix_get_kurtosis, METH_NOARGS,PyDoc_STR("description")},
	{"calculateMax", 		(PyCFunction) condensedMatrix_get_max, METH_NOARGS,PyDoc_STR("description")},
	{"calculateMin", 		(PyCFunction) condensedMatrix_get_min, METH_NOARGS,PyDoc_STR("description")},

	// Matrix as graph
	{"get_neighbors_for_node", (PyCFunction)condensedMatrix_get_neighbors_for_node, METH_VARARGS,PyDoc_STR("description")},
	{"choose_node_with_higher_cardinality", (PyCFunction)condensedMatrix_choose_node_with_higher_cardinality, METH_VARARGS,PyDoc_STR("description")},
	//{"calculate_rw_laplacian", (PyCFunction)condensedMatrix_calculate_rw_laplacian, METH_NOARGS,PyDoc_STR("description")},
	//{"calculate_affinity_matrix", (PyCFunction)condensedMatrix_calculate_affinity_matrix, METH_VARARGS,PyDoc_STR("description")},
	{NULL}  /* Sentinel */
};

PyObject* condensedMatrix_subscript(CondensedMatrix *self, PyObject *key){
	int pos, i,j;
	i = PyInt_AS_LONG(PyTuple_GET_ITEM(key,0));
	j = PyInt_AS_LONG(PyTuple_GET_ITEM(key,1));

	if (i < j){
		pos = calc_vector_pos(i,j,self);
		return PyFloat_FromDouble((double)self->data[pos]);
	}
	else{
		if (i==j){
			Py_INCREF(self->zero);
			return self->zero;
		}
		else{
			pos = calc_vector_pos(j,i,self);
			return PyFloat_FromDouble((double)self->data[pos]);
		}
	}
}

int condensedMatrix_ass_subscript(CondensedMatrix *self, PyObject *key, PyObject *v){
	int pos, i,j;
	i = PyInt_AS_LONG(PyTuple_GET_ITEM(key,0));
	j = PyInt_AS_LONG(PyTuple_GET_ITEM(key,1));

	if (i < j){
		pos = calc_vector_pos(i,j,self);
		self->data[pos] = PyFloat_AsDouble(v);
	}
	else{
		if (i!=j){
			pos = calc_vector_pos(j,i,self);
			self->data[pos] = PyFloat_AsDouble(v);
		}
	}
	return 0;
}

Py_ssize_t condensedMatrix_length(CondensedMatrix *self){
	return self->row_length;
}

static PyMappingMethods pdb_as_mapping = {
    (lenfunc)     	condensedMatrix_length,				/*mp_length*/
    (binaryfunc)	condensedMatrix_subscript,			/*mp_subscript*/
    (objobjargproc)	condensedMatrix_ass_subscript,		/*mp_ass_subscript*/
};

static PyTypeObject CondensedMatrixType = {
    PyObject_HEAD_INIT(NULL)
    0,                         						/*ob_size*/
    "condensedMatrix.CondensedMatrixType",      	/*tp_name*/
    sizeof(CondensedMatrix), 	/*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)condensedMatrix_dealloc, /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    &pdb_as_mapping,           /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT| Py_TPFLAGS_BASETYPE , /*tp_flags*/
    "Condensed matrix as in pdist",           /* tp_doc */
	0,		               					  /* tp_traverse */
	0,		               					  /* tp_clear */
	0,		               					  /* tp_richcompare */
	0,		              					  /* tp_weaklistoffset */
	0,		               	   /* tp_iter */
	0,		               	   /* tp_iternext */
	condensedMatrix_methods,   /* tp_methods */
	condensedMatrix_members,   /* tp_members */
	0,                         /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	(initproc)condensedMatrix_init, /* tp_init */
	0,                         		/* tp_alloc */
	condensedMatrix_new,        		/* tp_new */
};

#ifndef PyMODINIT_FUNC	/* declarations for DLL import/export */
#define PyMODINIT_FUNC void
#endif


PyMODINIT_FUNC initcondensedMatrix(void){
    PyObject* module;

    if (PyType_Ready(&CondensedMatrixType) < 0)
        return;

    module = Py_InitModule3("condensedMatrix", NULL,"Fast Access Condensed Matrix");
    if (module == NULL)
          return;

    Py_INCREF(&CondensedMatrixType);
    PyModule_AddObject(module, "CondensedMatrix", (PyObject*) &CondensedMatrixType);

    import_array();
}
