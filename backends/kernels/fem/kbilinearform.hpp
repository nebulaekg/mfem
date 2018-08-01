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

#ifndef MFEM_BACKENDS_KERNELS_BILINEAR_HPP
#define MFEM_BACKENDS_KERNELS_BILINEAR_HPP

#include "../../../config/config.hpp"
#if defined(MFEM_USE_BACKENDS) && defined(MFEM_USE_KERNELS)

namespace mfem
{

namespace kernels
{

enum KernelsIntegratorType
{
   DomainIntegrator       = 0,
   BoundaryIntegrator     = 1,
   InteriorFaceIntegrator = 2,
   BoundaryFaceIntegrator = 3
};

class KernelsIntegrator;

// *****************************************************************************
// * Class for bilinear form - "Matrix" with associated FE space and
// * BLFIntegrators.
// *****************************************************************************
class kBilinearForm : public Operator{
   friend class KernelsIntegrator;
protected:
   typedef std::vector<KernelsIntegrator*> IntegratorVector;
   SharedPtr<const Engine> ng;
   // State information
   mutable mfem::Mesh *mesh;
   mutable kFiniteElementSpace *otrialFESpace;
   mutable mfem::FiniteElementSpace *trialFESpace;
   mutable kFiniteElementSpace *otestFESpace;
   mutable mfem::FiniteElementSpace *testFESpace;
   IntegratorVector integrators;
   // The input and output vectors are mapped to local nodes for efficient
   // operations. In other words, they are E-vectors.
   // The size is: (number of elements) * (nodes in element) * (vector dim)
   mutable kernels::Vector localX, localY;
public:
   kBilinearForm(kFiniteElementSpace *ofespace_);
   kBilinearForm(kFiniteElementSpace*, kFiniteElementSpace*);
   void Init(const Engine&, kFiniteElementSpace*, kFiniteElementSpace*);
   const Engine &engine() const { return *ng; }
   device GetDevice(int idx = 0) const { return ng->GetDevice(idx); }

   // Useful mesh Information
   int BaseGeom() const { return mesh->GetElementBaseGeometry(); }
   int GetDim() const { return mesh->Dimension(); }
   int64_t GetNE() const{ return mesh->GetNE(); }
   mfem::Mesh& GetMesh() const { return *mesh; }

   kFiniteElementSpace& GetTrialKernelsFESpace() const;
   kFiniteElementSpace& GetTestKernelsFESpace() const;
   mfem::FiniteElementSpace& GetTrialFESpace() const;
   mfem::FiniteElementSpace& GetTestFESpace() const;

   // Useful FE information
   int64_t GetTrialNDofs() const;
   int64_t GetTestNDofs() const;
   int64_t GetTrialVDim() const;
   int64_t GetTestVDim() const;
   const mfem::FiniteElement& GetTrialFE(const int i) const;
   const mfem::FiniteElement& GetTestFE(const int i) const;

   // Adds new Domain Integrator.
   void AddDomainIntegrator(KernelsIntegrator*);
   // Adds new Boundary Integrator.
   void AddBoundaryIntegrator(KernelsIntegrator*);
   // Adds new interior Face Integrator.
   void AddInteriorFaceIntegrator(KernelsIntegrator*);
   // Adds new boundary Face Integrator.
   void AddBoundaryFaceIntegrator(KernelsIntegrator*);
   // Adds Integrator based on KernelsIntegratorType
   void AddIntegrator(KernelsIntegrator*, const KernelsIntegratorType );

   virtual const mfem::Operator *GetTrialProlongation() const;
   virtual const mfem::Operator *GetTestProlongation() const;

   virtual const mfem::Operator *GetTrialRestriction() const;
   virtual const mfem::Operator *GetTestRestriction() const;

   // Assembles the form i.e. sums over all domain/bdr integrators.
   virtual void Assemble();

   void FormLinearSystem(const mfem::Array<int> &constraintList,
                         mfem::Vector &x, mfem::Vector &b,
                         mfem::Operator *&Aout,
                         mfem::Vector &X, mfem::Vector &B,
                         int copy_interior = 0);

   void FormOperator(const mfem::Array<int> &constraintList,
                     mfem::Operator *&Aout);

   void InitRHS(const mfem::Array<int> &constraintList,
                mfem::Vector &x, mfem::Vector &b,
                mfem::Operator *Aout,
                mfem::Vector &X, mfem::Vector &B,
                int copy_interior = 0);

   // overrides
   virtual void Mult_(const Vector &x, Vector &y) const;
   virtual void MultTranspose_(const Vector &x, Vector &y) const;

   void KernelsRecoverFEMSolution(const mfem::Vector &X, const mfem::Vector &b,
                                  mfem::Vector &x);

   // Destroys bilinear form.
   ~kBilinearForm();
};

} // namespace mfem::kernels

} // namespace mfem


#endif // defined(MFEM_USE_BACKENDS) && defined(MFEM_USE_KERNELS)

#endif // MFEM_BACKENDS_KERNELS_BILINEAR_HPP