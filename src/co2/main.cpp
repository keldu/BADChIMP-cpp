// //////////////////////////////////////////////
//
// BADChIMP std_case
//
// For documentation see:
//    doc/documentation.pdf
// 
// //////////////////////////////////////////////

#include "../LBSOLVER.h"
#include "../IO.h"
#include "./LBco2help.h"

// SET THE LATTICE TYPE
#define LT D2Q9
#define VTK_CELL VTK::pixel

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
    //std::string chimpDir = "/home/AD.NORCERESEARCH.NO/esje/Programs/GitHub/BADCHiMP/";
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
    // Set solid boundary 
    std::vector<int> solidBoundaryNodes = findSolidBndNodes(nodes);

    // *************
    // READ FROM INPUT
    // *************
    // Number of iterations
    int nIterations = static_cast<int>( input["iterations"]["max"]);
    // Write interval
    int nItrWrite = static_cast<int>( input["iterations"]["write"]);
    // Number of fields
    const int nFluidFields = input["fluid"]["numfluids"];
    std::cout << "nFluidFields = " << nFluidFields << std::endl;
    // Relaxation time
    // lbBase_t tau = 1;
    // Kinematic viscosities
    std::valarray<lbBase_t> kin_visc = inputAsValarray<lbBase_t>(input["fluid"]["viscosity"]);
    std::valarray<lbBase_t> kin_visc_inv(nFluidFields);
    for (int n = 0; n < nFluidFields; ++n){
      kin_visc_inv[n] = 1./kin_visc[n];
    }
      
    
    // Body force
    VectorField<LT> bodyForce(1, 1);
    bodyForce.set(0, 0) = inputAsValarray<lbBase_t>(input["fluid"]["bodyforce"]);
    std::cout << std::endl;

    //Parameters for compressibility
    std::valarray<lbBase_t> alpha = LT::w0*inputAsValarray<lbBase_t>(input["fluid"]["alpha"]);
    std::cout << "alpha =";
    for (int n = 0; n < nFluidFields; ++n)
        std::cout << " " << alpha[n];
    std::cout << std::endl;
    std::valarray<lbBase_t> Gamma0 = inputAsValarray<lbBase_t>(input["fluid"]["alpha"]);
    std::valarray<lbBase_t> GammaNonZero = (1-LT::w0*Gamma0)/(1-LT::w0);
    
    
    // Color gradient
    //lbBase_t beta = 1;
    std::valarray<lbBase_t> beta = inputAsValarray<lbBase_t>(input["fluid"]["phaseseparation"]["beta"]);
    std::cout << "beta = " << std::endl;
    for (int i=0; i < nFluidFields; ++i) {
        for (int j=0; j < nFluidFields; ++j) {
            std::cout << " " << beta[i*nFluidFields + j];
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;

    std::valarray<lbBase_t> sigma = inputAsValarray<lbBase_t>(input["fluid"]["phaseseparation"]["sigma"]);
    std::cout << "sigma = " << std::endl;
    for (int i=0; i < nFluidFields; ++i) {
        for (int j=0; j < nFluidFields; ++j) {
            std::cout << " " << sigma[i*nFluidFields + j];
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;

    std::string dirNum = std::to_string(static_cast<int>(input["out"]["directoryNum"]));
    
    std::string outputDir2 = outputDir + "/out" + dirNum;

    
    // ******************
    // MACROSCOPIC FIELDS
    // ******************
    // Density
    ScalarField rho(nFluidFields, grid.size());
    ScalarField rhoRel(nFluidFields, grid.size());
    ScalarField rhoTot(1, grid.size());
    // Initiate density from file
    setScalarAttribute(rho, "init_rho_", vtklb);

    // Wall wettability
    setScalarAttributeWall(rhoRel, "init_rho_wall_", vtklb, nodes);

    // -- scale wettability
    normelizeScalarField(rhoRel, solidBoundaryNodes);

    // Velocity
    VectorField<LT> vel(nFluidFields, grid.size());
    // Initiate velocity
    for (auto fieldNo=0; fieldNo < vel.num_fields(); ++fieldNo) {
        for (auto nodeNo: bulkNodes) {
            vel.set(fieldNo, nodeNo) = 0;
        }
    }
    // ******************
    // SETUP BOUNDARY
    // ******************
    HalfWayBounceBack<LT> bounceBackBnd(findFluidBndNodes(nodes), nodes, grid);

    // *********
    // LB FIELDS
    // *********
    LbField<LT> f(nFluidFields, grid.size()); 
    LbField<LT> fTmp(nFluidFields, grid.size());
    // initiate lb distributions
    for (int fieldNo=0; fieldNo < f.num_fields(); ++fieldNo) {
        for (auto nodeNo: bulkNodes) {
            for (int q = 0; q < LT::nQ; ++q) {
                f(fieldNo, q, nodeNo) = LT::w[q]*rho(fieldNo, nodeNo);
                fTmp(fieldNo, q, nodeNo) = 0;
            }
        }
    }
    // **********
    // OUTPUT VTK
    // **********
    VTK::Output<VTK_CELL, double> output(VTK::BINARY, grid.getNodePos(bulkNodes), outputDir2, myRank, nProcs);
    output.add_file("lb_run");
    
    output.add_variable("rho", 1, rho.get_data(), rho.get_field_index(0, bulkNodes));
    for (int fieldNo=0; fieldNo < nFluidFields; ++fieldNo) {
        output.add_variable("rho" + std::to_string(fieldNo), 1, rho.get_data(), rho.get_field_index(fieldNo, bulkNodes));
    }
    output.add_variable("vel", LT::nD, vel.get_data(), vel.get_field_index(0, bulkNodes));
    
    /*
    auto node_pos = grid.getNodePos(bulkNodes); 
    auto global_dimensions = vtklb.getGlobaDimensions();
    Output output(global_dimensions, outputDir2, myRank, nProcs, node_pos);
    output.add_file("lb_run");
    VectorField<D3Q19> velIO(1, grid.size());
    for (int fieldNo=0; fieldNo < nFluidFields; ++fieldNo) {
        output["lb_run"].add_variable("rho" + std::to_string(fieldNo), rho.get_data(), rho.get_field_index(fieldNo, bulkNodes), 1);
    }
    output["lb_run"].add_variable("vel", velIO.get_data(), velIO.get_field_index(0, bulkNodes), 3);
    outputGeometry("lb_geo", outputDir2, myRank, nProcs, nodes, grid, vtklb);
    */
    
    // *********
    // MAIN LOOP
    // *********
    // Just ad hoc helper fields (Should be set outside the loop structure)
    std::valarray<lbBase_t> cNormInv(LT::nQ);
    std::valarray<lbBase_t> wAll(LT::nQ);
    for (int q=0; q < LT::nQNonZero_; ++q) {
        cNormInv[q] = 1.0/LT::cNorm[q];
        wAll[q] = LT::w[q];
    }
    cNormInv[LT::nQNonZero_] = 0;
    wAll[LT::nQNonZero_] = LT::w[LT::nQNonZero_];
    for (int i = 0; i <= nIterations; i++) {
        // Macroscopic values : rho, rhoTot and rhoRel = rho/rhoTot
        calcDensityFields(rho, rhoRel, rhoTot, bulkNodes, f);

        mpiBoundary.communciateScalarField(rhoRel);

        for (auto nodeNo: bulkNodes) {
            // Cacluate gradient
            ScalarField rhoRelNode(1, nFluidFields);
            for (int fieldNo = 0; fieldNo < nFluidFields; ++fieldNo)
                rhoRelNode(0, fieldNo) = rhoRel(fieldNo, nodeNo);

            const CGAtributes<LT> cgat(nFluidFields, nodeNo, cNormInv, Gamma0, rhoRelNode, rhoRel, grid);

            /* VectorField<LT> F(1, (nFluidFields*(nFluidFields-1))/2);
            ScalarField FSquare(1, F.size());
            ScalarField FNorm(1, F.size());
            LbField<LT> cDotFRC(1, F.size());
            LbField<LT> cosPhi(1, F.size());
            VectorField<LT> gradNode(1, nFluidFields);   
            int cnt = 0;
            //total effect of modified compressibility
            lbBase_t Gamma0TotNode = 0.0;
            lbBase_t GammaNonZeroTotNode = 0.0;
            
            for (int fieldNo_k=0; fieldNo_k<nFluidFields; ++fieldNo_k) {
                Gamma0TotNode += rhoRelNode(0, fieldNo_k)*Gamma0[fieldNo_k];
                GammaNonZeroTotNode += rhoRelNode(0, fieldNo_k)*GammaNonZero[fieldNo_k];
		        gradNode.set(0, fieldNo_k) = grad<LT>(rhoRel, fieldNo_k, nodeNo, grid);
                for (int fieldNo_l = 0; fieldNo_l < fieldNo_k; ++fieldNo_l) {
                    F.set(0, cnt) = rhoRelNode(0, fieldNo_l)*gradNode(0, fieldNo_k) - rhoRelNode(0, fieldNo_k)*gradNode(0, fieldNo_l);
                    cDotFRC.set(0, cnt) = LT::cDotAll(F(0,cnt));
                    FSquare(0, cnt) = LT::dot(F(0, cnt), F(0, cnt));
                    FNorm(0,cnt) = sqrt(FSquare(0, cnt));
                    if (abs(FNorm(0, cnt)) < lbBaseEps)
                        FNorm(0, cnt) = lbBaseEps;
                    cosPhi.set(0, cnt) = cDotFRC(0, cnt)*cNormInv/FNorm(0,cnt);
                    cnt++;                    
                }
            } */

            std::valarray<lbBase_t> feqTotRel0Node(LT::nQ);
            for (int q=0; q < LT::nQNonZero_; ++q) {
                feqTotRel0Node[q]= wAll[q]* cgat.GammaNonZeroTotNode_;//GammaNonZeroTotNode;
            }
            feqTotRel0Node[LT::nQNonZero_] = wAll[LT::nQNonZero_]*cgat.Gamma0TotNode_;//Gamma0TotNode;
            
                // Calculate velocity
            // Copy of local velocity distribution
            auto velNode = calcVel<LT>(f(0, nodeNo), rhoTot(0, nodeNo));
	        lbBase_t visc_inv = rhoRelNode(0, 0)*kin_visc_inv[0];
            for (int fieldNo=1; fieldNo<nFluidFields; ++fieldNo){
                velNode += calcVel<LT>(f(fieldNo, nodeNo), rhoTot(0, nodeNo));
		        visc_inv += rhoRelNode(0, fieldNo)*kin_visc_inv[fieldNo];
	        }
            velNode += 0.5*bodyForce(0, 0)/rhoTot(0, nodeNo);
	        lbBase_t tauFlNode = LT::c2Inv/visc_inv + 0.5;
            // Save density and velocity for printing
            vel.set(0, nodeNo) = velNode;
                
            // BGK-collision term
            const auto u2 = LT::dot(velNode, velNode);
            const auto cu = LT::cDotAll(velNode);
            // Calculate the Guo-force correction
            const auto uF = LT::dot(velNode, bodyForce(0, 0));
            const auto cF = LT::cDotAll(bodyForce(0, 0));

            LbField<LT> fTotNode(1,1);
            fTotNode.set(0,0) = 0;
            LbField<LT> omegaRC(1, nFluidFields);

            for (int fieldNo=0; fieldNo<nFluidFields; ++fieldNo) {
                const auto fNode = f(fieldNo, nodeNo);
                const auto rhoNode = rho(fieldNo, nodeNo);
                //const auto omegaBGK = calcOmegaBGK<LT>(fNode, tauFlNode, rhoNode, u2, cu);
		        const auto feqNode = calcfeq_TEST<LT>(Gamma0[fieldNo], GammaNonZero[fieldNo], rhoNode, u2, cu);
		        const auto omegaBGK = calcOmegaBGK_TEST<LT>(fNode, feqNode, tauFlNode);
                const std::valarray<lbBase_t> deltaOmegaF = rhoRel(fieldNo, nodeNo) * calcDeltaOmegaF<LT>(tauFlNode, cu, uF, cF);
                LbField<LT> deltaOmegaST(1,1);
               
                // Recoloring step
                int field_k_ind = (fieldNo*(fieldNo-1))/2;
                omegaRC.set(0, fieldNo) = 0;
                deltaOmegaST.set(0 ,0) = 0;
                for (int field_l = 0; field_l < fieldNo; ++field_l) {
                    const int F_ind = field_k_ind + field_l;
        		    const int sigmaBeta_ind = fieldNo*nFluidFields + field_l;
                    deltaOmegaST.set(0 ,0) += calcDeltaOmegaST<LT>(tauFlNode, sigma[sigmaBeta_ind], cgat.FNorm_(0, F_ind), cgat.cDotFRC_(0, F_ind)/cgat.FNorm_(0, F_ind));
                    omegaRC.set(0, fieldNo) += beta[sigmaBeta_ind]*rhoRel(field_l, nodeNo)*cgat.cosPhi_(0, F_ind);
                }
                for (int field_l = fieldNo + 1; field_l < nFluidFields; ++field_l) {
                    const int field_k_ind = (field_l*(field_l-1))/2;
                    const int F_ind =  field_k_ind + fieldNo;
        		    const int sigmaBeta_ind = fieldNo*nFluidFields + field_l;
                    deltaOmegaST.set(0 ,0) += calcDeltaOmegaST<LT>(tauFlNode, sigma[sigmaBeta_ind], cgat.FNorm_(0, F_ind), -cgat.cDotFRC_(0, F_ind)/cgat.FNorm_(0, F_ind));
                    omegaRC.set(0, fieldNo) -= beta[sigmaBeta_ind]*rhoRel(field_l, nodeNo)*cgat.cosPhi_(0,F_ind);
                }

                //omegaRC.set(0, fieldNo) *= wAll*rhoNode;
                omegaRC.set(0, fieldNo) *= rhoNode*feqTotRel0Node;

                // Calculate total lb field
                fTotNode.set(0, 0) += fNode + deltaOmegaF + omegaBGK + deltaOmegaST(0, 0);
            }
            // Collision and propagation
            for (int fieldNo=0; fieldNo<nFluidFields; ++fieldNo) {
                fTmp.propagateTo(fieldNo, nodeNo, rhoRel(fieldNo, nodeNo)*fTotNode(0, 0) + omegaRC(0, fieldNo), grid);
            }

        } // End nodes
        // Swap data_ from fTmp to f;
        f.swapData(fTmp);  // LBfield

        // *******************
        // BOUNDARY CONDITIONS
        // *******************
        // Mpi
        mpiBoundary.communicateLbField(f, grid);
        // Half way bounce back
        bounceBackBnd.apply(f, grid);

        // *************
        // WRITE TO FILE
        // *************
        if ( ((i % nItrWrite) == 0)  ) {
	  /*
	    for (auto nn: bulkNodes) {
                velIO(0, 0, nn) = vel(0, 0, nn);
                velIO(0, 1, nn) = vel(0, 1, nn);
                velIO(0, 2, nn) = 0; 

            }
            output.write("lb_run", i);
	  */
	    output.write(i);
            if (myRank==0) {
                std::cout << "PLOT AT ITERATION : " << i << std::endl;
            }
        }

    } // End iterations

    MPI_Finalize();

    return 0;
}
