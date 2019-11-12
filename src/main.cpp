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

#include "LBbndmpi.h"
#include "LBboundary.h"
#include "LBcollision.h"
#include "LBcollision2phase.h"
#include "LBlatticetypes.h"
#include "LBfield.h"
#include "LBgeometry.h"
#include "LBgrid.h"
#include "LBhalfwaybb.h"
#include "LBinitiatefield.h"
#include "LBmacroscopic.h"
#include "LBnodes.h"
#include "LBsnippets.h"
#include "LButilities.h"

#include "Input.h"
#include "Output.h"

#include<algorithm> // std::max

// SET THE LATTICE TYPE
// #define LT D2Q9
#define LT D3Q19


int main()
{
    // SETUP MPI
    MPI_Init(NULL, NULL);
    int nProcs;
    MPI_Comm_size(MPI_COMM_WORLD, &nProcs);
    int myRank;
    MPI_Comm_rank(MPI_COMM_WORLD, &myRank);

    // TEST PRINTOUT
    std::cout << "Begin test Two phase new" << std::endl;

    // SETUP THE INPUT AND OUTPUT PATHS
    // std::string chimpDir = "/home/ejette/Programs/GitHub/BADChIMP-cpp/";
    //std::string chimpDir = "/home/ejette/Programs/GITHUB/badchimpp/";
    std::string chimpDir = "./";
    std::string mpiDir = chimpDir + "input/mpi/";
    std::string inputDir = chimpDir + "input/";
    std::string outDir = chimpDir + "output/rho_val_";



    std::cout << "before read files" << std::endl;
    // READ BOUNDARY FILES WITH MPI INFORMATION
    MpiFile<LT> rankFile(mpiDir + "rank_" + std::to_string(myRank) + "_rank.mpi");
    MpiFile<LT> localFile(mpiDir + "rank_" + std::to_string(myRank) + "_local_labels.mpi");
    MpiFile<LT> globalFile(mpiDir + "rank_" + std::to_string(myRank) + "_global_labels.mpi");

    std::cout << "Sizes = " << rankFile.size() << " " << localFile.size() << " " << globalFile.size() << std::endl;

    // SETUP GRID
    std::cout << "grid" << std::endl;
    auto grid  = Grid<LT>::makeObject(localFile, rankFile, myRank);
    std::cout << "grid.size = " << grid.size() << std::endl;

    // SETUP NODE
    std::cout << "nodes" << std::endl;
    auto nodes = Nodes<LT>::makeObject(localFile, rankFile, myRank, grid);

    // SETUP MPI BOUNDARY
    std::cout << "mpi boundary" << std::endl;
    BndMpi<LT> mpiBoundary(myRank);
    mpiBoundary.setup(localFile, globalFile, rankFile, nodes,  grid);

    // SETUP BOUNCE BACK BOUNDARY (fluid boundary)
    std::cout << "bbBnd" << std::endl;
    HalfWayBounceBack<LT> bbBnd(findBulkNodes(nodes), nodes, grid); // = makeFluidBoundary<HalfWayBounceBack>(nodes, grid);

    // SETUP SOLID BOUNDARY
    std::vector<int> solidBnd = findSolidBndNodes(nodes);

    // SETUP BULK NODES
    std::vector<int> bulkNodes = findBulkNodes(nodes);

    // SETUP CONST PRESSURE AND FLUID SINK
    std::vector<int> constDensNodes, sourceNodes;
    
    for (auto bulkNode: bulkNodes) {
        int y = grid.pos(bulkNode, 1) - 1;
        if (nodes.getRank(bulkNode) == myRank) {
	  //if ( y < 3) // sink
	  if (y < 0){ // Never true
	    sourceNodes.push_back(bulkNode);
	  }
	  //if ( y > (rankFile.dim_global(1) - 2*1 - 4) ) // Const pressure
	  if ( y > (rankFile.dim_global(1)+10) ){ // Never true 
	    constDensNodes.push_back(bulkNode);
	  }
        }
    }

    //---------------------END OF SETUP OF BOUNDARY CONDITION AND MPI---------------------
    
    // READ INPUT FILE
    Input input(inputDir + "input.dat");

    // Vector source
    VectorField<LT> bodyForce(1, 1);
    bodyForce.set(0, 0) = inputAsValarray<lbBase_t>(input["fluid"]["bodyforce"]);

    int nIterations = static_cast<int>( input["iterations"]["max"]);

    lbBase_t tau0 = input["fluid"]["tau"][0];
    lbBase_t tau1 = input["fluid"]["tau"][1];
    lbBase_t sigma = input["fluid"]["sigma"];
    lbBase_t beta = input["fluid"]["beta"];

    lbBase_t tauDiff0 = input["diff-field"]["tauDiff"][0];
    lbBase_t tauDiff1 = input["diff-field"]["tauDiff"][1];
    lbBase_t tauDiff2 = input["diff-field"]["tauDiff"][2];

    lbBase_t H = input["Partial-Misc"]["H"][0];

    std::string outDir2 = "outTest";
    //std::string outDir2 = <std::string>(input["out"]["directory"][0]); //virker ikke!
    
    // SET DERIVED VARAIBLES
    lbBase_t nu0Inv = 1.0 / (LT::c2 * (tau0 - 0.5));
    lbBase_t nu1Inv = 1.0 / (LT::c2 * (tau1 - 0.5));

    lbBase_t DwInv = 1.0 / (LT::c2 * (tauDiff1 - 0.5));
    lbBase_t DsInv = 1.0 / (LT::c2 * (tauDiff0 - 0.5));
    lbBase_t Dw = (LT::c2 * (tauDiff1 - 0.5));
    lbBase_t Ds = (LT::c2 * (tauDiff0 - 0.5));

    

    // Scalar source
    ScalarField Q(2, grid.size());
    for (int n = 0; n < Q.size(); ++n) {
        Q(0, n) = 0.0;
        Q(1, n) = 0.0;
    }
    // SETUP LB FIELDS
    LbField<LT> f(1, grid.size());  // LBfield
    LbField<LT> fTmp(1, grid.size());  // LBfield
    LbField<LT> fInd(1, grid.size()); // LB indicator field
    LbField<LT> fIndTmp(1, grid.size()); // LB indicator field

    LbField<LT> g(3, grid.size()); // LBfield diffusion
    LbField<LT> gTmp(3, grid.size()); // LBfield diffusion

    /*
    LbField<LT> f2(2, grid.size());  // LBfield
    LbField<LT> f2Tmp(2, grid.size());  // LBfield
    
    LbField<LT> g2(2, grid.size()); // LBfield diffusion
    LbField<LT> g2Tmp(2, grid.size()); // LBfield diffusion
    */
    
    // SETUP MACROSCOPIC FIELDS
    ScalarField rho(1, grid.size()); // LBfield
    VectorField<LT> vel(1, grid.size()); // LBfield
    ScalarField cField(1, grid.size()); // LBfield
    ScalarField cgNormField(1, grid.size()); // LBfield
    ScalarField cgFieldX(1, grid.size()); // LBfield
    ScalarField cgFieldY(1, grid.size()); // LBfield
    ScalarField cgFieldZ(1, grid.size()); // LBfield
    ScalarField kappaField(1, grid.size()); // LBfield
    //ScalarField cgField0(1, grid.size()); // LBfield
    //ScalarField cgField1(1, grid.size()); // LBfield
    ScalarField waterChPot(1, grid.size()); // LBfield
    ScalarField indField(1, grid.size()); // LB indicator field
    VectorField<LT> gradients(2, grid.size()); // LBfield

    ScalarField rhoDiff(3, grid.size()); // LBfield

    // SETUP ALTERNATIVE MACROSCOPIC FIELDS
    /*
    ScalarField rho2(2, grid.size()); // LBfield
    VectorField<LT> vel2(1, grid.size()); // LBfield
    ScalarField cgField2(1, grid.size()); // LBfield
    ScalarField rhoDiff2(2, grid.size()); // LBfield
    */
    
    // FILL MACROSCOPIC FIELDS
    //   Fluid densities and velocity
    std::srand(8549388);
    int x0 = 0.5*rankFile.dim_global(0);
    int y0 = 0.25*rankFile.dim_global(1);
    int z0 = 0.5*rankFile.dim_global(2);
    int r0 = 0.3*rankFile.dim_global(0);
    int x01 = 0.5*rankFile.dim_global(0);
    int y01 = 0.75*rankFile.dim_global(1);
    int z01 = 0.5*rankFile.dim_global(2);
    int r01 = 0.20*rankFile.dim_global(0);

    
    for (auto nodeNo: bulkNodes) {
        rho(0, nodeNo) = 1.0; //(1.0 * std::rand()) / (RAND_MAX * 1.0);
	int y = grid.pos(nodeNo, 1) - 1;
	int x = grid.pos(nodeNo, 0) - 1;
	int z = grid.pos(nodeNo, 2) - 1;
	lbBase_t rSq= (x-x0)*(x-x0) + (y-y0)*(y-y0) + (z-z0)*(z-z0);
	lbBase_t r1Sq = (x-x01)*(x-x01) + (y-y01)*(y-y01) + (z-z01)*(z-z01);
	if(rSq<=r0*r0){
	  //if ( (y < (0.5*rankFile.dim_global(1)) || y > (0.8*rankFile.dim_global(1)))
	  //   || (x < (0.2*rankFile.dim_global(0)) || x >= (0.8*rankFile.dim_global(0)) )){
	  rho(0, nodeNo) = 1.0+ 2*sigma*LT::c2Inv/r0;
	  rhoDiff(0, nodeNo) = 0.005; // LB Diff field
	  rhoDiff(1, nodeNo) = 0.0; // LB Diff field
	  rhoDiff(2, nodeNo) = 0.0; // LB Diff field
	  indField(0, nodeNo) = rho(0, nodeNo)-rhoDiff(0, nodeNo);//1.0; // LB indicator field
	  /*
	  rho2(0, nodeNo) = 1.0;
	  rho2(1, nodeNo) = 0.0;
	  rhoDiff2(0, nodeNo) = 0.01; // LB Diff field
	  rhoDiff2(1, nodeNo) = 0.0; // LB Diff field
	  */
	}
	
	else if(r1Sq<=r01*r01){
	  rho(0, nodeNo) = 1.0+ 2*sigma*LT::c2Inv/r01;
	  rhoDiff(0, nodeNo) = 0.01; // LB Diff field
	  rhoDiff(1, nodeNo) = 0.0; // LB Diff field
	  rhoDiff(2, nodeNo) = 0.0; // LB Diff field
	  indField(0, nodeNo) = rho(0, nodeNo)-rhoDiff(0, nodeNo);//1.0; // LB indicator field
	  
	  //rho2(0, nodeNo) = 1.0;
	  //rho2(1, nodeNo) = 0.0;
	  //rhoDiff2(0, nodeNo) = 0.01; // LB Diff field
	  //rhoDiff2(1, nodeNo) = 0.0; // LB Diff field
	}
	
	else{
	  indField(0, nodeNo) = 0.0;
	  rho(0, nodeNo) = 1.0;
	  rhoDiff(0, nodeNo) = 0.0;
	  rhoDiff(1, nodeNo) = 0.0;//0.03968;
	  rhoDiff(2, nodeNo) = 0.0;//0.02;

	  /*
	  rho2(0, nodeNo) = 0.0;
	  rho2(1, nodeNo) = 1.0;
	  rhoDiff2(0, nodeNo) = 0.0;
	  rhoDiff2(1, nodeNo) = 0.04;
	  */
	}
        for (int d=0; d < LT::nD; ++d){
            vel(0, d, nodeNo) = 0.0;
	    /*
	    vel2(0, d, nodeNo) = 0.0;
	    */
	}
    }
    //   Solid boundary (Wettability)
    for (auto nodeNo: solidBnd) {
      //rho(0, nodeNo) = 1.0;
	indField(0, nodeNo) = 1.0;
	rhoDiff(0, nodeNo) = 0.0; // LB Diff field
	rhoDiff(1, nodeNo) = 0.0; // LB Diff field
	rhoDiff(2, nodeNo) = 0.0; // LB Diff field
	/*
	rho2(0, nodeNo) = 1.0;
	rho2(1, nodeNo) = 0.0;
	*/
    }
    //---------------------END OF INPUT TO INITIALIZATION OF FIELDS---------------------

    
    // INITIATE LB FIELDS
    // -- total fluid
    initiateLbField(0, 0, 0, bulkNodes, rho, vel, f);  // LBinitiatefield
    // -- indicator field
    initiateLbField(0, 0, 0, bulkNodes, indField, vel, fInd);  // LBinitiatefield
    // -- diffusive fields
    initiateLbField(0, 0, 0, bulkNodes, rhoDiff, vel, g);  // LBinitiatefield
    initiateLbField(1, 1, 0, bulkNodes, rhoDiff, vel, g);  // LBinitiatefield
    initiateLbField(2, 2, 0, bulkNodes, rhoDiff, vel, g);  // LBinitiatefield

    //Original color gradient
    /*
    // -- phase 0
    initiateLbField(0, 0, 0, bulkNodes, rho2, vel2, f2);  // LBinitiatefield
    // -- phase 1
    initiateLbField(1, 1, 0, bulkNodes, rho2, vel2, f2);  // LBinitiatefield
    initiateLbField(0, 0, 0, bulkNodes, rhoDiff2, vel, g2);  // LBinitiatefield
    initiateLbField(1, 1, 0, bulkNodes, rhoDiff2, vel, g2);  // LBinitiatefield
    */
    
    // JLV    
    //---------------------SETUP OUTPUT---------------------
    std::vector<std::vector<int>> node_pos; node_pos.reserve(bulkNodes.size());
    for (const auto& node:bulkNodes) {
      node_pos.push_back(grid.pos(node));
    }

    Output output(globalFile.dim_global(), outDir2, myRank, nProcs-1, node_pos);
    output.add_file("fluid");
    output["fluid"].add_variable("rho", rho.get_data(), rho.get_field_index(0, bulkNodes), 1);
    output["fluid"].add_variable("vel", vel.get_data(), vel.get_field_index(0, bulkNodes), LT::nD);
    output["fluid"].add_variable("Water_phase", indField.get_data(), indField.get_field_index(0, bulkNodes), 1);
    output["fluid"].add_variable("salt_in_water", rhoDiff.get_data(), rhoDiff.get_field_index(0, bulkNodes), 1);
    output["fluid"].add_variable("water_in_oil", rhoDiff.get_data(), rhoDiff.get_field_index(1, bulkNodes), 1);
    output["fluid"].add_variable("salt_in_oil", rhoDiff.get_data(), rhoDiff.get_field_index(2, bulkNodes), 1);
    output["fluid"].add_variable("waterGrad", gradients.get_data(), gradients.get_field_index(0, bulkNodes), LT::nD);
    output["fluid"].add_variable("colorGrad", gradients.get_data(), gradients.get_field_index(1, bulkNodes), LT::nD);
    output["fluid"].add_variable("kappa", kappaField.get_data(), kappaField.get_field_index(0, bulkNodes), 1);
    //Original color gradient phases
    /*
    output["fluid"].add_variable("rho1", rho2.get_data(), rho2.get_field_index(0, bulkNodes), 1);
    output["fluid"].add_variable("rho2", rho2.get_data(), rho2.get_field_index(1, bulkNodes), 1);
    output["fluid"].add_variable("vel2", vel2.get_data(), vel.get_field_index(0, bulkNodes), LT::nD);
    output["fluid"].add_variable("salt_in_water2", rhoDiff.get_data(), rhoDiff.get_field_index(0, bulkNodes), 1);
    output["fluid"].add_variable("water_in_oil2", rhoDiff.get_data(), rhoDiff.get_field_index(1, bulkNodes), 1);
    */
    
    output.write("fluid", 0);
    //end JLV
    //---------------------END OF SETUP OUTPUT---------------------
    
    
    // -----------------MAIN LOOP------------------
    /* Comments to main loop:
     * Calculation of cu is kept outside of calcOmega and calcDeltaOmega
     *  to avoid recalculation  of the dot-product.
     * It is therefore natural to give all scalar products as inputs to the
     *  calcOmega and calcDeltaOmega.
     * Note that we have a oneIteration function in 'LBiteration.h'. This seemed
     *   to affect the speed an gave a slow down of 10-15 %.
     */

    
    
    for (int i = 0; i <= nIterations; i++) {

        for (auto nodeNo : bulkNodes) {
            // UPDATE MACROSCOPIC DENSITIES
            // Calculate rho for each phase
	    lbBase_t rhoTotNode = rho(0, nodeNo) = calcRho<LT>(f(0,nodeNo));  // LBmacroscopic
	    lbBase_t indicator0Node = indField(0, nodeNo) = calcRho<LT>(fInd(0,nodeNo));  // LBmacroscopic
	    lbBase_t rhoDiff0Node = rhoDiff(0, nodeNo) = calcRho<LT>(g(0,nodeNo));  // LBmacroscopic
	    lbBase_t rhoDiff1Node = rhoDiff(1, nodeNo) = calcRho<LT>(g(1,nodeNo));  // LBmacroscopic
	    lbBase_t rhoDiff2Node = rhoDiff(2, nodeNo) = calcRho<LT>(g(2,nodeNo));  // LBmacroscopic

	    /*
	    lbBase_t rho20Node = rho2(0, nodeNo) = calcRho<LT>(f2(0,nodeNo)); // LBmacroscopic
	    lbBase_t rho21Node = rho2(1, nodeNo) = calcRho<LT>(f2(1,nodeNo)); // LBmacroscopic
	    rhoDiff2(0, nodeNo) = calcRho<LT>(g2(0,nodeNo));  // LBmacroscopic
	    rhoDiff2(1, nodeNo) = calcRho<LT>(g2(1,nodeNo));  // LBmacroscopic
	    */
	    
            // Calculate color gradient kernel
	    const lbBase_t rhoType0Node = indicator0Node+rhoDiff0Node;
	    const lbBase_t rhoType1Node = rhoTotNode-rhoType0Node;
	    //cgField(0, nodeNo) = 2*indicator0Node/rhoTotNode-1;
	    cField(0, nodeNo) = (rhoType0Node-rhoType1Node)/rhoTotNode;
	    //cgField(1, nodeNo) = (rhoDiff0Node-rhoTotNode+indicator0Node)/rhoTotNode;
	    //cgField(2, nodeNo) = (rhoDiff1Node-indicator0Node)/rhoTotNode;
	    
	    // Calculate water gradient kernel
	    waterChPot(0, nodeNo) = /*rhoTotNode**/indicator0Node/*-rhoDiff0Node*/+rhoDiff1Node/H;

	    /*
	    // Original color gradient kernel
	    cgField2(0, nodeNo) = (rho20Node - rho21Node)/(rho20Node + rho21Node);
	    */
        }  // End for all bulk nodes

        // Need to set the density when when calculating the densities
        // Predefine two regions:
        //   1) Pressure is constant
        //   2) Constant sink
        // Strategy - When mass is removed, then remove both oil and water
        //          - If mass is added at the top then then only add oil.

	/*Phase sources Q at the Constant-Density (Pressure) nodes are calculated*/
	/*
	for (auto nodeNo: constDensNodes) {
            lbBase_t rho0Node = rho(0, nodeNo);
            lbBase_t rho1Node = rho(1, nodeNo);

            setConstDensity(Q(0, nodeNo), Q(1, nodeNo), rho0Node, rho1Node, 1.0);
            // Calculate color gradient kernel
            cgField(0, nodeNo) = 0;//(rho0Node - rho1Node)/(rho0Node + rho1Node);
        }
	*/
	/*Phase sources Q at the Constant-rate nodes are calculated*/
	/*
	for (auto nodeNo: sourceNodes) {
            lbBase_t rho0Node = rho(0, nodeNo);
            lbBase_t rho1Node = rho(1, nodeNo);
            //setConstSource(Q(0, nodeNo), Q(1, nodeNo), rho0Node, rho1Node, -1e-4);
	    setConstSource(Q(0, nodeNo), Q(1, nodeNo), rho0Node, rho1Node, -1e-6);
            // Calculate color gradient kernel
            cgField(0, nodeNo) = 0;//(rho0Node - rho1Node)/(rho0Node + rho1Node);
        }
	*/

        for (auto nodeNo: solidBnd) {
	  const lbBase_t rhoTotNode = rho(0, nodeNo);
	  const lbBase_t indicator0Node = indField(0, nodeNo);
	  const lbBase_t rhoDiff0Node = rhoDiff(0, nodeNo);
	  const lbBase_t rhoDiff1Node = rhoDiff(1, nodeNo);
	  cField(0, nodeNo) = 2*indicator0Node-1;
	  //cgField(1, nodeNo) = (rhoDiff0Node-rhoTotNode+indicator0Node)/rhoTotNode;
	  //cgField(2, nodeNo) = (rhoDiff1Node-indicator0Node)/rhoTotNode;
	  waterChPot(0, nodeNo) = indicator0Node;

	  //Original color gradient
	  /*
	  const lbBase_t rho20Node = rho2(0, nodeNo);
	  const lbBase_t rho21Node = rho2(1, nodeNo);
	  cgField2(0, nodeNo) = (rho20Node - rho21Node)/(rho20Node + rho21Node);
	  */
	}

        //  MPI: COMMUNCATE SCALAR 'cgField'
        mpiBoundary.communciateScalarField(cField);
	//mpiBoundary.communciateScalarField(cgField0);
	//mpiBoundary.communciateScalarField(cgField1);
	/*
	//Original color gradient
	mpiBoundary.communciateScalarField(cgField2);
	*/
	
	//  MPI: COMMUNCATE SCALAR 'waterGrad'
        mpiBoundary.communciateScalarField(waterChPot);

	// Curvature kappa calculation
	// color gradient Norm
	//------------------------------------------
	for (auto nodeNo: bulkNodes) {
	  // -- calculate the color gradient
	  std::valarray<lbBase_t> colorGradNode = grad(cField, 0, nodeNo, grid);
	  gradients.set(1, nodeNo) = colorGradNode;
	  cgNormField(0, nodeNo) = vecNorm<LT>(colorGradNode);

	  if(cgNormField(0, nodeNo)>0.0){
	    cgFieldX(0, nodeNo) = colorGradNode[0]/cgNormField(0, nodeNo);
	    cgFieldY(0, nodeNo) = colorGradNode[1]/cgNormField(0, nodeNo);
	    cgFieldZ(0, nodeNo) = colorGradNode[2]/cgNormField(0, nodeNo);
	  }
	  else{
	    cgFieldX(0, nodeNo) = 0.0;
	    cgFieldY(0, nodeNo) = 0.0;
	    cgFieldZ(0, nodeNo) = 0.0;
	  }
	  
	  //colorGradNode *= 1.0/(CGNorm + (CGNorm < lbBaseEps));
	  
	}
	
	for (auto nodeNo: solidBnd) {
	  // -- calculate the color gradient
	  //std::valarray<lbBase_t> colorGradNode = 0.0;//grad(cField, 0, nodeNo, grid);
	  //gradients.set(1, nodeNo) = colorGradNode;
	  cgNormField(0, nodeNo) = 0.0;
	  
	  cgFieldX(0, nodeNo) = 0.0;
	  cgFieldY(0, nodeNo) = 0.0;
	  cgFieldZ(0, nodeNo) = 0.0;
	}
	
	mpiBoundary.communciateScalarField(cgNormField);
	mpiBoundary.communciateScalarField(cgFieldX);
	mpiBoundary.communciateScalarField(cgFieldY);
	mpiBoundary.communciateScalarField(cgFieldZ);
	
	//------------------------------------------

	
        for (auto nodeNo: bulkNodes) {

            // Set the local total lb distribution
            // Use std:valarray insatead of auto as valarray uses expression templates that do not work well with auto
	    std::valarray<lbBase_t> fTot = f(0, nodeNo);
	    //Original color gradient
	    /*
	    std::valarray<lbBase_t> fTot2 = f2(0, nodeNo) + f2(1, nodeNo);
	    */
	    
            // UPDATE MACROSCOPIC VARIABLES
            // -- densities
            //lbBase_t rho0Node = rho(0, nodeNo);
            //lbBase_t rho1Node = rho(1, nodeNo);
	    lbBase_t indicator0Node = indField(0, nodeNo);

	    //Original color gradient
	    /*
	    lbBase_t rho20Node = rho2(0, nodeNo);
            lbBase_t rho21Node = rho2(1, nodeNo);
	    */
	    
	    // -- total density
	    lbBase_t rhoTotNode = rho(0, nodeNo);
	    //Original color gradient
	    /*
	    lbBase_t rhoTot2Node = rho20Node + rho21Node;
	    */
	    
	    lbBase_t rhoDiff0Node = rhoDiff(0, nodeNo);
	    lbBase_t rhoDiff1Node = rhoDiff(1, nodeNo);
	    lbBase_t rhoDiff2Node = rhoDiff(2, nodeNo);

	    //Original color gradient
	    /*
	    lbBase_t rhoDiff20Node = rhoDiff2(0, nodeNo);
	    lbBase_t rhoDiff21Node = rhoDiff2(1, nodeNo);
	    */

	    // -- kappa calculation --------------------------
	    std::valarray<lbBase_t> gradColorGradNormNode = grad(cgNormField, 0, nodeNo, grid);
	    std::valarray<lbBase_t> colorGradNode = gradients(1, nodeNo);
	    lbBase_t CGNorm = cgNormField( 0, nodeNo);//vecNorm<LT>(colorGradNode);
            
	    
	    lbBase_t divGradc = 0.0;
	    for (int q = 0; q < LT::nQ; ++q) {
	      int neigNode = grid.neighbor(q, nodeNo);
	      divGradc += LT::w[q]*cField(0, neigNode);
	    }
	    divGradc -= cField(0, nodeNo);
	    divGradc *= 2*LT::c2Inv;

	    
	    /*  
	    std::valarray<lbBase_t> tempVect;
	    for (int q = 0; q < LT::nQ; ++q) {
	      int neigNode = grid.neighbor(q, nodeNo);
	      tempVect[q] = LT::w[q]*cField(0, neigNode);
	    }
	    */
	    std::valarray<lbBase_t> gradColorGradXNode = grad(cgFieldX, 0, nodeNo, grid);
	    std::valarray<lbBase_t> gradColorGradYNode = grad(cgFieldY, 0, nodeNo, grid);
	    std::valarray<lbBase_t> gradColorGradZNode = grad(cgFieldZ, 0, nodeNo, grid);

	    
	    if (CGNorm>0.0){
	      //kappaField(0, nodeNo) = -(divGradc-LT::dot(colorGradNode/CGNorm, gradColorGradNormNode) )/CGNorm ;
	      kappaField(0, nodeNo) = -(gradColorGradXNode[0]+gradColorGradYNode[1]+gradColorGradZNode[2]);
	    }
	    else kappaField(0, nodeNo) = 0.0;

	    // -----------------------------------------------
	    
            // -- force
            //std::valarray<lbBase_t> forceNode = setForceGravity(rhoTotNode*indicator0Node, rhoTotNode*(1-indicator0Node), bodyForce, 0);
	    std::valarray<lbBase_t> forceNode = 0.5*sigma*kappaField(0, nodeNo)*colorGradNode;
            // -- velocity
            std::valarray<lbBase_t> velNode = calcVel<LT>(fTot, rhoTotNode, forceNode);  // LBmacroscopic
            vel.set(0, nodeNo) = velNode;

	    //Original color gradient
	    /*
	    std::valarray<lbBase_t> vel2Node = calcVel<LT>(fTot2, rhoTot2Node, forceNode);  // LBmacroscopic
            vel2.set(0, nodeNo) = vel2Node;
	    */
	    
	    // -- calculate the color gradient
	    //std::valarray<lbBase_t> colorGradNode = grad(cField, 0, nodeNo, grid);
	    //gradients.set(1, nodeNo) = colorGradNode;

	    /*
	    std::valarray<lbBase_t> colorGrad1Node = grad(cgField, 0, nodeNo, grid);
	    // -- calculate the normalized color gradient
            lbBase_t CG1Norm = vecNorm<LT>(colorGrad1Node);
            colorGrad1Node *= 1.0/(CG1Norm + (CG1Norm < lbBaseEps));

	    std::valarray<lbBase_t> colorGrad2Node = grad(cgField, 0, nodeNo, grid);
	    // -- calculate the normalized color gradient
            lbBase_t CG2Norm = vecNorm<LT>(colorGrad2Node);
            colorGrad2Node *= 1.0/(CG2Norm + (CG2Norm < lbBaseEps)); 
	    */

	    
	    
	    // -- calculate the water gradient
            std::valarray<lbBase_t> waterGradNode = grad(waterChPot, 0, nodeNo, grid);
	    gradients.set(0, nodeNo) = waterGradNode;
	    
            // Correct mass density for mass source
            lbBase_t q0Node = 0.0;// Q(0, nodeNo);
            //lbBase_t q1Node = 0.0;// Q(1, nodeNo);
	    lbBase_t R0Node = 0.0; //                            <------------------------------ Diffusive source set to zero
	    lbBase_t R1Node = 1.e-3*LT::dot(waterGradNode,colorGradNode); //
	    lbBase_t R2Node = 0.0; // 
	    lbBase_t RIndNode = -R1Node/*/rhoTotNode*/;
            rho(0, nodeNo) = rhoTotNode += 0.5*q0Node;
            //rho(1, nodeNo) = rho1Node += 0.5*q1Node;
	    rhoDiff(0, nodeNo) = rhoDiff0Node += 0.5*R0Node;
	    lbBase_t c0Node = rhoDiff0Node/rhoTotNode;
	    rhoDiff(1, nodeNo) = rhoDiff1Node += 0.5*R1Node;
	    lbBase_t c1Node = rhoDiff1Node/rhoTotNode;
	    rhoDiff(2, nodeNo) = rhoDiff2Node += 0.5*R2Node;
	    lbBase_t c2Node = rhoDiff2Node/rhoTotNode;
	    indicator0Node = indField(0, nodeNo)+= 0.5*RIndNode;
	    lbBase_t cIndNode = indicator0Node/rhoTotNode;
	    
            // CALCULATE BGK COLLISION TERM
            // Mean collision time /rho_tot/\nu_tot = \sum_s \rho_s/\nu_s
	    lbBase_t tau = LT::c2Inv * rhoTotNode / ((indicator0Node)*nu0Inv + (rhoTotNode-indicator0Node)*nu1Inv) + 0.5;

	    /*
	    lbBase_t tauWaterPhase = tauDiff1;
	    if (indicator0Node>0.0)
	    tauWaterPhase = LT::c2Inv * indicator0Node / ((indicator0Node-rhoDiff0Node)*DwInv + rhoDiff0Node*DsInv) + 0.5;
	    */
	    //tauWaterPhase = LT::c2Inv * ((1-rhoDiff0Node/indicator0Node)*Dw + rhoDiff0Node/indicator0Node*Ds) + 0.5;

	    
            lbBase_t uu = LT::dot(velNode, velNode);  // Square of the velocity
            std::valarray<lbBase_t> cu = LT::cDotAll(velNode);  // velocity dotted with lattice vectors
            std::valarray<lbBase_t> omegaBGK = calcOmegaBGK<LT>(fTot, tau, rhoTotNode, uu, cu);  // LBcollision
	    std::valarray<lbBase_t> omegaBGKInd = calcOmegaBGK<LT>(fInd(0, nodeNo), tauDiff1, indicator0Node, uu, cu);  // LBcollisionIndicator
	    std::valarray<lbBase_t> omegaBGKDiff0 = calcOmegaBGK<LT>(g(0,nodeNo), tauDiff0, rhoDiff0Node, uu, cu);  // LBcollision
	    std::valarray<lbBase_t> omegaBGKDiff1 = calcOmegaBGK<LT>(g(1,nodeNo), tauDiff1, rhoDiff1Node, uu, cu);  // LBcollision
	    std::valarray<lbBase_t> omegaBGKDiff2 = calcOmegaBGK<LT>(g(2,nodeNo), tauDiff2, rhoDiff2Node, uu, cu);  // LBcollision

	    // CALCULATE BGK COLLISION TERM FOR ORIGINAL COLOR GRADIENT
            // Mean collision time /rho_tot/\nu_tot = \sum_s \rho_s/\nu_s
	    /*
	    lbBase_t tau2 = LT::c2Inv * rhoTot2Node / (rho20Node*nu0Inv + rho21Node*nu1Inv) + 0.5;  
            lbBase_t uu2 = LT::dot(vel2Node, vel2Node);  // Square of the velocity
            std::valarray<lbBase_t> cu2 = LT::cDotAll(vel2Node);  // velocity dotted with lattice vectors
            std::valarray<lbBase_t> omegaBGK2 = calcOmegaBGK<LT>(fTot2, tau2, rhoTot2Node, uu2, cu2);  // LBcollision
	    */
	    
            // CALCULATE FORCE CORRECTION TERM
            lbBase_t  uF = LT::dot(velNode, forceNode);
            std::valarray<lbBase_t>  cF = LT::cDotAll(forceNode);
            std::valarray<lbBase_t> deltaOmegaF = calcDeltaOmegaF<LT>(tau, cu, uF, cF);  // LBcollision
	    std::valarray<lbBase_t> deltaOmegaFDiff0 = calcDeltaOmegaFDiff<LT>(tauDiff0, c0Node, cu, uF, cF);  // LBcollision
	    std::valarray<lbBase_t> deltaOmegaFDiff1 = calcDeltaOmegaFDiff<LT>(tauDiff1, c1Node, cu, uF, cF);  // LBcollision
	    std::valarray<lbBase_t> deltaOmegaFDiff2 = calcDeltaOmegaFDiff<LT>(tauDiff2, c1Node, cu, uF, cF);  // LBcollision

            // CALCULATE MASS SOURCE CORRECTION TERM
            std::valarray<lbBase_t> deltaOmegaQ0 = calcDeltaOmegaQ<LT>(tau, cu, uu, q0Node);
            //std::valarray<lbBase_t> deltaOmegaQ1 = calcDeltaOmegaQ<LT>(tau, cu, uu, q1Node);

	    // CALCULATE DIFFUSIVE MASS SOURCE CORRECTION TERM
	    std::valarray<lbBase_t> deltaOmegaR0 = calcDeltaOmegaR<LT>(tauDiff0, cu, R0Node);
            std::valarray<lbBase_t> deltaOmegaR1 = calcDeltaOmegaR<LT>(tauDiff1, cu, R1Node);
	    std::valarray<lbBase_t> deltaOmegaRInd = calcDeltaOmegaR<LT>(tauDiff1, cu, RIndNode);
	    
            // CALCULATE SURFACE TENSION PERTURBATION

	    // -- calculate the normalized color gradient
	    
	    //std::valarray<lbBase_t> colorGradNode = gradients.set(1, nodeNo);
	  
            colorGradNode *= 1.0/(CGNorm + (CGNorm < lbBaseEps));

            std::valarray<lbBase_t> cCGNorm = LT::cDotAll(colorGradNode);
	    //std::valarray<lbBase_t> cCG1Norm = LT::cDotAll(colorGrad1Node);
	    //std::valarray<lbBase_t> cCG2Norm = LT::cDotAll(colorGrad2Node);
	    

            std::valarray<lbBase_t> deltaOmegaST = calcDeltaOmegaST<LT>(tau, sigma, CGNorm, cCGNorm);

	    const lbBase_t rhoType0Node = indicator0Node+rhoDiff0Node;
	    const lbBase_t rhoType1Node = rhoTotNode-rhoType0Node;
	    
	    std::valarray<lbBase_t> deltaOmegaRCInd = calcDeltaOmegaRC<LT>(beta, indicator0Node, rhoType1Node, rhoTotNode, cCGNorm);
	    std::valarray<lbBase_t> deltaOmegaRCDiff0 = calcDeltaOmegaRC<LT>(beta, rhoDiff0Node, rhoType1Node, rhoTotNode, cCGNorm);
	    std::valarray<lbBase_t> deltaOmegaRCDiff1 = calcDeltaOmegaRC<LT>(beta, rhoDiff1Node, rhoType0Node, rhoTotNode, -cCGNorm);
	    std::valarray<lbBase_t> deltaOmegaRCDiff2 = calcDeltaOmegaRC<LT>(beta, rhoDiff2Node, rhoType0Node, rhoTotNode, -cCGNorm);

	    // CALCULATE SURFACE TENSION PERTURBATION FOR ORIGINAL COLOR GRADIENT
            // -- calculate the normalized color gradient
	    /*
	    std::valarray<lbBase_t> colorGrad2Node = grad(cgField2, 0, nodeNo, grid);
            lbBase_t CGNorm2 = vecNorm<LT>(colorGrad2Node);
            colorGrad2Node *= 1.0/(CGNorm2 + (CGNorm2 < lbBaseEps));
	    
            std::valarray<lbBase_t> cCGNorm2 = LT::cDotAll(colorGrad2Node);
	    std::valarray<lbBase_t> deltaOmegaST2 = calcDeltaOmegaST<LT>(tau2, sigma, CGNorm2, cCGNorm2);
	    std::valarray<lbBase_t> deltaOmegaRC2 = calcDeltaOmegaRC<LT>(beta, rho20Node, rho21Node, rhoTot2Node, cCGNorm2);
	    std::valarray<lbBase_t> deltaOmegaRCDiff20 = calcDeltaOmegaRC<LT>(beta, rhoDiff20Node, rho21Node, rhoTot2Node, cCGNorm2);
	    std::valarray<lbBase_t> deltaOmegaRCDiff21 = calcDeltaOmegaRC<LT>(beta, -rhoDiff21Node, rho20Node, rhoTot2Node, cCGNorm2);

            // COLLISION AND PROPAGATION
	    //Original color gradient
	    
            lbBase_t c0, c1, cDiff0, cDiff1;
            c0 = (rho20Node/rhoTot2Node);  // Concentration of phase 0
            c1 = (rho21Node/rhoTot2Node);  // Concentration of phase 1
	    cDiff0 = (rho20Node/rhoTot2Node);  // Concentration of phase 0
            cDiff1 = (rho21Node/rhoTot2Node);  // Concentration of phase 1
	    */
	    //-----------------------
            for (int q = 0; q < LT::nQ; ++q) {  // Collision should provide the right hand side must be
	      
	      fTmp(0, q,  grid.neighbor(q, nodeNo)) =    fTot[q]            + omegaBGK[q]    + deltaOmegaST[q]*0.          + deltaOmegaF[q];
                
	      fIndTmp(0, q,  grid.neighbor(q, nodeNo)) = fInd(0, q, nodeNo) + omegaBGKInd[q]   + deltaOmegaST[q]*cIndNode*0. + deltaOmegaFDiff1[q] + deltaOmegaRCInd[q]*tauDiff1  + deltaOmegaRInd[q];
	      gTmp(0, q,  grid.neighbor(q, nodeNo)) =    g(0, q, nodeNo)    + omegaBGKDiff0[q] + deltaOmegaST[q]*c0Node*0.   + deltaOmegaFDiff0[q] + deltaOmegaRCDiff0[q]*tauDiff0;
	      gTmp(1, q,  grid.neighbor(q, nodeNo)) =    g(1, q, nodeNo)    + omegaBGKDiff1[q] + deltaOmegaST[q]*c1Node*0.   + deltaOmegaFDiff1[q] + deltaOmegaRCDiff1[q]*tauDiff1 + deltaOmegaR1[q];
	      gTmp(2, q,  grid.neighbor(q, nodeNo)) =    g(2, q, nodeNo)    + omegaBGKDiff2[q] + deltaOmegaST[q]*c2Node*0.   + deltaOmegaFDiff2[q] + deltaOmegaRCDiff2[q]*tauDiff2 ;
	      //Original color gradient
	      /*
	      f2Tmp(0, q,  grid.neighbor(q, nodeNo)) = c0 * (fTot2[q] + omegaBGK2[q] +  deltaOmegaST2[q]) + deltaOmegaRC2[q];
	      f2Tmp(1, q,  grid.neighbor(q, nodeNo)) = c1 * (fTot2[q] + omegaBGK2[q] +  deltaOmegaST2[q]) - deltaOmegaRC2[q];
	      g2Tmp(0, q,  grid.neighbor(q, nodeNo)) = g2(0, q, nodeNo) + omegaBGKDiff0[q] + deltaOmegaST2[q]*cDiff0*0 + deltaOmegaRCDiff20[q];
	      g2Tmp(1, q,  grid.neighbor(q, nodeNo)) = g2(1, q, nodeNo) + omegaBGKDiff1[q] + deltaOmegaST2[q]*cDiff1*0 + deltaOmegaRCDiff21[q];
	      */
	      //-----------------------
            }
        } // End nodes

        // PRINT

        if ( (i % static_cast<int>(input["iterations"]["write"])) == 0) {
	  
	  
	  //-------------------------------------------------------------
	  /*
	  std::string tmpName(outDir);
          tmpName += std::to_string(myRank) + "_" + std::to_string(i);
          tmpName += ".dat";
          std::ofstream ofs;
          ofs.open(tmpName);
          if (!ofs) {
            std::cout << "Error: could not open file: " << tmpName << std::endl;
            return 1;
          }

          for (auto nodeNo: bulkNodes) {
            ofs << std::setprecision(23) << grid.pos(nodeNo, 0) << " " << grid.pos(nodeNo, 1) << " " << grid.pos(nodeNo, 2) << " " << rho(0, nodeNo) << " " << rhoDiff(0, nodeNo)
                            << " " << vel(0, 0, nodeNo) << " " << vel(0, 1, nodeNo) << " " << vel(0, 2, nodeNo) << " " << nodes.getType(nodeNo) << std::endl;
          }
          ofs.close();
	  */
	  //-------------------------------------------------------------

	  
          // JLV
          output.write("fluid", i);
          // JLV
	  if (myRank==0)
            std::cout << "PLOT AT ITERATION : " << i << std::endl;
        }

        // Swap data_ from fTmp to f;
        f.swapData(fTmp);  // LBfield
	g.swapData(gTmp);  // LBfield
	fInd.swapData(fIndTmp);  // LBfield
	//Original color gradient
	/*
	f2.swapData(f2Tmp);  // LBfield
	g2.swapData(g2Tmp);  // LBfield
	*/
	
        // MPI Boundary
        // Hente verdier hente fra ghost
        // Sette i bulk
        mpiBoundary.communicateLbField(0, f, grid);
	mpiBoundary.communicateLbField(0, g, grid);
        mpiBoundary.communicateLbField(1, g, grid);
	mpiBoundary.communicateLbField(2, g, grid);
	mpiBoundary.communicateLbField(0, fInd, grid);

	//Original color gradient
	/*
	mpiBoundary.communicateLbField(0, f2, grid);
	mpiBoundary.communicateLbField(1, f2, grid);
	mpiBoundary.communicateLbField(0, g2, grid);
        mpiBoundary.communicateLbField(1, g2, grid);
	*/
	
        // BOUNDARY CONDITIONS
        bbBnd.apply(0, f, grid);  // LBboundary
	bbBnd.apply(0, g, grid);  // LBboundary
        bbBnd.apply(1, g, grid);  // LBboundary
	bbBnd.apply(2, g, grid);  // LBboundary
	bbBnd.apply(0, fInd, grid); // LBboundary

	//Original color gradient
	/*
	bbBnd.apply(0, f2, grid);  // LBboundary
	bbBnd.apply(1, f2, grid);  // LBboundary
	bbBnd.apply(0, g2, grid);  // LBboundary
        bbBnd.apply(1, g2, grid);  // LBboundary
	*/
	
    } // End iterations
    // -----------------END MAIN LOOP------------------

    //mpi.end();

    MPI_Finalize();

    return 0;
}

