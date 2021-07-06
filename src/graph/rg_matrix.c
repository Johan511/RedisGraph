/*
* Copyright 2018-2021 Redis Labs Ltd. and Contributors
*
* This file is available under the Redis Labs Source Available License Agreement
*/

#include "RG.h"
#include "rg_matrix.h"
#include "../util/rmalloc.h"

#define DELTA_MAX_PENDING_CHANGES 2

void RG_Matrix_setDirty
(
	RG_Matrix C
) {
	ASSERT(C);
	C->dirty = true;
}

static inline void _SetUndirty
(
	RG_Matrix C
) {
	ASSERT(C);
	C->dirty = false;
}

RG_Matrix RG_Matrix_getTranspose
(
	const RG_Matrix C
) {
	ASSERT(C != NULL);
	return C->transposed;
}

// returns underlying GraphBLAS matrix
GrB_Matrix RG_Matrix_getGrB_Matrix
(
	const RG_Matrix C
) {
	ASSERT(C != NULL);
	return RG_MATRIX_MATRIX(C);
}

// returns underlying delta plus GraphBLAS matrix
GrB_Matrix RG_Matrix_getDeltaPlus
(
	RG_Matrix C
) {
	ASSERT(C != NULL);
	return RG_MATRIX_DELTA_PLUS(C);
}

bool RG_Matrix_isDirty
(
	const RG_Matrix C
) {
	ASSERT(C);
	return C->dirty;
}

// locks the matrix
void RG_Matrix_Lock
(
	RG_Matrix C
) {
	ASSERT(C);
	pthread_mutex_lock(&C->mutex);
}

// unlocks the matrix
void RG_Matrix_Unlock
(
	RG_Matrix C
) {
	ASSERT(C);
	pthread_mutex_unlock(&C->mutex);
}

void RG_Matrix_setMultiEdge
(
	RG_Matrix C,
	bool multi_edge
) {
	ASSERT(C);
	if(C->maintain_transpose) RG_Matrix_setMultiEdge(C->transposed, multi_edge);
	C->multi_edge = multi_edge;
}

bool RG_Matrix_getMultiEdge
(
	const RG_Matrix C
) {
	ASSERT(C);
	return C->multi_edge;
}

GrB_Info RG_Matrix_nvals    // get the number of entries in a matrix
(
    GrB_Index *nvals,       // matrix has nvals entries
    const RG_Matrix A       // matrix to query
) {
	ASSERT(A      !=  NULL);
	ASSERT(nvals  !=  NULL);

	GrB_Matrix  m;
	GrB_Matrix  dp;
	GrB_Matrix  dm;
	GrB_Info    info;

	GrB_Index  m_nvals   =  0;
	GrB_Index  dp_nvals  =  0;
	GrB_Index  dm_nvals  =  0;

	// nvals = nvals(M) + nvals(DP) - nvals(DM)

	m   =  RG_MATRIX_MATRIX(A);
	dp  =  RG_MATRIX_DELTA_PLUS(A);
	dm  =  RG_MATRIX_DELTA_MINUS(A);

	info = GrB_Matrix_nvals(&m_nvals, m);
	ASSERT(info == GrB_SUCCESS);
	info = GrB_Matrix_nvals(&dp_nvals, dp);
	ASSERT(info == GrB_SUCCESS);
	info = GrB_Matrix_nvals(&dm_nvals, dm);
	ASSERT(info == GrB_SUCCESS);

	*nvals = m_nvals + dp_nvals - dm_nvals;
	return info;
}

GrB_Info RG_Matrix_resize       // change the size of a matrix
(
    RG_Matrix C,                // matrix to modify
    GrB_Index nrows_new,        // new number of rows in matrix
    GrB_Index ncols_new         // new number of columns in matrix
) {
	ASSERT(C != NULL);
	GrB_Info info;

	if(C->maintain_transpose) {
		info = RG_Matrix_resize(C->transposed, nrows_new, ncols_new);
		ASSERT(info == GrB_SUCCESS);
	}

	GrB_Matrix  m            =  RG_MATRIX_MATRIX(C);
	GrB_Matrix  delta_plus   =  RG_MATRIX_DELTA_PLUS(C);
	GrB_Matrix  delta_minus  =  RG_MATRIX_DELTA_MINUS(C);

	info = GrB_Matrix_resize(m, nrows_new, ncols_new);
	ASSERT(info == GrB_SUCCESS);

	info = GrB_Matrix_resize(delta_plus, nrows_new, ncols_new);
	ASSERT(info == GrB_SUCCESS);
	
	info = GrB_Matrix_resize(delta_minus, nrows_new, ncols_new);
	ASSERT(info == GrB_SUCCESS);
	
	return info;
}

GrB_Info RG_Matrix_extractElement_BOOL     // x = A(i,j)
(
    bool *x,                               // extracted scalar
    const RG_Matrix A,                     // matrix to extract a scalar from
    GrB_Index i,                           // row index
    GrB_Index j                            // column index
) {
	ASSERT(x != NULL);
	ASSERT(A != NULL);

	GrB_Info info;
	GrB_Matrix  m      =  RG_MATRIX_MATRIX(A);
	GrB_Matrix  dp     =  RG_MATRIX_DELTA_PLUS(A);
	GrB_Matrix  dm     =  RG_MATRIX_DELTA_MINUS(A);

	// see if entry is marked for deletion
	info = GrB_Matrix_extractElement_BOOL(x, dm, i, j);
	if(info == GrB_SUCCESS) {
		// entry is marked for deletion
		return GrB_NO_VALUE;
	}

	// see if entry is in either 'm' or 'dp'
	info = GrB_Matrix_extractElement_BOOL(x, m, i, j);
	if(info == GrB_SUCCESS) return info;

	info = GrB_Matrix_extractElement_BOOL(x, dp, i, j);
	return info;
}

GrB_Info RG_Matrix_extractElement_UINT64   // x = A(i,j)
(
    uint64_t *x,                           // extracted scalar
    const RG_Matrix A,                     // matrix to extract a scalar from
    GrB_Index i,                           // row index
    GrB_Index j                            // column index
) {
	ASSERT(x != NULL);
	ASSERT(A != NULL);

	GrB_Info info;
	GrB_Matrix  m      =  RG_MATRIX_MATRIX(A);
	GrB_Matrix  dp     =  RG_MATRIX_DELTA_PLUS(A);
	GrB_Matrix  dm     =  RG_MATRIX_DELTA_MINUS(A);

	// see if entry is marked for deletion
	info = GrB_Matrix_extractElement_UINT64(x, dm, i, j);
	if(info == GrB_SUCCESS) {
		// entry is marked for deletion
		return GrB_NO_VALUE;
	}

	// see if entry is in either 'm' or 'dp'
	info = GrB_Matrix_extractElement_UINT64(x, m, i, j);
	if(info == GrB_SUCCESS) return info;

	info = GrB_Matrix_extractElement_UINT64(x, dp, i, j);
	return info;
}

GrB_Info RG_Matrix_removeElement
(
    RG_Matrix C,                    // matrix to remove entry from
    GrB_Index i,                    // row index
    GrB_Index j                     // column index
) {
	ASSERT(C);

	bool      x;
	GrB_Info  info;

	if(C->maintain_transpose) {
		info = RG_Matrix_removeElement(C->transposed, j, i);
		ASSERT(info == GrB_SUCCESS);
	}

	bool        in_m   =  false;
	bool        in_dp  =  false;
	GrB_Matrix  m      =  RG_MATRIX_MATRIX(C);
	GrB_Matrix  dp     =  RG_MATRIX_DELTA_PLUS(C);
	GrB_Matrix  dm     =  RG_MATRIX_DELTA_MINUS(C);

#if RG_DEBUG
	// check bounds
	GrB_Index nrows;
	GrB_Index ncols;
	GrB_Matrix_nrows(&nrows, m);
	GrB_Matrix_ncols(&ncols, m);
	ASSERT(i < nrows);
	ASSERT(j < ncols);
#endif

	// entry should exists in either delta-plus or main
	// locate entry
	info = GrB_Matrix_extractElement(&x, m, i, j);
	in_m = (info == GrB_SUCCESS);

	info = GrB_Matrix_extractElement(&x, dp, i, j);
	in_dp = (info == GrB_SUCCESS);

	if(!(in_m || in_dp)) {
		// entry missing from both 'm' and 'dp'
		return GrB_SUCCESS;
	}

	// removed entry should be in either 'm' or in 'dp'
	ASSERT(in_m != in_dp);

	if(in_m) {
		// entry is removed from 'm'
		// mark deletion in delta minus
		info = GrB_Matrix_setElement(dm, true, i, j);
		ASSERT(info == GrB_SUCCESS);

		// TODO: postpone entity deletion to sync
	} else {
		// remove entry from 'dp'
		info = GrB_Matrix_removeElement(dp, i, j);
		ASSERT(info == GrB_SUCCESS);

		// TODO: delete entity
	}

	RG_Matrix_setDirty(C);
	return info;
}

GrB_Info RG_Matrix_subassign_UINT64 // C(I,J)<Mask> = accum (C(I,J),x)
(
    RG_Matrix C,                    // input/output matrix for results
    const GrB_Matrix Mask,          // optional mask for C(I,J), unused if NULL
    const GrB_BinaryOp accum,       // optional accum for Z=accum(C(I,J),x)
    uint64_t x,                     // scalar to assign to C(I,J)
    const GrB_Index *I,             // row indices
    GrB_Index ni,                   // number of row indices
    const GrB_Index *J,             // column indices
    GrB_Index nj,                   // number of column indices
    const GrB_Descriptor desc       // descriptor for C(I,J) and Mask
) {
	// TODO: do we really need this function?
	ASSERT(false);
	ASSERT(C != NULL);
	GrB_Info info;

	if(C->maintain_transpose) {
		info = RG_Matrix_subassign_UINT64(C->transposed, Mask, accum, x, J, nj,
				I, ni, desc); 
		ASSERT(info == GrB_SUCCESS);
	}

	GrB_Matrix delta_plus = RG_MATRIX_DELTA_PLUS(C);

	info = GxB_Matrix_subassign_UINT64   // C(I,J)<Mask> = accum (C(I,J),x)
		(
		 delta_plus,           // input/output matrix for results
		 Mask,                 // optional mask for C(I,J), unused if NULL
		 accum,                // optional accum for Z=accum(C(I,J),x)
		 x,                    // scalar to assign to C(I,J)
		 I,                    // row indices
		 ni,                   // number of row indices
		 J,                    // column indices
		 nj,                   // number of column indices
		 desc                  // descriptor for C(I,J) and Mask
		);

	if(info == GrB_SUCCESS) _SetDirty(C);
	return info;
}

static GrB_Info RG_Matrix_sync
(
	RG_Matrix C
) {
	ASSERT(C != NULL);

	GrB_Matrix      m     =  RG_MATRIX_MATRIX(C);
	GrB_Descriptor  desc  =  GrB_NULL;
	GrB_Matrix      mask  =  GrB_NULL;
	GrB_Matrix      dp    =  RG_MATRIX_DELTA_PLUS(C);
	GrB_Matrix      dm    =  RG_MATRIX_DELTA_MINUS(C);

	GrB_Info info;
	GrB_Index dp_nvals;
	GrB_Index dm_nvals;
	GrB_Matrix_nvals(&dp_nvals, dp);
	GrB_Matrix_nvals(&dm_nvals, dm);

	bool  additions  =  dp_nvals  >  0;
	bool  deletions  =  dm_nvals  >  0;

	//--------------------------------------------------------------------------
	// perform deletions
	//--------------------------------------------------------------------------

	if(deletions) {
		info = GrB_transpose(m, dm, GrB_NULL, m, GrB_DESC_RSCT0);
		ASSERT(info == GrB_SUCCESS);

		// clear delta minus
		info = GrB_Matrix_clear(dm);
		ASSERT(info == GrB_SUCCESS);
	}

	//--------------------------------------------------------------------------
	// perform additions
	//--------------------------------------------------------------------------

	if(additions) {
		GrB_Type t;
		GrB_BinaryOp op;
		info = GxB_Matrix_type(&t, m);
		ASSERT(info == GrB_SUCCESS);

		// use SECOND to overwrite entries that exists in both:
		// 'm' and 'delta-plus' with values from 'delta-plus'
		op = (t == GrB_BOOL) ? GrB_SECOND_BOOL : GrB_SECOND_UINT64;
		info = GrB_Matrix_eWiseAdd_BinaryOp(m, GrB_NULL, GrB_NULL, op, m,
				dp, GrB_NULL);
		ASSERT(info == GrB_SUCCESS);

		// clear delta plus
		info = GrB_Matrix_clear(dp);
		ASSERT(info == GrB_SUCCESS);
	}

	info = GrB_wait(&m);
	ASSERT(info == GrB_SUCCESS);

	return info;
}

GrB_Info RG_Matrix_wait
(
	RG_Matrix A,
	bool force_sync
) {
	ASSERT(A != NULL);
	if(A->maintain_transpose) RG_Matrix_wait(A->transposed, force_sync);
	
	GrB_Info    info         =  GrB_SUCCESS;
	GrB_Matrix  m            =  RG_MATRIX_MATRIX(A);
	GrB_Matrix  delta_plus   =  RG_MATRIX_DELTA_PLUS(A);
	GrB_Matrix  delta_minus  =  RG_MATRIX_DELTA_MINUS(A);

	info = GrB_wait(&delta_plus);
	ASSERT(info == GrB_SUCCESS);

	info = GrB_wait(&delta_minus);
	ASSERT(info == GrB_SUCCESS);

	// check if merge is required
	
	GrB_Index delta_plus_nvals;
	GrB_Index delta_minus_nvals;
	GrB_Matrix_nvals(&delta_plus_nvals, delta_plus);
	GrB_Matrix_nvals(&delta_minus_nvals, delta_minus);
	if(force_sync ||
	   delta_plus_nvals + delta_minus_nvals >= DELTA_MAX_PENDING_CHANGES) {
		info = RG_Matrix_sync(A);
	}

	_SetUndirty(A);

	return info;
}

