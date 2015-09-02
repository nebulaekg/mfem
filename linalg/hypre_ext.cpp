// Copyright (c) 2010, Lawrence Livermore National Security, LLC. Produced at
// the Lawrence Livermore National Laboratory. LLNL-CODE-443211. All Rights
// reserved. See file COPYRIGHT for details.
//
// This file is part of the MFEM library. For more information and source code
// availability see http://mfem.org.
//
// MFEM is free software; you can redistribute it and/or modify it under the
// terms of the GNU Lesser General Public License (as published by the Free
// Software Foundation) version 2.1 dated February 1999.

#include "../config/config.hpp"

#ifdef MFEM_USE_MPI

#include "hypre_ext.hpp"
#include <limits>

namespace mfem
{
namespace internal
{

/*--------------------------------------------------------------------------
 *                        A*X = B style elimination
 *--------------------------------------------------------------------------*/

/*
  Function:  hypre_CSRMatrixEliminateAXB

  Eliminate the rows and columns of A corresponding to the
  given sorted (!) list of rows. Put I on the eliminated diagonal,
  subtract columns times X from B.
*/
void hypre_CSRMatrixEliminateAXB(hypre_CSRMatrix *A,
                                 HYPRE_Int nrows_to_eliminate,
                                 HYPRE_Int *rows_to_eliminate,
                                 hypre_Vector *X,
                                 hypre_Vector *B)
{
   HYPRE_Int  i, j;
   HYPRE_Int  irow, jcol, ibeg, iend, pos;
   HYPRE_Real a;

   HYPRE_Int  *Ai    = hypre_CSRMatrixI(A);
   HYPRE_Int  *Aj    = hypre_CSRMatrixJ(A);
   HYPRE_Real *Adata = hypre_CSRMatrixData(A);
   HYPRE_Int   nrows = hypre_CSRMatrixNumRows(A);

   HYPRE_Real *Xdata = hypre_VectorData(X);
   HYPRE_Real *Bdata = hypre_VectorData(B);

   /* eliminate the columns */
   for (i = 0; i < nrows; i++)
   {
      ibeg = Ai[i];
      iend = Ai[i+1];
      for (j = ibeg; j < iend; j++)
      {
         jcol = Aj[j];
         pos = hypre_BinarySearch(rows_to_eliminate, jcol, nrows_to_eliminate);
         if (pos != -1)
         {
            a = Adata[j];
            Adata[j] = 0.0;
            Bdata[i] -= a * Xdata[jcol];
         }
      }
   }

   /* remove the rows and set the diagonal equal to 1 */
   for (i = 0; i < nrows_to_eliminate; i++)
   {
      irow = rows_to_eliminate[i];
      ibeg = Ai[irow];
      iend = Ai[irow+1];
      for (j = ibeg; j < iend; j++)
      {
         if (Aj[j] == irow)
         {
            Adata[j] = 1.0;
         }
         else
         {
            Adata[j] = 0.0;
         }
      }
   }
}

/*
  Function:  hypre_CSRMatrixEliminateOffdColsAXB

  Eliminate the given sorted (!) list of columns of A, subtract them from B.
*/
void hypre_CSRMatrixEliminateOffdColsAXB(hypre_CSRMatrix *A,
                                         HYPRE_Int ncols_to_eliminate,
                                         HYPRE_Int *eliminate_cols,
                                         HYPRE_Real *eliminate_coefs,
                                         hypre_Vector *B)
{
   HYPRE_Int i, j;
   HYPRE_Int ibeg, iend, pos;
   HYPRE_Real a;

   HYPRE_Int *Ai = hypre_CSRMatrixI(A);
   HYPRE_Int *Aj = hypre_CSRMatrixJ(A);
   HYPRE_Real *Adata = hypre_CSRMatrixData(A);
   HYPRE_Int nrows = hypre_CSRMatrixNumRows(A);

   HYPRE_Real *Bdata = hypre_VectorData(B);

   for (i = 0; i < nrows; i++)
   {
      ibeg = Ai[i];
      iend = Ai[i+1];
      for (j = ibeg; j < iend; j++)
      {
         pos = hypre_BinarySearch(eliminate_cols, Aj[j], ncols_to_eliminate);
         if (pos != -1)
         {
            a = Adata[j];
            Adata[j] = 0.0;
            Bdata[i] -= a * eliminate_coefs[pos];
         }
      }
   }
}

/*
  Function:  hypre_CSRMatrixEliminateOffdRowsAXB

  Eliminate (zero) the given list of rows of A.
*/
void hypre_CSRMatrixEliminateOffdRowsAXB(hypre_CSRMatrix *A,
                                         HYPRE_Int  nrows_to_eliminate,
                                         HYPRE_Int *rows_to_eliminate)
{
   HYPRE_Int  *Ai    = hypre_CSRMatrixI(A);
   HYPRE_Real *Adata = hypre_CSRMatrixData(A);

   HYPRE_Int i, j;
   HYPRE_Int irow, ibeg, iend;

   for (i = 0; i < nrows_to_eliminate; i++)
   {
      irow = rows_to_eliminate[i];
      ibeg = Ai[irow];
      iend = Ai[irow+1];
      for (j = ibeg; j < iend; j++)
      {
         Adata[j] = 0.0;
      }
   }
}

/*
  Function:  hypre_ParCSRMatrixEliminateAXB

  This function eliminates the global rows and columns of a matrix
  A corresponding to given lists of sorted (!) local row numbers,
  so that the solution to the system A*X = B is X_b for the given rows.

  The elimination is done as follows:

                    (input)                  (output)

                / A_ii | A_ib \          / A_ii |  0   \
            A = | -----+----- |   --->   | -----+----- |
                \ A_bi | A_bb /          \   0  |  I   /

                        / X_i \          / X_i \
                    X = | --- |   --->   | --- |  (no change)
                        \ X_b /          \ X_b /

                        / B_i \          / B_i - A_ib * X_b \
                    B = | --- |   --->   | ---------------- |
                        \ B_b /          \        X_b       /

*/
void hypre_ParCSRMatrixEliminateAXB(hypre_ParCSRMatrix *A,
                                    HYPRE_Int num_rowscols_to_elim,
                                    HYPRE_Int *rowscols_to_elim,
                                    hypre_ParVector *X,
                                    hypre_ParVector *B)
{
   hypre_CSRMatrix *diag = hypre_ParCSRMatrixDiag(A);
   hypre_CSRMatrix *offd = hypre_ParCSRMatrixOffd(A);
   HYPRE_Int diag_nrows  = hypre_CSRMatrixNumRows(diag);
   HYPRE_Int offd_ncols  = hypre_CSRMatrixNumCols(offd);

   hypre_Vector *Xlocal = hypre_ParVectorLocalVector(X);
   hypre_Vector *Blocal = hypre_ParVectorLocalVector(B);

   HYPRE_Real   *Bdata  = hypre_VectorData(Blocal);
   HYPRE_Real   *Xdata  = hypre_VectorData(Xlocal);

   HYPRE_Int  num_offd_cols_to_elim;
   HYPRE_Int  *offd_cols_to_elim;
   HYPRE_Real *eliminate_coefs;

   /* figure out which offd cols should be eliminated and with what coef */
   hypre_ParCSRCommHandle *comm_handle;
   hypre_ParCSRCommPkg *comm_pkg;
   HYPRE_Int num_sends;
   HYPRE_Int index, start;
   HYPRE_Int i, j, k, irow;

   HYPRE_Real *eliminate_row = hypre_CTAlloc(HYPRE_Real, diag_nrows);
   HYPRE_Real *eliminate_col = hypre_CTAlloc(HYPRE_Real, offd_ncols);
   HYPRE_Real *buf_data, coef;

   /* make sure A has a communication package */
   comm_pkg = hypre_ParCSRMatrixCommPkg(A);
   if (!comm_pkg)
   {
      hypre_MatvecCommPkgCreate(A);
      comm_pkg = hypre_ParCSRMatrixCommPkg(A);
   }

   /* HACK: rows that shouldn't be eliminated are marked with quiet NaN;
      those that should are set to the boundary value from X; this is to
      avoid sending complex type (int+double) or communicating twice. */
   for (i = 0; i < diag_nrows; i++)
   {
      eliminate_row[i] = std::numeric_limits<HYPRE_Real>::quiet_NaN();
   }
   for (i = 0; i < num_rowscols_to_elim; i++)
   {
      irow = rowscols_to_elim[i];
      eliminate_row[irow] = Xdata[irow];
   }

   /* use a Matvec communication pattern to find (in eliminate_col)
      which of the local offd columns are to be eliminated */
   num_sends = hypre_ParCSRCommPkgNumSends(comm_pkg);
   buf_data = hypre_CTAlloc(HYPRE_Real,
                            hypre_ParCSRCommPkgSendMapStart(comm_pkg,
                                                            num_sends));
   index = 0;
   for (i = 0; i < num_sends; i++)
   {
      start = hypre_ParCSRCommPkgSendMapStart(comm_pkg, i);
      for (j = start; j < hypre_ParCSRCommPkgSendMapStart(comm_pkg, i+1); j++)
      {
         k = hypre_ParCSRCommPkgSendMapElmt(comm_pkg,j);
         buf_data[index++] = eliminate_row[k];
      }
   }
   comm_handle = hypre_ParCSRCommHandleCreate(1, comm_pkg,
                                              buf_data, eliminate_col);

   /* do sequential part of the elimination while stuff is getting sent */
   hypre_CSRMatrixEliminateAXB(diag, num_rowscols_to_elim, rowscols_to_elim,
                               Xlocal, Blocal);

   /* finish the communication */
   hypre_ParCSRCommHandleDestroy(comm_handle);

   /* received eliminate_col[], count offd columns to eliminate */
   num_offd_cols_to_elim = 0;
   for (i = 0; i < offd_ncols; i++)
   {
      coef = eliminate_col[i];
      if (coef == coef) // test for NaN
      {
         num_offd_cols_to_elim++;
      }
   }

   offd_cols_to_elim = hypre_CTAlloc(HYPRE_Int, num_offd_cols_to_elim);
   eliminate_coefs = hypre_CTAlloc(HYPRE_Real, num_offd_cols_to_elim);

   /* get a list of offd column indices and coefs */
   num_offd_cols_to_elim = 0;
   for (i = 0; i < offd_ncols; i++)
   {
      coef = eliminate_col[i];
      if (coef == coef) // test for NaN
      {
         offd_cols_to_elim[num_offd_cols_to_elim] = i;
         eliminate_coefs[num_offd_cols_to_elim] = coef;
         num_offd_cols_to_elim++;
      }
   }

   hypre_TFree(buf_data);
   hypre_TFree(eliminate_row);
   hypre_TFree(eliminate_col);

   /* eliminate the off-diagonal part */
   hypre_CSRMatrixEliminateOffdColsAXB(offd, num_offd_cols_to_elim,
                                       offd_cols_to_elim,
                                       eliminate_coefs, Blocal);

   hypre_CSRMatrixEliminateOffdRowsAXB(offd, num_rowscols_to_elim,
                                       rowscols_to_elim);

   /* set boundary values in the rhs */
   for (int i = 0; i < num_rowscols_to_elim; i++)
   {
      irow = rowscols_to_elim[i];
      Bdata[irow] = Xdata[irow];
   }

   hypre_TFree(offd_cols_to_elim);
   hypre_TFree(eliminate_coefs);
}


/*--------------------------------------------------------------------------
 *                        (A + Ae) style elimination
 *--------------------------------------------------------------------------*/

/*
  Function:  hypre_CSRMatrixElimCreate

  Prepare the Ae matrix: count nnz, initialize I, allocate J and data.
*/
void hypre_CSRMatrixElimCreate(hypre_CSRMatrix *A,
                               hypre_CSRMatrix *Ae,
                               HYPRE_Int nrows, HYPRE_Int *rows,
                               HYPRE_Int ncols, HYPRE_Int *cols,
                               HYPRE_Int *col_mark)
{
   HYPRE_Int  i, j, col;
   HYPRE_Int  A_beg, A_end;

   HYPRE_Int  *A_i     = hypre_CSRMatrixI(A);
   HYPRE_Int  *A_j     = hypre_CSRMatrixJ(A);
   HYPRE_Int   A_rows  = hypre_CSRMatrixNumRows(A);

   hypre_CSRMatrixI(Ae) = hypre_TAlloc(HYPRE_Int, A_rows+1);

   HYPRE_Int  *Ae_i    = hypre_CSRMatrixI(Ae);
   HYPRE_Int   nnz     = 0;

   for (i = 0; i < A_rows; i++)
   {
      Ae_i[i] = nnz;

      A_beg = A_i[i];
      A_end = A_i[i+1];

      if (hypre_BinarySearch(rows, i, nrows) >= 0)
      {
         /* full row */
         nnz += A_end - A_beg;

         if (col_mark)
         {
            for (j = A_beg; j < A_end; j++)
            {
               col_mark[A_j[j]] = 1;
            }
         }
      }
      else
      {
         /* count columns */
         for (j = A_beg; j < A_end; j++)
         {
            col = A_j[j];
            if (hypre_BinarySearch(cols, col, ncols) >= 0)
            {
               nnz++;
               if (col_mark) { col_mark[col] = 1; }
            }
         }
      }
   }
   Ae_i[A_rows] = nnz;

   hypre_CSRMatrixJ(Ae) = hypre_TAlloc(HYPRE_Int, nnz);
   hypre_CSRMatrixData(Ae) = hypre_TAlloc(HYPRE_Real, nnz);
   hypre_CSRMatrixNumNonzeros(Ae) = nnz;
}

/*
  Function:  hypre_CSRMatrixEliminateRowsCols

  Eliminate rows and columns of A, store eliminated values in Ae.
  If 'diag' is nonzero, the eliminated diagonal of A is set to identity.
  If 'col_remap' is not NULL it specifies renumbering of columns of Ae.
*/
void hypre_CSRMatrixEliminateRowsCols(hypre_CSRMatrix *A,
                                      hypre_CSRMatrix *Ae,
                                      HYPRE_Int nrows, HYPRE_Int *rows,
                                      HYPRE_Int ncols, HYPRE_Int *cols,
                                      int diag, HYPRE_Int* col_remap)
{
   HYPRE_Int  i, j, k, col;
   HYPRE_Int  A_beg, Ae_beg, A_end;
   HYPRE_Real a;

   HYPRE_Int  *A_i     = hypre_CSRMatrixI(A);
   HYPRE_Int  *A_j     = hypre_CSRMatrixJ(A);
   HYPRE_Real *A_data  = hypre_CSRMatrixData(A);
   HYPRE_Int   A_rows  = hypre_CSRMatrixNumRows(A);

   HYPRE_Int  *Ae_i    = hypre_CSRMatrixI(Ae);
   HYPRE_Int  *Ae_j    = hypre_CSRMatrixJ(Ae);
   HYPRE_Real *Ae_data = hypre_CSRMatrixData(Ae);

   for (i = 0; i < A_rows; i++)
   {
      A_beg = A_i[i];
      A_end = A_i[i+1];
      Ae_beg = Ae_i[i];

      if (hypre_BinarySearch(rows, i, nrows) >= 0)
      {
         /* eliminate row */
         for (j = A_beg, k = Ae_beg; j < A_end; j++, k++)
         {
            col = A_j[j];
            Ae_j[k] = col_remap ? col_remap[col] : col;
            a = (diag && col == i) ? 1.0 : 0.0;
            Ae_data[k] = A_data[j] - a;
            A_data[j] = a;
         }
      }
      else
      {
         /* eliminate columns */
         for (j = A_beg, k = Ae_beg; j < A_end; j++)
         {
            col = A_j[j];
            if (hypre_BinarySearch(cols, col, ncols) >= 0)
            {
               Ae_j[k] = col_remap ? col_remap[col] : col;
               Ae_data[k] = A_data[j];
               A_data[j] = 0.0;
               k++;
            }
         }
      }
   }
}


/*
  Function:  hypre_ParCSRMatrixEliminateAAe

                    (input)                  (output)

                / A_ii | A_ib \          / A_ii |  0   \
            A = | -----+----- |   --->   | -----+----- |
                \ A_bi | A_bb /          \   0  |  I   /


                                         /   0  |   A_ib   \
                                    Ae = | -----+--------- |
                                         \ A_bi | A_bb - I /

*/
void hypre_ParCSRMatrixEliminateAAe(hypre_ParCSRMatrix *A,
                                    hypre_ParCSRMatrix **Ae,
                                    HYPRE_Int num_rowscols_to_elim,
                                    HYPRE_Int *rowscols_to_elim)
{
   HYPRE_Int i, j, k;

   hypre_CSRMatrix *A_diag = hypre_ParCSRMatrixDiag(A);
   hypre_CSRMatrix *A_offd = hypre_ParCSRMatrixOffd(A);
   HYPRE_Int A_diag_nrows  = hypre_CSRMatrixNumRows(A_diag);
   HYPRE_Int A_offd_ncols  = hypre_CSRMatrixNumCols(A_offd);

   *Ae = hypre_ParCSRMatrixCreate(hypre_ParCSRMatrixComm(A),
                                  hypre_ParCSRMatrixGlobalNumRows(A),
                                  hypre_ParCSRMatrixGlobalNumCols(A),
                                  hypre_ParCSRMatrixRowStarts(A),
                                  hypre_ParCSRMatrixColStarts(A),
                                  0, 0, 0);

   hypre_ParCSRMatrixSetRowStartsOwner(*Ae, 0);
   hypre_ParCSRMatrixSetColStartsOwner(*Ae, 0);

   hypre_CSRMatrix *Ae_diag = hypre_ParCSRMatrixDiag(*Ae);
   hypre_CSRMatrix *Ae_offd = hypre_ParCSRMatrixOffd(*Ae);
   HYPRE_Int Ae_offd_ncols;

   HYPRE_Int  num_offd_cols_to_elim;
   HYPRE_Int  *offd_cols_to_elim;

   HYPRE_Int  *A_col_map_offd = hypre_ParCSRMatrixColMapOffd(A);
   HYPRE_Int  *Ae_col_map_offd;

   HYPRE_Int  *col_mark;
   HYPRE_Int  *col_remap;

   /* figure out which offd cols should be eliminated */
   {
      hypre_ParCSRCommHandle *comm_handle;
      hypre_ParCSRCommPkg *comm_pkg;
      HYPRE_Int num_sends, *int_buf_data;
      HYPRE_Int index, start;

      HYPRE_Int *eliminate_row = hypre_CTAlloc(HYPRE_Int, A_diag_nrows);
      HYPRE_Int *eliminate_col = hypre_CTAlloc(HYPRE_Int, A_offd_ncols);

      /* make sure A has a communication package */
      comm_pkg = hypre_ParCSRMatrixCommPkg(A);
      if (!comm_pkg)
      {
         hypre_MatvecCommPkgCreate(A);
         comm_pkg = hypre_ParCSRMatrixCommPkg(A);
      }

      /* which of the local rows are to be eliminated */
      for (i = 0; i < A_diag_nrows; i++)
      {
         eliminate_row[i] = 0;
      }
      for (i = 0; i < num_rowscols_to_elim; i++)
      {
         eliminate_row[rowscols_to_elim[i]] = 1;
      }

      /* use a Matvec communication pattern to find (in eliminate_col)
         which of the local offd columns are to be eliminated */
      num_sends = hypre_ParCSRCommPkgNumSends(comm_pkg);
      int_buf_data = hypre_CTAlloc(HYPRE_Int,
                                   hypre_ParCSRCommPkgSendMapStart(comm_pkg,
                                                                   num_sends));
      index = 0;
      for (i = 0; i < num_sends; i++)
      {
         start = hypre_ParCSRCommPkgSendMapStart(comm_pkg, i);
         for (j = start; j < hypre_ParCSRCommPkgSendMapStart(comm_pkg, i+1); j++)
         {
            k = hypre_ParCSRCommPkgSendMapElmt(comm_pkg, j);
            int_buf_data[index++] = eliminate_row[k];
         }
      }
      comm_handle = hypre_ParCSRCommHandleCreate(11, comm_pkg,
                                                 int_buf_data, eliminate_col);

      /* eliminate diagonal part, overlapping it with communication */
      hypre_CSRMatrixElimCreate(A_diag, Ae_diag,
                                num_rowscols_to_elim, rowscols_to_elim,
                                num_rowscols_to_elim, rowscols_to_elim,
                                NULL);

      hypre_CSRMatrixEliminateRowsCols(A_diag, Ae_diag,
                                       num_rowscols_to_elim, rowscols_to_elim,
                                       num_rowscols_to_elim, rowscols_to_elim,
                                       1, NULL);
      hypre_CSRMatrixReorder(Ae_diag);

      /* finish the communication */
      hypre_ParCSRCommHandleDestroy(comm_handle);

      /* received eliminate_col[], count offd columns to eliminate */
      num_offd_cols_to_elim = 0;
      for (i = 0; i < A_offd_ncols; i++)
      {
         if (eliminate_col[i]) { num_offd_cols_to_elim++; }
      }

      offd_cols_to_elim = hypre_CTAlloc(HYPRE_Int, num_offd_cols_to_elim);

      /* get a list of offd column indices and coefs */
      num_offd_cols_to_elim = 0;
      for (i = 0; i < A_offd_ncols; i++)
      {
         if (eliminate_col[i])
         {
            offd_cols_to_elim[num_offd_cols_to_elim++] = i;
         }
      }

      hypre_TFree(int_buf_data);
      hypre_TFree(eliminate_row);
      hypre_TFree(eliminate_col);
   }

   /* eliminate the off-diagonal part */
   col_mark = hypre_CTAlloc(HYPRE_Int, A_offd_ncols);
   col_remap = hypre_CTAlloc(HYPRE_Int, A_offd_ncols);

   hypre_CSRMatrixElimCreate(A_offd, Ae_offd,
                             num_rowscols_to_elim, rowscols_to_elim,
                             num_offd_cols_to_elim, offd_cols_to_elim,
                             col_mark);

   for (i = k = 0; i < A_offd_ncols; i++)
   {
      if (col_mark[i]) { col_remap[i] = k++; }
   }

   hypre_CSRMatrixEliminateRowsCols(A_offd, Ae_offd,
                                    num_rowscols_to_elim, rowscols_to_elim,
                                    num_offd_cols_to_elim, offd_cols_to_elim,
                                    0, col_remap);

   /* create col_map_offd for Ae */
   Ae_offd_ncols = 0;
   for (i = 0; i < A_offd_ncols; i++)
   {
      if (col_mark[i]) { Ae_offd_ncols++; }
   }

   Ae_col_map_offd  = hypre_CTAlloc(HYPRE_Int, Ae_offd_ncols);

   Ae_offd_ncols = 0;
   for (i = 0; i < A_offd_ncols; i++)
   {
      if (col_mark[i])
      {
         Ae_col_map_offd[Ae_offd_ncols++] = A_col_map_offd[i];
      }
   }

   hypre_ParCSRMatrixColMapOffd(*Ae) = Ae_col_map_offd;
   hypre_CSRMatrixNumCols(Ae_offd) = Ae_offd_ncols;

   hypre_TFree(col_remap);
   hypre_TFree(col_mark);
   hypre_TFree(offd_cols_to_elim);

   hypre_ParCSRMatrixSetNumNonzeros(*Ae);
   hypre_MatvecCommPkgCreate(*Ae);
}


/*--------------------------------------------------------------------------
 *                               Split
 *--------------------------------------------------------------------------*/

void hypre_CSRMatrixSplit(hypre_CSRMatrix *A,
                          HYPRE_Int nr, HYPRE_Int nc,
                          HYPRE_Int *row_block_num, HYPRE_Int *col_block_num,
                          hypre_CSRMatrix **blocks)
{
   HYPRE_Int i, j, k, bi, bj;

   HYPRE_Int* A_i = hypre_CSRMatrixI(A);
   HYPRE_Int* A_j = hypre_CSRMatrixJ(A);
   HYPRE_Complex* A_data = hypre_CSRMatrixData(A);

   HYPRE_Int A_rows = hypre_CSRMatrixNumRows(A);
   HYPRE_Int A_cols = hypre_CSRMatrixNumCols(A);

   HYPRE_Int *num_rows = hypre_CTAlloc(HYPRE_Int, nr);
   HYPRE_Int *num_cols = hypre_CTAlloc(HYPRE_Int, nc);

   HYPRE_Int *block_row = hypre_TAlloc(HYPRE_Int, A_rows);
   HYPRE_Int *block_col = hypre_TAlloc(HYPRE_Int, A_cols);

   for (i = 0; i < A_rows; i++)
   {
      block_row[i] = num_rows[row_block_num[i]]++;
   }
   for (j = 0; j < A_cols; j++)
   {
      block_col[j] = num_cols[col_block_num[j]]++;
   }

   /* allocate the blocks */
   for (i = 0; i < nr; i++)
   {
      for (j = 0; j < nc; j++)
      {
         hypre_CSRMatrix *B = hypre_CSRMatrixCreate(num_rows[i], num_cols[j], 0);
         hypre_CSRMatrixI(B) = hypre_CTAlloc(HYPRE_Int, num_rows[i] + 1);
         blocks[i*nc + j] = B;
      }
   }

   /* count block row nnz */
   for (i = 0; i < A_rows; i++)
   {
      bi = row_block_num[i];
      for (j = A_i[i]; j < A_i[i+1]; j++)
      {
         bj = col_block_num[A_j[j]];
         hypre_CSRMatrix *B = blocks[bi*nc + bj];
         hypre_CSRMatrixI(B)[block_row[i] + 1]++;
      }
   }

   /* count block nnz */
   for (k = 0; k < nr*nc; k++)
   {
      hypre_CSRMatrix *B = blocks[k];
      HYPRE_Int* B_i = hypre_CSRMatrixI(B);

      HYPRE_Int nnz = 0, rs;
      for (int k = 1; k <= hypre_CSRMatrixNumRows(B); k++)
      {
         rs = B_i[k], B_i[k] = nnz, nnz += rs;
      }

      hypre_CSRMatrixJ(B) = hypre_TAlloc(HYPRE_Int, nnz);
      hypre_CSRMatrixData(B) = hypre_TAlloc(HYPRE_Complex, nnz);
      hypre_CSRMatrixNumNonzeros(B) = nnz;
   }

   /* populate blocks */
   for (i = 0; i < A_rows; i++)
   {
      bi = row_block_num[i];
      for (j = A_i[i]; j < A_i[i+1]; j++)
      {
         k = A_j[j];
         bj = col_block_num[k];
         hypre_CSRMatrix *B = blocks[bi*nc + bj];
         HYPRE_Int *bii = hypre_CSRMatrixI(B) + block_row[i] + 1;
         hypre_CSRMatrixJ(B)[*bii] = block_col[k];
         hypre_CSRMatrixData(B)[*bii] = A_data[j];
         (*bii)++;
      }
   }

   hypre_TFree(block_col);
   hypre_TFree(block_row);

   hypre_TFree(num_cols);
   hypre_TFree(num_rows);
}


void hypre_ParCSRMatrixSplit(hypre_ParCSRMatrix *A,
                             HYPRE_Int nr, HYPRE_Int nc,
                             hypre_ParCSRMatrix **blocks,
                             int interleaved)
{
   HYPRE_Int i, j, k;

   MPI_Comm comm = hypre_ParCSRMatrixComm(A);

   hypre_CSRMatrix *Adiag = hypre_ParCSRMatrixDiag(A);
   hypre_CSRMatrix *Aoffd = hypre_ParCSRMatrixOffd(A);

   HYPRE_Int global_rows = hypre_ParCSRMatrixGlobalNumRows(A);
   HYPRE_Int global_cols = hypre_ParCSRMatrixGlobalNumCols(A);

   HYPRE_Int local_rows = hypre_CSRMatrixNumRows(Adiag);
   HYPRE_Int local_cols = hypre_CSRMatrixNumCols(Adiag);
   HYPRE_Int offd_cols = hypre_CSRMatrixNumCols(Aoffd);

   hypre_assert(local_rows % nr == 0 && local_cols % nc == 0);
   hypre_assert(global_rows % nr == 0 && global_cols % nc == 0);

   HYPRE_Int block_rows = local_rows / nr;
   HYPRE_Int block_cols = local_cols / nc;
   HYPRE_Int num_blocks = nr * nc;

   /* mark local rows and columns with block number */
   HYPRE_Int *row_block_num = hypre_TAlloc(HYPRE_Int, local_rows);
   HYPRE_Int *col_block_num = hypre_TAlloc(HYPRE_Int, local_cols);

   for (i = 0; i < local_rows; i++)
   {
      row_block_num[i] = interleaved ? (i % nr) : (i / block_rows);
   }
   for (i = 0; i < local_cols; i++)
   {
      col_block_num[i] = interleaved ? (i % nc) : (i / block_cols);
   }

   /* determine the block numbers for offd columns */
   HYPRE_Int* offd_col_block_num = hypre_TAlloc(HYPRE_Int, offd_cols);
   hypre_ParCSRCommHandle *comm_handle;
   HYPRE_Int *int_buf_data;
   {
      /* make sure A has a communication package */
      hypre_ParCSRCommPkg *comm_pkg = hypre_ParCSRMatrixCommPkg(A);
      if (!comm_pkg)
      {
         hypre_MatvecCommPkgCreate(A);
         comm_pkg = hypre_ParCSRMatrixCommPkg(A);
      }

      /* calculate the final global column numbers for each block */
      HYPRE_Int *count = hypre_CTAlloc(HYPRE_Int, nc);
      HYPRE_Int *block_global_col = hypre_TAlloc(HYPRE_Int, local_cols);
      HYPRE_Int first_col = hypre_ParCSRMatrixFirstColDiag(A) / nc;
      for (i = 0; i < local_cols; i++)
      {
         block_global_col[i] = first_col + count[col_block_num[i]]++;
      }
      hypre_TFree(count);

      /* use a Matvec communication pattern to determine offd_col_block_num */
      HYPRE_Int num_sends = hypre_ParCSRCommPkgNumSends(comm_pkg);
      int_buf_data = hypre_CTAlloc(HYPRE_Int,
                                   hypre_ParCSRCommPkgSendMapStart(comm_pkg,
                                                                   num_sends));
      HYPRE_Int start, index = 0;
      for (i = 0; i < num_sends; i++)
      {
         start = hypre_ParCSRCommPkgSendMapStart(comm_pkg, i);
         for (j = start; j < hypre_ParCSRCommPkgSendMapStart(comm_pkg, i+1); j++)
         {
            k = hypre_ParCSRCommPkgSendMapElmt(comm_pkg, j);
            int_buf_data[index++] = col_block_num[k] + nc*block_global_col[k];
         }
      }
      hypre_TFree(block_global_col);

      comm_handle = hypre_ParCSRCommHandleCreate(11, comm_pkg, int_buf_data,
                                                 offd_col_block_num);
   }

   /* create the block matrices */
   HYPRE_Int num_procs = 1;
   if (!hypre_ParCSRMatrixAssumedPartition(A))
   {
      hypre_MPI_Comm_size(comm, &num_procs);
   }

   HYPRE_Int *row_starts = hypre_TAlloc(HYPRE_Int, num_procs+1);
   HYPRE_Int *col_starts = hypre_TAlloc(HYPRE_Int, num_procs+1);
   for (i = 0; i <= num_procs; i++)
   {
      row_starts[i] = hypre_ParCSRMatrixRowStarts(A)[i] / nr;
      col_starts[i] = hypre_ParCSRMatrixColStarts(A)[i] / nc;
   }

   for (i = 0; i < num_blocks; i++)
   {
      blocks[i] = hypre_ParCSRMatrixCreate(comm,
                                           global_rows/nr, global_cols/nc,
                                           row_starts, col_starts, 0, 0, 0);
   }

   /* split diag part */
   hypre_CSRMatrix **csr_blocks = hypre_TAlloc(hypre_CSRMatrix*, nr*nc);
   hypre_CSRMatrixSplit(Adiag, nr, nc, row_block_num, col_block_num,
                        csr_blocks);

   for (i = 0; i < num_blocks; i++)
   {
      hypre_TFree(hypre_ParCSRMatrixDiag(blocks[i]));
      hypre_ParCSRMatrixDiag(blocks[i]) = csr_blocks[i];
   }

   /* finish communication, receive offd_col_block_num */
   hypre_ParCSRCommHandleDestroy(comm_handle);
   hypre_TFree(int_buf_data);

   /* decode global offd column numbers */
   HYPRE_Int* offd_global_col = hypre_TAlloc(HYPRE_Int, offd_cols);
   for (i = 0; i < offd_cols; i++)
   {
      offd_global_col[i] = offd_col_block_num[i] / nc;
      offd_col_block_num[i] %= nc;
   }

   /* split offd part */
   hypre_CSRMatrixSplit(Aoffd, nr, nc, row_block_num, offd_col_block_num,
                        csr_blocks);

   for (i = 0; i < num_blocks; i++)
   {
      hypre_TFree(hypre_ParCSRMatrixOffd(blocks[i]));
      hypre_ParCSRMatrixOffd(blocks[i]) = csr_blocks[i];
   }

   hypre_TFree(csr_blocks);
   hypre_TFree(col_block_num);
   hypre_TFree(row_block_num);

   /* update block col-maps */
   for (int bi = 0; bi < nr; bi++)
   {
      for (int bj = 0; bj < nc; bj++)
      {
         hypre_ParCSRMatrix *block = blocks[bi*nc + bj];
         hypre_CSRMatrix *block_offd = hypre_ParCSRMatrixOffd(block);
         HYPRE_Int block_offd_cols = hypre_CSRMatrixNumCols(block_offd);

         HYPRE_Int *block_col_map = hypre_TAlloc(HYPRE_Int, block_offd_cols);
         for (i = j = 0; i < offd_cols; i++)
         {
            HYPRE_Int bn = offd_col_block_num[i];
            if (bn == bj) { block_col_map[j++] = offd_global_col[i]; }
         }
         hypre_assert(j == block_offd_cols);

         hypre_ParCSRMatrixColMapOffd(block) = block_col_map;
      }
   }

   hypre_TFree(offd_global_col);
   hypre_TFree(offd_col_block_num);

   /* finish the new matrices, make them own all the stuff */
   for (i = 0; i < num_blocks; i++)
   {
      hypre_ParCSRMatrixSetNumNonzeros(blocks[i]);
      hypre_MatvecCommPkgCreate(blocks[i]);

      hypre_ParCSRMatrixOwnsData(blocks[i]) = 1;
      hypre_ParCSRMatrixOwnsRowStarts(blocks[i]) = !i;
      hypre_ParCSRMatrixOwnsColStarts(blocks[i]) = !i;
   }
}

}

} // namespace mfem::internal

#endif // MFEM_USE_MPI
