//                                MFEM Example 1
//
// Compile with: make ex1
//
// Sample runs:  ex1 ../data/square-disc.mesh
//               ex1 ../data/star.mesh
//               ex1 ../data/escher.mesh
//               ex1 ../data/fichera.mesh
//               ex1 ../data/square-disc-p2.vtk
//               ex1 ../data/square-disc-p3.mesh
//               ex1 ../data/square-disc-nurbs.mesh
//               ex1 ../data/disc-nurbs.mesh
//               ex1 ../data/pipe-nurbs.mesh
//
// Description:  This example code demonstrates the use of MFEM to define a
//               simple isoparametric finite element discretization of the
//               Laplace problem -Delta u = 1 with homogeneous Dirichlet
//               boundary conditions. Specifically, we discretize with the
//               FE space coming from the mesh (linear by default, quadratic
//               for quadratic curvilinear mesh, NURBS for NURBS mesh, etc.)
//
//               The example highlights the use of mesh refinement, finite
//               element grid functions, as well as linear and bilinear forms
//               corresponding to the left-hand side and right-hand side of the
//               discrete linear system. We also cover the explicit elimination
//               of boundary conditions on all boundary edges, and the optional
//               connection to the GLVis tool for visualization.

#include <fstream>
#include "mfem.hpp"

#include "sight.h"
#include "mfem.h"
using namespace sight;

int main (int argc, char *argv[])
{
   Mesh *mesh;

   SightInit(argc, argv, "ex1", "dbg.MFEM.ex1");

   if (argc <= 3)
   {
      cerr << "\nUsage: ex1 <mesh_file> ref_levels finElement\n" << endl;
      return 1;
   }
   char* meshFile = argv[1];
   int ref_levels = atoi(argv[2]);
   char* finElement = argv[3];
   
   modularApp mfemApp("MFEM App", namedMeasures("time", new timeMeasure())); 
   std::vector<port> externalOutputs;
   compModule mod(instance("Ex1", 1, 1), inputs(port(context(config("meshFile", meshFile)))),
                  externalOutputs,
                  ref_levels=0,
                  context(config("ref_levels", ref_levels,
                                 "finElement", finElement)));
   mod.setOutCtxt(0, context(config("result", ref_levels)));
   
   return 0;
   /*
   // 1. Read the mesh from the given mesh file. We can handle triangular,
   //    quadrilateral, tetrahedral or hexahedral elements with the same code.
   ifstream imesh(meshFile);
   if (!imesh)
   {
      cerr << "\nCan not open mesh file: " << argv[1] << '\n' << endl;
      return 2;
   }
   mesh = new Mesh(imesh, 1, 1);
   imesh.close();

   // 2. Refine the mesh to increase the resolution. In this example we do
   //    'ref_levels' of uniform refinement. We choose 'ref_levels' to be the
   //    largest number that gives a final mesh with no more than 50,000
   //    elements.
   {
      /*int ref_levels =
         (int)floor(log(50000./mesh->GetNE())/log(2.)/mesh->Dimension());* /
      for (int l = 0; l < ref_levels; l++)
         mesh->UniformRefinement();
   }

   // 3. Define a finite element space on the mesh. Here we use isoparametric
   //    finite elements coming from the mesh nodes (linear by default).
   FiniteElementCollection *fec;
   /*if (mesh->GetNodes())
      fec = mesh->GetNodes()->OwnFEC();
   else* /
   if(strcmp(finElement, "linear")==0)
      fec = new LinearFECollection;
   else if(strcmp(finElement, "h1")
      fec = new H1_FECollection(p, dim);
   FiniteElementSpace *fespace = new FiniteElementSpace(mesh, fec);
   dbg << "Number of unknowns: " << fespace->GetVSize() << endl;

   // 4. Set up the linear form b(.) which corresponds to the right-hand side of
   //    the FEM linear system, which in this case is (1,phi_i) where phi_i are
   //    the basis functions in the finite element fespace.
   LinearForm *b = new LinearForm(fespace);
   ConstantCoefficient one(1.0);
   b->AddDomainIntegrator(new DomainLFIntegrator(one));
   b->Assemble();

   // 5. Define the solution vector x as a finite element grid function
   //    corresponding to fespace. Initialize x with initial guess of zero,
   //    which satisfies the boundary conditions.
   GridFunction x(fespace);
   x = 0.0;

   // 6. Set up the bilinear form a(.,.) on the finite element space
   //    corresponding to the Laplacian operator -Delta, by adding the Diffusion
   //    domain integrator and imposing homogeneous Dirichlet boundary
   //    conditions. The boundary conditions are implemented by marking all the
   //    boundary attributes from the mesh as essential (Dirichlet). After
   //    assembly and finalizing we extract the corresponding sparse matrix A.
   BilinearForm *a = new BilinearForm(fespace);
   a->AddDomainIntegrator(new DiffusionIntegrator(one));
   a->Assemble();
   Array<int> ess_bdr(mesh->bdr_attributes.Max());
   ess_bdr = 1;
   a->EliminateEssentialBC(ess_bdr, x, *b);
   a->Finalize();
   const SparseMatrix &A = a->SpMat();

   // 7. Define a simple symmetric Gauss-Seidel preconditioner and use it to
   //    solve the system Ax=b with PCG.
   GSSmoother M(A);
   PCG(A, M, *b, x, 1, 200, 1e-12, 0.0);

   // 8. Save the refined mesh and the solution. This output can be viewed later
   //    using GLVis: "glvis -m refined.mesh -g sol.gf".
   {
      ofstream mesh_ofs("refined.mesh");
      mesh_ofs.precision(8);
      mesh->Print(mesh_ofs);
      ofstream sol_ofs("sol.gf");
      sol_ofs.precision(8);
      x.Save(sol_ofs);
   }

   // 9. (Optional) Send the solution by socket to a GLVis server.
   /*char vishost[] = "localhost";
   int  visport   = 19916;
   osockstream sol_sock(visport, vishost);
   sol_sock << "solution\n";
   sol_sock.precision(8);
   mesh->Print(sol_sock);
   x.Save(sol_sock);
   sol_sock.send();* /
   mfem::emitMesh(mesh, &x);
   
   mod.setOutCtxt(0, context(config("error", posDev(particles, numDims),
                                               "numParticles", numParticles,
                                               "numDims", numDims)));

   // 10. Free the used memory.
   delete a;
   delete b;
   delete fespace;
   if (!mesh->GetNodes())
      delete fec;
   delete mesh;

   return 0;*/
}
