// //////////////////////////////////////////////
//
//  SUBGRID BOUNDARY CONDTIONS
//
// TO RUN PROGRAM: type "mpirun -np <#procs> badchimpp" in command
// line in main directory
//
// //////////////////////////////////////////////


#include "../LBSOLVER"
#include "../IO"
#include "../std_case/LBregularboundarybasic3d.h"
#include "LBpolymer.h"

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
    std::string chimpDir = "/home/ejette/Programs/GitHub/BADChIMP-cpp/";
    std::string mpiDir = chimpDir + "input/mpi/";
    std::string inputDir = chimpDir + "input/";
    std::string outputDir = chimpDir + "output/";

    // ***********************
    // SETUP GRID AND GEOMETRY
    // ***********************
    Input input(inputDir + "input.dat");
    
    GeneralizedNewtonian<LT> carreau(inputDir + "test_rheo.dat");
    
    LBvtk<LT> vtklb(mpiDir + "tmp" + std::to_string(myRank) + ".vtklb");
    Grid<LT> grid(vtklb);
    Nodes<LT> nodes(vtklb, grid);
    BndMpi<LT> mpiBoundary(vtklb, nodes, grid);


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




    ScalarField surfaceDistance(1, grid.size());vtklb.toAttribute("q");
    for (int n=vtklb.beginNodeNo(); n<vtklb.endNodeNo(); ++n) {
        surfaceDistance(0, n) = vtklb.getScalarAttribute<double>();
    }

    VectorField<LT> surfaceNormal(1, grid.size());
    vtklb.toAttribute("nx");
    for (int n=vtklb.beginNodeNo(); n<vtklb.endNodeNo(); ++n) {
        surfaceNormal(0, 0, n) = vtklb.getScalarAttribute<double>();
    }
    vtklb.toAttribute("ny");
    for (int n=vtklb.beginNodeNo(); n<vtklb.endNodeNo(); ++n) {
        surfaceNormal(0, 1, n) = vtklb.getScalarAttribute<double>();
    }
    vtklb.toAttribute("nz");
    for (int n=vtklb.beginNodeNo(); n<vtklb.endNodeNo(); ++n) {
        surfaceNormal(0, 2, n) = vtklb.getScalarAttribute<double>();
    }

    // -- tangent vectors
    // -- field no 0 is t1 
    // -- field no 1 is t2
    VectorField<LT> surfaceTangent(2, grid.size());
    vtklb.toAttribute("t1x");
    for (int n=vtklb.beginNodeNo(); n<vtklb.endNodeNo(); ++n) {
        surfaceTangent(0, 0, n) = vtklb.getScalarAttribute<double>();
    }
    vtklb.toAttribute("t1y");
    for (int n=vtklb.beginNodeNo(); n<vtklb.endNodeNo(); ++n) {
        surfaceTangent(0, 1, n) = vtklb.getScalarAttribute<double>();
    }
    vtklb.toAttribute("t1z");
    for (int n=vtklb.beginNodeNo(); n<vtklb.endNodeNo(); ++n) {
        surfaceTangent(0, 2, n) = vtklb.getScalarAttribute<double>();
    }

    vtklb.toAttribute("t2x");
    for (int n=vtklb.beginNodeNo(); n<vtklb.endNodeNo(); ++n) {
        surfaceTangent(1, 0, n) = vtklb.getScalarAttribute<double>();
    }
    vtklb.toAttribute("t2y");
    for (int n=vtklb.beginNodeNo(); n<vtklb.endNodeNo(); ++n) {
        surfaceTangent(1, 1, n) = vtklb.getScalarAttribute<double>();
    }
    vtklb.toAttribute("t2z");
    for (int n=vtklb.beginNodeNo(); n<vtklb.endNodeNo(); ++n) {
        surfaceTangent(1, 2, n) = vtklb.getScalarAttribute<double>();
    }

 
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
        vel(0,0,nodeNo) = 0.0;
        for (int d=1; d < LT::nD; ++d)
            vel(0, d, nodeNo) = 0.0;
    }

    // ******************
    // SETUP BOUNDARY
    // ****************** 
    std::vector<int> boundaryNodes = findFluidBndNodes(nodes);
    HalfWayBounceBack<LT> bounceBackBnd(findFluidBndNodes(nodes), nodes, grid);
    RegularBoundaryBasic3d<LT> regularizedBoundary(boundaryNodes, surfaceDistance, surfaceNormal, surfaceTangent, rho, bodyForce, tau, nodes, grid);
    // *********
    // LB FIELDS
    // *********
    LbField<LT> f(1, grid.size());  // LBfield
    LbField<LT> fTmp(1, grid.size());  // LBfield

    // initieate values
    for (auto nodeNo: bulkNodes) {
        auto cF = LT::cDotAll(bodyForce(0,0));
        auto cu = LT::cDotAll(vel(0, nodeNo));
        auto uu = vel(0, 0, nodeNo)*vel(0, 0, nodeNo);
        for (int alpha = 0; alpha < LT::nQ; ++alpha) {
            auto c = LT::c(alpha);
            f(0, alpha, nodeNo)  = LT::w[alpha] * rho(0, nodeNo);  
            f(0, alpha, nodeNo) += LT::w[alpha] * LT::c2Inv * rho(0, nodeNo) * cu[alpha];
            f(0, alpha, nodeNo) += LT::w[alpha] * LT::c4Inv0_5 *(cu[alpha] * cu[alpha] - LT::c2*uu) * rho(0, nodeNo);
            f(0, alpha, nodeNo) -= 0.5*LT::c2Inv*LT::w[alpha]*cF[alpha];
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
    VectorField<D3Q19> velIO(1, grid.size());
    ScalarField tauField(1, grid.size());
    ScalarField viscosityField(1, grid.size());    
    output["lb_run"].add_variable("rho", rho.get_data(), rho.get_field_index(0, bulkNodes), 1);
    output["lb_run"].add_variable("tau", tauField.get_data(), tauField.get_field_index(0, bulkNodes), 1);
    output["lb_run"].add_variable("viscosity", viscosityField.get_data(), viscosityField.get_field_index(0, bulkNodes), 1);
    output["lb_run"].add_variable("vel", velIO.get_data(), velIO.get_field_index(0, bulkNodes), 3);
    // Print geometry 
    outputGeometry("lb_geo", outDir2, myRank, nProcs, nodes, grid, vtklb);

    // *********
    // MAIN LOOP
    // *********
    const std::clock_t beginTime = std::clock();
    for (int i = 0; i <= nIterations; i++) {
        // ***************
        // GLOBAL COUNTERS
        // ***************
        // For all bulk nodes
        for (auto nodeNo: bulkNodes) {
            // Copy of local velocity diestirubtion
            auto fNode = f(0, nodeNo);

            // MACROSCOPIC VALUES
            auto rhoNode = calcRho<LT>(fNode);
            auto velNode = calcVel<LT>(fNode, rhoNode, force);

            rho(0, nodeNo) = rhoNode;
            for (int d = 0; d < LT::nD; ++d)
                vel(0, d, nodeNo) = velNode[d];

            // BGK COLLISION TERM
            // SRT
            auto u2 = LT::dot(velNode, velNode);
            auto cu = LT::cDotAll(velNode);
            auto omegaBGK = carreau.omegaBGK(fNode, rhoNode, velNode, u2, cu, force, 0);
            tau = carreau.tau();
            tauField(0, nodeNo) = tau;
            viscosityField(0, nodeNo) = carreau.viscosity(); 
            /* tau = 0.6;
            auto omegaBGK = calcOmegaBGK<LT>(fNode, tau, rhoNode, u2, cu); */
            auto uF = LT::dot(velNode, force);
            auto cF = LT::cDotAll(force);
            auto deltaOmegaF = calcDeltaOmegaF<LT>(tau, cu, uF, cF);

            // COLLISION AND PROPAGATION
            for (int q = 0; q < LT::nQ; ++q) {  
                fTmp(0, q,  grid.neighbor(q, nodeNo)) = fNode[q] + omegaBGK[q] + deltaOmegaF[q];
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
        bounceBackBnd.apply(0, f, grid);

        // *************
        // WRITE TO FILE
        // *************
        
        if ( ((i % nItrWrite) == 0) && (i > 0) ) {
            for (auto nn: bulkNodes) {
                velIO(0, 0, nn) = vel(0, 0, nn);
                velIO(0, 1, nn) = vel(0, 1, nn);
                if (LT::nD == 2)
                {
                    velIO(0, 2, nn) = 0;                    
                } 
                else
                {
                    velIO(0, 2, nn) = vel(0, 2, nn);;
                }
            }
            
            output.write("lb_run", i);
            if (myRank==0) {
                std::cout << "PLOT AT ITERATION : " << i << " ( " << float( std::clock () - beginTime ) /  CLOCKS_PER_SEC << " sec)" << std::endl;
            }
        }

    } // End iterations

    MPI_Finalize();

    return 0;
}
