// ///////////////////////////////////////////////////////////
//
//  RANS solver
//
//  TO DO
//        - new viscosity model (new RANS class)
//            - at least TRT 
//            - Other possibilites : Cumulant, Regularized LB..
//        - 2 scalar advective diffusive - fields k and \epsilon
//                -- with source terms
//
//        -Focus on bulk:
//           - First standard bounce back
//             Then:        
//             - Boundary cond. logarithmic law of the wall
//
//        -Lattice Boltzmann model:
//            - See eg. Filippova ... and succi 2001
//            - Advection diffusion use LB algorithm
//        
// 
// ///////////////////////////////////////////////////////////

#include "../LBSOLVER.h"
#include "../IO.h"
#include "LBrans.h"

// SET THE LATTICE TYPE
#define LT D2Q9

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
    std::string chimpDir = "/home/AD.NORCERESEARCH.NO/esje/Programs/GitHub/BADCHiMP/";
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
    // Set bulk nodes
    std::vector<int> bulkNodes = findBulkNodes(nodes);
    
    // *************
    // READ FROM INPUT
    // *************
    // Number of iterations
    // int nIterations = static_cast<int>( input["iterations"]["max"]);
    int nIterations = input["iterations"]["max"];
    // Write interval
    // int nItrWrite = static_cast<int>( input["iterations"]["write"]);
    int nItrWrite = input["iterations"]["write"];
    // Relaxation time
    lbBase_t tau = input["fluid"]["tau"];
    // Body force
    VectorField<LT> bodyForce(1, 1);
    // bodyForce.set(0, 0) = inputAsValarray<lbBase_t>(input["fluid"]["bodyforce"]);
    bodyForce.set(0, 0) = input["fluid"]["bodyforce"];

    // *************
    // DEFINE RHEOLOGY
    // *************
    Rans<LT> rans(input);
    
    // ******************
    // MACROSCOPIC FIELDS
    // ******************
    // Viscosity
    ScalarField viscosity(1, grid.size());
    // Density
    ScalarField rho(1, grid.size());
    // Initiate density from file
    vtklb.toAttribute("init_rho");
    for (int n=vtklb.beginNodeNo(); n < vtklb.endNodeNo(); ++n) {
        rho(0, n) = vtklb.getScalarAttribute<lbBase_t>();
    }
    // Velocity
    VectorField<LT> vel(1, grid.size());
    // Initiate velocity
    for (auto nodeNo: bulkNodes) {
        for (int d=0; d < LT::nD; ++d)
            vel(0, d, nodeNo) = 0.0;
    }
    // k field
    ScalarField kinE(1, grid.size());
    // Initiate k field from file
    vtklb.toAttribute("init_k");
    for (int n=vtklb.beginNodeNo(); n < vtklb.endNodeNo(); ++n) {
        kinE(0, n) = vtklb.getScalarAttribute<lbBase_t>();
    }
    // epsilon field
    ScalarField epslion(1, grid.size());
    // Initiate k field from file
    vtklb.toAttribute("init_epsilon");
    for (int n=vtklb.beginNodeNo(); n < vtklb.endNodeNo(); ++n) {
        epslion(0, n) = vtklb.getScalarAttribute<lbBase_t>();
    }
    
    
    // ******************
    // SETUP BOUNDARY
    // ******************
    HalfWayBounceBack<LT> bounceBackBnd(findFluidBndNodes(nodes), nodes, grid);

    // *********
    // LB FIELDS
    // *********
    // flow field
    LbField<LT> f(1, grid.size()); 
    LbField<LT> fTmp(1, grid.size());
    // k field
    LbField<LT> g(1, grid.size()); 
    LbField<LT> gTmp(1, grid.size());
    // epsilon field
    LbField<LT> h(1, grid.size()); 
    LbField<LT> hTmp(1, grid.size());
    
    // initiate lb distributions
    for (auto nodeNo: bulkNodes) {
        for (int q = 0; q < LT::nQ; ++q) {
            f(0, q, nodeNo) = LT::w[q]*rho(0, nodeNo);
	    g(0, q, nodeNo) = LT::w[q]*kinE(0, nodeNo);
	    h(0, q, nodeNo) = LT::w[q]*epslion(0, nodeNo);
        }
    }

    // **********
    // OUTPUT VTK
    // **********
    //VectorField<LT> velIO(1, grid.size());
    Output<LT> output(grid, bulkNodes, outputDir, myRank, nProcs);
    output.add_file("lb_run");
    output.add_scalar_variables({"viscosity", "rho"}, {viscosity, rho});
    output.add_vector_variables({"vel"}, {vel});

    Output<LT,int> geo(grid.pos(), outputDir, myRank, nProcs, "geo", nodes.geo(grid, vtklb));
    geo.write();
    //outputGeometry("lb_geo", outputDir, myRank, nProcs, nodes, grid, vtklb);

    // *********
    // MAIN LOOP
    // *********
    for (int i = 0; i <= nIterations; i++) {
        // Calculate macroscopic values for all nodes
        for (auto nodeNo: bulkNodes) {
            
        }
        // Communicate rho fields
        
        for (auto nodeNo: bulkNodes) {
            // Copy of local velocity diestirubtion
            const std::valarray<lbBase_t> fNode = f(0, nodeNo);

            // Macroscopic values
            const lbBase_t rhoNode = calcRho<LT>(fNode);
            const auto velNode = calcVel<LT>(fNode, rhoNode, bodyForce(0, 0));

            // Save density and velocity for printing
            rho(0, nodeNo) = rhoNode;
            vel.set(0, nodeNo) = velNode;
                
            // BGK-collision term
            const lbBase_t u2 = LT::dot(velNode, velNode);
            const std::valarray<lbBase_t> cu = LT::cDotAll(velNode);
            auto omegaBGK = rans.omegaBGK(fNode, rhoNode, velNode, u2, cu, bodyForce(0, 0), 0);

            // Calculate the Guo-force correction
            const lbBase_t uF = LT::dot(velNode, bodyForce(0, 0));
            const std::valarray<lbBase_t> cF = LT::cDotAll(bodyForce(0, 0));
            tau = rans.tau();
            viscosity(0, nodeNo) = rans.viscosity();
            const std::valarray<lbBase_t> deltaOmegaF = calcDeltaOmegaF<LT>(tau, cu, uF, cF);

            // Collision and propagation
            fTmp.propagateTo(0, nodeNo, fNode + omegaBGK + deltaOmegaF, grid);

        } // End nodes

        // Swap data_ from fTmp to f;
        f.swapData(fTmp);  // LBfield

        // *******************
        // BOUNDARY CONDITIONS
        // *******************
        // Mpi
        mpiBoundary.communicateLbField(0, f, grid);
        // Half way bounce back
        bounceBackBnd.apply(f, grid);

        // *************
        // WRITE TO FILE
        // *************
        if ( ((i % nItrWrite) == 0)  ) {
            // for (auto nn: bulkNodes) {
            //     velIO(0, 0, nn) = vel(0, 0, nn);
            //     velIO(0, 1, nn) = vel(0, 1, nn);
            //     velIO(0, 2, nn) = 0;
            // }
            output.write(i);
            if (myRank==0) {
                std::cout << "PLOT AT ITERATION : " << i << std::endl;
            }
        }

    } // End iterations

    MPI_Finalize();

    return 0;
}
