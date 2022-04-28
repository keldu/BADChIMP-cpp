//=====================================================================================
//                      Q U E M A D A   R H E O L O G Y 
//
//                      A N N E L U S   G E O M E T R Y
//
//=====================================================================================

#include "../LBSOLVER.h"
#include "../IO.h"
#include "LBsixdof.h"
#include "LBrehology.h"
// SET THE LATTICE TYPE
#define LT D3Q19

int main()
{
    //---------------------------------------------------------------------------------
    //                                   Setup 
    //--------------------------------------------------------------------------------- 

    //                                   Setup 
    //--------------------------------------------------------------------------------- mpi
    MPI_Init(NULL, NULL);
    int nProcs;
    MPI_Comm_size(MPI_COMM_WORLD, &nProcs);
    int myRank;
    MPI_Comm_rank(MPI_COMM_WORLD, &myRank);

    //                                   Setup 
    //--------------------------------------------------------------------------------- input output paths
    std::string chimpDir = "/home/AD.NORCERESEARCH.NO/esje/Programs/GitHub/BADCHiMP/";
    std::string mpiDir = chimpDir + "input/mpi/";
    std::string inputDir = chimpDir + "input/";
    std::string outputDir = chimpDir + "output/";

    //                                   Setup 
    //--------------------------------------------------------------------------------- grid and geometry objects
    Input input(inputDir + "input.dat");
    LBvtk<LT> vtklb(mpiDir + "tmp" + std::to_string(myRank) + ".vtklb");
    Grid<LT> grid(vtklb);
    Nodes<LT> nodes(vtklb, grid);
    BndMpi<LT> mpiBoundary(vtklb, nodes, grid);
    //--------------------------------------------------------------------------------- Set bulk nodes
    std::vector<int> bulkNodes = findBulkNodes(nodes);


    //                                   Setup 
    //--------------------------------------------------------------------------------- read input-file
    // Number of iterations
    int nIterations = input["iterations"]["max"];
    // Write interval
    int nItrWrite = input["iterations"]["write"];
    // Relaxation time
    lbBase_t tau = input["fluid"]["tau"];


    //---------------------------------------------------------------------------------
    //                               Physical system 
    //--------------------------------------------------------------------------------- 

    //                               Physical system 
    //--------------------------------------------------------------------------------- Rheology object
    QuemadaLawRheology<LT> quemada(inputDir + "test.dat", 0.0014133);
    //                               Physical system 
    //--------------------------------------------------------------------------------- indicator field
    ScalarField phi(1, grid.size());
    vtklb.toAttribute("phi");
    for (int nodeNo = vtklb.beginNodeNo(); nodeNo < vtklb.endNodeNo(); ++nodeNo) 
    {
        const lbBase_t val = vtklb.getScalarAttribute<lbBase_t>();
        phi(0, nodeNo) = val;
    }
    //--------------------------------------------------------------------------------- position relative to origo
    VectorField<LT> pos(1, grid.size());
    vtklb.toAttribute("x0");
    for (int nodeNo = vtklb.beginNodeNo(); nodeNo < vtklb.endNodeNo(); ++nodeNo) 
    {
        const lbBase_t val = vtklb.getScalarAttribute<lbBase_t>();
        pos(0, 0, nodeNo) = val;
        pos(0, 2, nodeNo) = 0;
    }
    vtklb.toAttribute("y0");
    for (int nodeNo = vtklb.beginNodeNo(); nodeNo < vtklb.endNodeNo(); ++nodeNo) 
    {
        const lbBase_t val = vtklb.getScalarAttribute<lbBase_t>();
        pos(0, 1, nodeNo) = val;
    }

    //--------------------------------------------------------------------------------- viscosity
    ScalarField viscosity(1, grid.size());
    //--------------------------------------------------------------------------------- rho
    ScalarField rho(1, grid.size());
    for (int n=vtklb.beginNodeNo(); n < vtklb.endNodeNo(); ++n) {
        rho(0, n) = 1.0;
    } 
    //--------------------------------------------------------------------------------- velocity
    VectorField<LT> vel(1, grid.size());
    // Initiate velocity
    for (auto nodeNo: bulkNodes) {
        for (int d=0; d < LT::nD; ++d)
            vel(0, d, nodeNo) = 0.0;
    }
    //--------------------------------------------------------------------------------- force
    VectorField<LT> force(1, grid.size());

    //                               Physical system 
    //--------------------------------------------------------------------------------- boundary conditions
    InterpolatedBounceBackBoundary<LT> ippBnd(findFluidBndNodes(nodes), nodes, grid);
    VectorField<LT> solidFluidForce(1, grid.size());

    //--------------------------------------------------------------------------------- lb fields
    LbField<LT> f(1, grid.size()); 
    LbField<LT> fTmp(1, grid.size());
    // initiate lb distributions
    for (auto nodeNo: bulkNodes) {
        for (int q = 0; q < LT::nQ; ++q) {
            f(0, q, nodeNo) = LT::w[q]*rho(0, nodeNo);
        }
    }

    //                               Output  
    //--------------------------------------------------------------------------------- vtk output
    Output<LT> output(grid, bulkNodes, outputDir, myRank, nProcs);
    output.add_file("lb_run_annulus");
    //output.add_file("lb_run");
    output.add_scalar_variables({"phi", "rho", "viscosity"}, {phi, rho, viscosity});
    output.add_vector_variables({"vel", "bodyforce", "solidforce", "pos"}, {vel, force, solidFluidForce, pos});
    

    //==================================================================================
    //                                  M A I N   L O O P  
    //==================================================================================
    std::ofstream writeDeltaP;
    std::ofstream writeSurfaceForce;
    if (myRank == 0 ) {
        writeDeltaP.open(outputDir + "deltaP.dat");
        writeSurfaceForce.open(outputDir + "surfaceforce.dat");
    }

    std::valarray<lbBase_t> surfaceForce(LT::nD);

    lbBase_t u_mean = 0;
    lbBase_t omega = 0; //1.0e-3;
    lbBase_t alpha = 0.0;
    const std::time_t beginLoop = std::time(NULL);
    for (int i = 0; i <= nIterations; i++) {
        //                              main loop 
        //----------------------------------------------------------------------------- flux correction force - initiation
        lbBase_t uxLocal = 0.0;
        for (auto nodeNo: bulkNodes) {
            auto cf = LT::qSumC(f(0, nodeNo));
            uxLocal += cf[2];
        }
        lbBase_t uxGlobal;
        MPI_Allreduce(&uxLocal, &uxGlobal, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        lbBase_t numNodesLocal = bulkNodes.size();
        lbBase_t numNodesGlobal;
        MPI_Allreduce(&numNodesLocal, &numNodesGlobal, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);  

        //----------------------------------------------------------------------------- set flux
        const int Ninit = 1000;
        const lbBase_t u_low = 0.01;
        
        if (i < Ninit) {
            const lbBase_t my_pi = 3.14159265358979323;
            u_mean = 0.5*u_low*(1 - std::cos(i*my_pi/(1.0*Ninit)));
        }

        //----------------------------------------------------------------------------- set flux correction force
        lbBase_t forceX = 2*(u_mean*numNodesGlobal - uxGlobal)/numNodesGlobal;     
        if (myRank == 0) {
            writeDeltaP << u_mean << " " << forceX << std::endl;
        }

        //                               main loop 
        //----------------------------------------------------------------------------- Begin node loop
        for (auto nodeNo: bulkNodes) {
            const std::valarray<lbBase_t> fNode = f(0, nodeNo);
            const std::valarray<lbBase_t> fluxCorrectionForce = {0, 0, forceX};
            //force.set(0, nodeNo) = forceNode;

            //------------------------------------------------------------------------- rho 
            const lbBase_t rhoNode = calcRho<LT>(fNode);
            rho(0, nodeNo) = rhoNode;
            // pressure(0, nodeNo) = LT::c2*rhoNode + deltaP*laplacePressure(0, nodeNo);
            //------------------------------------------------------------------------- velocity 
            //const auto velNode = calcVel<LT>(fNode, rhoNode, forceNode);
            const std::valarray<lbBase_t> posNode = pos(0, nodeNo);
            const auto velNode = calcVelRot<LT>(fNode, rhoNode, posNode, fluxCorrectionForce, omega, alpha);
            vel.set(0, nodeNo) = velNode;
            //------------------------------------------------------------------------- force 
            const std::valarray<lbBase_t> forceNode = pseudoForce(posNode, velNode, omega, alpha) + fluxCorrectionForce;
            force.set(0,nodeNo) = forceNode;
            //                           main loop 
            //------------------------------------------------------------------------- BGK-collision term
            const lbBase_t u2 = LT::dot(velNode, velNode);
            const std::valarray<lbBase_t> cu = LT::cDotAll(velNode);
            //------------------------------------------------------------------------- tau
            // auto omegaBGK = carreau.omegaBGK(fNode, rhoNode, velNode, u2, cu, forceNode, 0);
            // tau = powerLaw.tau(fNode, rhoNode, velNode, u2, cu, forceNode);
            // tau = quemada.tau(fNode, rhoNode, velNode, u2, cu, forceNode);
            tau = 0.8;
            viscosity(0, nodeNo) = tau;
            auto omegaBGK = calcOmegaBGK<LT>(fNode, tau, rhoNode, u2, cu);
            //------------------------------------------------------------------------- Guo-force correction
            const lbBase_t uF = LT::dot(velNode, forceNode);
            const std::valarray<lbBase_t> cF = LT::cDotAll(forceNode);
            //tau = carreau.tau();
            const std::valarray<lbBase_t> deltaOmegaF = calcDeltaOmegaF<LT>(tau, cu, uF, cF);

            //------------------------------------------------------------------------- Collision and propagation
            fTmp.propagateTo(0, nodeNo, fNode + omegaBGK + deltaOmegaF, grid);

        } //--------------------------------------------------------------------------- End node loop
     
        //                               main loop 
        //----------------------------------------------------------------------------- Swap fTmp and f
        f.swapData(fTmp);  
        //                            bondary conditions
        //----------------------------------------------------------------------------- mpi
        mpiBoundary.communicateLbField(0, f, grid);
        //----------------------------------------------------------------------------- inlet, outlet and solid
        // bounceBackBnd.apply(f, grid);
        //ippBnd.apply(phi, 0, f, grid);
        surfaceForce = ippBnd.apply(phi, 0, f, grid, solidFluidForce);
        lbBase_t FzLocal;
        lbBase_t FzGlobal;
        MPI_Allreduce(&FzLocal, &FzGlobal, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

        if (myRank == 0) {
            writeSurfaceForce << FzGlobal << std::endl;
        }
        // abbBnd.apply(0, f, vel, pind, grid);
        /*wetBoundary.applyBoundaryCondition(0, f, force, grid);
        lbBase_t deltaMass  = 0.0;
        for (const auto & nodeNo: bulkNodes) {
            deltaMass += 1 - LT::qSum(f(0, nodeNo));
        }
        wetBoundary.addMassToWallBoundary(0, f, force, deltaMass);
        */

        //                               main loop 
        //----------------------------------------------------------------------------- write to file
        if ( ((i % nItrWrite) == 0)  ) {
            output.write(i);
            
            if (myRank==0) {
                std::cout << "PLOT AT ITERATION : " << i << std::endl;
            }
        }

    } //------------------------------------------------------------------------------- End iterations
    if(myRank == 0) {
        std::cout << " RUNTIME = " << std::time(NULL) - beginLoop << std::endl;
    }
    writeDeltaP.close();
    writeSurfaceForce.close();

    MPI_Finalize();
    return 0;
}
