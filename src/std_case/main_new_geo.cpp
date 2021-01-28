// //////////////////////////////////////////////
//
// TWO PHASE SOLVER
//
//
// Using the color gradient method (Rothman-Keller
//  type) with the Reis Phillips surface pertubation
//  and / Latva-Kokko recolor step. (As suggested
//  by Leclaire et al and Yu-Hang Fu et al)
//
// This is an explicit two-phase implementation. A
// full multiphase implementation will be added later.
//
// TO RUN PROGRAM: type "mpirun -np <#procs> badchimpp" in command
// line in main directory
//
// //////////////////////////////////////////////

#include <sstream>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <algorithm> 

#include "../lbsolver/LBbndmpi.h"
#include "../lbsolver/LBboundary.h"
#include "../lbsolver/LBcollision.h"
#include "../lbsolver/LBcollision2phase.h"
#include "../lbsolver/LBlatticetypes.h"
#include "../lbsolver/LBfield.h"
#include "../lbsolver/LBgeometry.h"
#include "../lbsolver/LBgrid.h"
#include "../lbsolver/LBhalfwaybb.h"
#include "../lbsolver/LBfreeSlipCartesian.h"
#include "../lbsolver/LBfreeFlowCartesian.h"
#include "../lbsolver/LBinitiatefield.h"
#include "../lbsolver/LBmacroscopic.h"
#include "../lbsolver/LBnodes.h"
#include "../lbsolver/LBsnippets.h"
#include "../lbsolver/LButilities.h"
#include "../lbsolver/LBpressurebnd.h"

#include "../io/Input.h"
#include "../io/Output.h"

#include "../lbsolver/LBvtk.h"

#include "../lbsolver/LBbounceback.h"
#include "../lbsolver/LBfreeslipsolid.h"

#include "../lbsolver/LBinletoutlet.h"


// SET THE LATTICE TYPE
#define LT D3Q19



int main()
{
    // *********
    // SETUP MPI
    // *********
    MPI_Init(NULL, NULL);
    int nProcs;
    MPI_Comm_size(MPI_COMM_WORLD, &nProcs);
    int myRank;
    MPI_Comm_rank(MPI_COMM_WORLD, &myRank);

    // ********************************
    // SETUP THE INPUT AND OUTPUT PATHS
    // ********************************
    std::string chimpDir = "./";
    std::string mpiDir = chimpDir + "input/mpi/";
    std::string inputDir = chimpDir + "input/";
    std::string outputDir = chimpDir + "output/";

    // ***********************
    // SETUP GRID AND GEOMETRY
    // ***********************
    Input input(inputDir + "input.dat");
    LBvtk<LT> vtklb(mpiDir + "tmp" + std::to_string(myRank) + ".vtklb");
    Grid<LT> grid(vtklb);
    Nodes<LT> nodes(vtklb, grid);
    BndMpi<LT> mpiBoundary(vtklb, nodes, grid);
    
    
    // ********
    // SETUP BOUNDARY
    // ********

    // -- Bounce Back
    HalfWayBounceBack<LT> fluidWallBnd(findBulkNodes(nodes), nodes, grid);

    // SETUP SOLID BOUNDARY
    std::vector<int> solidBnd = findSolidBndNodes(nodes);

    // SETUP BULK NODES
    std::vector<int> bulkNodes = findBulkNodes(nodes);
    
    // *************
    // SET LB VALUES
    // *************
    
    // Number of iterations
    int nIterations = static_cast<int>( input["iterations"]["max"]);
    // Write interval
    int nItrWrite = static_cast<int>( input["iterations"]["write"]);
    // Relaxation time
    lbBase_t tau = input["fluid"]["tau"][0];
    // Vector source
    VectorField<LT> bodyForce(1, 1);
    bodyForce.set(0, 0) = inputAsValarray<lbBase_t>(input["fluid"]["bodyforce"]);
    // Driver force
    std::valarray<lbBase_t> force = bodyForce(0, 0);

    //output directory
    std::string dirNum = std::to_string(static_cast<int>(input["out"]["directoryNum"]));
    std::string outDir2 = outputDir+"out"+dirNum;


    // ******************
    // MACROSCOPIC FIELDS
    // ******************
    // Density
    ScalarField rho(1, grid.size());
    // Velocity
    VectorField<LT> vel(1, grid.size());
    // Initiate values
    for (auto nodeNo: bulkNodes) {
        rho(0, nodeNo) = 1.0;
        for (int d=0; d < LT::nD; ++d)
            vel(0, d, nodeNo) = 0.0;
    }

    // *********
    // LB FIELDS
    // *********
    LbField<LT> f(1, grid.size());  // LBfield
    LbField<LT> fTmp(1, grid.size());  // LBfield
    // initieate values
    for (auto nodeNo: bulkNodes) {
        for (int q = 0; q < LT::nQ; ++q) {
            f(0, q, nodeNo) = LT::w[q]*rho(0, nodeNo);
        }
    }
        
    // **********
    // OUTPUT VTK
    // **********
    auto node_pos = grid.getNodePos(bulkNodes); // Need a named variable as Outputs constructor takes a reference as input
    auto global_dimensions = vtklb.getGlobaDimensions();
    // Setup output file
    Output output(global_dimensions, outDir2, myRank, nProcs, node_pos);
    output.add_file("lb_run");
    // Add density to the output file
    output["lb_run"].add_variable("rho", rho.get_data(), rho.get_field_index(0, bulkNodes), 1);
    output["lb_run"].add_variable("vel", vel.get_data(), vel.get_field_index(0, bulkNodes), LT::nD);

    // Print geometry and boundary marker
    outputGeometry("lb_geo", outDir2, myRank, nProcs, nodes, grid, vtklb);

    // *********
    // MAIN LOOP
    // *********
    
    // For all time steps
    const std::clock_t beginTime = std::clock();
    for (int i = 0; i <= nIterations; i++) {
        // For all bulk nodes
        for (auto nodeNo: bulkNodes) {
            // Copy of local velocity diestirubtion
            std::valarray<lbBase_t> fNode = f(0, nodeNo);

            // MACROSCOPIC VALUES
            lbBase_t rhoNode = calcRho<LT>(fNode);
            std::valarray<lbBase_t> velNode = calcVel<LT>(fNode, rhoNode, force);
            rho(0, nodeNo) = rhoNode;
            for (int d = 0; d < LT::nD; ++d)
                vel(0, d, nodeNo) = velNode[d];
            // BGK COLLISION TERM
            // SRT
            lbBase_t u2 = LT::dot(velNode, velNode);
            std::valarray<lbBase_t> cu = LT::cDotAll(velNode);
            std::valarray<lbBase_t> omegaBGK = calcOmegaBGK<LT>(fNode, tau, rhoNode, u2, cu);
            lbBase_t uF = LT::dot(velNode, force);
            std::valarray<lbBase_t> cF = LT::cDotAll(force);
            std::valarray<lbBase_t> deltaOmegaF = calcDeltaOmegaF<LT>(tau, cu, uF, cF);

            // COLLISION AND PROPAGATION
            for (int q = 0; q < LT::nQ; ++q) {
                fTmp(0, q,  grid.neighbor(q, nodeNo)) = fNode[q]  + omegaBGK[q] + deltaOmegaF[q];
            }
        } // End nodes

        // Swap data_ from fTmp to f;
        f.swapData(fTmp);  // LBfield

        // *******************
        // BOUNDARY CONDITIONS
        // *******************
        // Mpi
        mpiBoundary.communicateLbField(0, f, grid);
        // Half way bounce back
        fluidWallBnd.apply(0, f, grid);

        // *************
        // WRITE TO FILE
        // *************
        if ( ((i % nItrWrite) == 0) && (i > 0) ) {
            output.write("lb_run", i);
            if (myRank==0)
                std::cout << "PLOT AT ITERATION : " << i << " ( " << float( std::clock () - beginTime ) /  CLOCKS_PER_SEC << " sec)" << std::endl;
        }
    } // End iterations
 
    MPI_Finalize();

    return 0;
}
