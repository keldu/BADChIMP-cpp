#ifndef LBSIXDOF_H
#define LBSIXDOF_H

#include "../lbsolver/LBglobal.h"
#include "../lbsolver/LBlatticetypes.h"
#include "../lbsolver/LBfield.h"
#include "../lbsolver/LBvtk.h"
#include "../lbsolver/LBnodes.h"
#include "../lbsolver/LBboundary.h"

#include <array>

//=====================================================================================
//
//               R O T A T I N G   R E F E R A N C E   F R A M E
//
//=====================================================================================
/*template<typename DXQY>
class RotatingRefranceFrame
{
public:
    RotatingReferanceFrame() {};
    std::valarray<lbBase_t> velocity() const;
    std::valarray<lbBase_t> force() const;
    template<typename T1, typename T2, typename T3>
    void apply(const T1 &fNode, const lbBase_t rho, const T2 &pos, const T3 &bodyForce, const lbBase_t omega, const lbBase_t alpha);

private:
    std::valarray<lbBase_t> vel_(DXQY::nD);
    std::valarray<lbBase_t> force_(DXQY::nD);    
};

//                             RotatingReranceFrame
//----------------------------------------------------------------------------------- apply
template<typename DXQY>
template<typename T1, typename T2, typename T3>
void RotatingReferanceFrame<DXQY>::apply(const T1 &fNode, const lbBase_t rho, const T2 &pos, const T3 &bodyForce, const lbBase_t omega, const lbBase_t alpha)
{
    const lbBase_t rhoInv  = 1.0/rho;
    const lbBase_t omega2 = omega*omega; 
    const lbBase_t oneOmega2Inverse = 1.0/(1 + omega2);
    const auto Mi = DXQY::qSumC(fNode);
    const lbBase_t Cx = Mi[0]/rho + 0.5*(bodyForce[0] + omega2*pos[0] + pos[1]*alpha);
    const lbBase_t Cy = Mi[1]/rho + 0.5*(bodyForce[1] + omega2*pos[1] - pos[0]*alpha);    
} */


//=====================================================================================
//
//                            P S E U D O   F O R C E
//
//=====================================================================================
template<typename DXQY, typename T1, typename T2, typename T3>
std::valarray<lbBase_t> calcVelRot(const T1 &fNode, const lbBase_t rho, const T2 &pos, const T3 &bodyForce, const lbBase_t omega, const lbBase_t alpha)
{
    const auto Mi = DXQY::qSumC(fNode);
    const lbBase_t omega2 = omega*omega; 
    const lbBase_t Cx = Mi[0]/rho + 0.5*(bodyForce[0] + omega2*pos[0] + pos[1]*alpha);
    const lbBase_t Cy = Mi[1]/rho + 0.5*(bodyForce[1] + omega2*pos[1] - pos[0]*alpha);

    std::valarray<lbBase_t> vel(DXQY::nD);
    const lbBase_t tmp = 1.0/(1 + omega2);
    vel[0] = tmp*(Cx + omega*Cy);
    vel[1] = tmp*(Cy - omega*Cx);
    vel[2] = Mi[2]/rho + 0.5*bodyForce[1];

    return vel;
}

template<typename T1, typename T2>
std::valarray<lbBase_t> pseudoForce(const T1 &pos, const T2 &vel, const lbBase_t omega, const lbBase_t alpha) 
{
    std::valarray<lbBase_t> force(3);
    force[2] = 0;
    // Centrifugal force
    const lbBase_t omega2 = omega*omega;
    force[0] = omega2 * pos[0];
    force[1] = omega2 * pos[1];
    // Coriolis force
    force[0] += vel[1]*omega;
    force[1] -= vel[0]*omega;
    // Euler force
    force[0] += pos[1]*alpha;
    force[1] -= pos[0]*alpha;

    return force;
}


//=====================================================================================
//
//               A N T I -  B O U N C E   B A C K   B O U N D A R Y
//
//=====================================================================================
template<typename DXQY>
class AntiBounceBackBoundary
{
public:
    AntiBounceBackBoundary(){};
    AntiBounceBackBoundary(const std::vector<int> &bndNodes, const Nodes<DXQY> &nodes, const Grid<DXQY> &grid)
    :bnd_(bndNodes, nodes, grid){};

    void apply(const int fieldNo, LbField<DXQY> &f, const VectorField<DXQY> &u, const ScalarField &pind, const Grid<DXQY> &grid) const;

private:
    lbBase_t alphaLinkABB( const int fieldNo, const int alpha, const int nodeNo, const LbField<DXQY> &f, const VectorField<DXQY> &u, const Grid<DXQY> &grid) const;

    Boundary<DXQY> bnd_;
};

//                             AntiBounceBackBoundary
//----------------------------------------------------------------------------------- apply
template<typename DXQY>
void AntiBounceBackBoundary<DXQY>::apply(const int fieldNo, LbField<DXQY> &f, const VectorField<DXQY> &u, const ScalarField &pind, const Grid<DXQY> &grid) const
{
    for (int bndNo = 0; bndNo < bnd_.size(); ++bndNo) 
    {
        //std::cout << "Boundary node = " << bndNo << std::endl;
        const int nodeNo = bnd_.nodeNo(bndNo);        
        for (const auto &beta: bnd_.beta(bndNo)) {
            const int nodeBnd = grid.neighbor(DXQY::reverseDirection(beta), nodeNo);

            if (pind(0, nodeBnd)>0) {
                f(fieldNo, beta, nodeNo) = alphaLinkABB(fieldNo, beta, nodeNo, f, u, grid);
            } 
        }
        for (const auto &delta: bnd_.delta(bndNo)) {
            const int deltaRev = DXQY::reverseDirection(delta);
            if ( pind(0, grid.neighbor(deltaRev, nodeNo)) > 0 ) {
                f(fieldNo, delta, nodeNo) = alphaLinkABB(fieldNo, delta, nodeNo, f, u, grid);
            }
            if (( pind(0, grid.neighbor(delta, nodeNo)) > 0 )) {
                f(fieldNo, deltaRev, nodeNo) = alphaLinkABB(fieldNo, deltaRev, nodeNo, f, u, grid);
            }
        } 
    }  
} 

//                             AntiBounceBackBoundary
//----------------------------------------------------------------------------------- alphaLinkABB
template<typename DXQY>
lbBase_t AntiBounceBackBoundary<DXQY>::alphaLinkABB( const int fieldNo, const int alpha, const int nodeNo, const LbField<DXQY> &f, const VectorField<DXQY> &u, const Grid<DXQY> &grid) const
{
    const int alphaRev = DXQY::reverseDirection(alpha);
    const int nodeBnd = grid.neighbor(alphaRev, nodeNo);
    const std::valarray<lbBase_t> uNode = u(fieldNo, nodeNo);
    const lbBase_t u2 = DXQY::dot(uNode, uNode);
    const lbBase_t cu = DXQY::dot(DXQY::c(alphaRev), uNode);

    return -f(fieldNo, alphaRev, nodeBnd) + 2*DXQY::w[alphaRev]*(1 + DXQY::c4Inv0_5*(cu*cu - DXQY::c2*u2) );
}


//=====================================================================================
//
//          I N T E R P O L A T E D   B O U N C E   B A C K   B O U N D A R Y
//
//=====================================================================================
template<typename DXQY>
class InterpolatedBounceBackBoundary
{
public:
    InterpolatedBounceBackBoundary(){};
    InterpolatedBounceBackBoundary(const std::vector<int> &bndNodes, const Nodes<DXQY> &nodes, const Grid<DXQY> &grid)
    :bnd_(bndNodes, nodes, grid){};

    void apply(const int fieldNo, LbField<DXQY> &f, const Grid<DXQY> &grid) const;
    void apply(const ScalarField &phi, const int fieldNo, LbField<DXQY> &f, const Grid<DXQY> &grid) const;  
    std::valarray<lbBase_t> apply(const ScalarField &phi, const int fieldNo, LbField<DXQY> &f, const Grid<DXQY> &grid, VectorField<DXQY> &momentumExchange) const;  
    

private:
    lbBase_t betaLinkIBB(const int beta, const int nodeNo, const int fieldNo, const LbField<DXQY> &f, const Grid<DXQY> &grid) const;
    lbBase_t betaLinkIBB(const lbBase_t q, const int beta, const int nodeNo, const int fieldNo, const LbField<DXQY> &f, const Grid<DXQY> &grid) const;
    lbBase_t deltaLinkIBB(const int delta, const int nodeNo, const int fieldNo, const LbField<DXQY> &f, const Grid<DXQY> &grid) const;

    Boundary<DXQY> bnd_;
};

//                             InterpolatedBounceBackBoundary
//----------------------------------------------------------------------------------- apply
template<typename DXQY>
void InterpolatedBounceBackBoundary<DXQY>::apply(const int fieldNo, LbField<DXQY> &f, const Grid<DXQY> &grid) const
{
    for (int bndNo = 0; bndNo < bnd_.size(); ++bndNo) 
    {
        const int nodeNo = bnd_.nodeNo(bndNo);
        for (const auto & beta: bnd_.beta(bndNo))
        {
            const lbBase_t fval = betaLinkIBB(beta, nodeNo, fieldNo, f, grid);
            f(fieldNo, beta, nodeNo) = fval; 
        }

        for (const auto & delta: bnd_.delta(bndNo))
        {
            const lbBase_t fval = deltaLinkIBB(delta, nodeNo, fieldNo, f, grid);
            f(fieldNo, delta, nodeNo) = fval; 

            const int deltaRev = DXQY::reverseDirection(delta);
            const lbBase_t fvalRev = deltaLinkIBB(deltaRev, nodeNo, fieldNo, f, grid);
            f(fieldNo, deltaRev, nodeNo) = fvalRev;
        }
    }
}

//                             InterpolatedBounceBackBoundary
//----------------------------------------------------------------------------------- apply version 02
template<typename DXQY>
void InterpolatedBounceBackBoundary<DXQY>::apply(const ScalarField &phi, const int fieldNo, LbField<DXQY> &f, const Grid<DXQY> &grid) const
{
    for (int bndNo = 0; bndNo < bnd_.size(); ++bndNo) 
    {
        const int nodeNo = bnd_.nodeNo(bndNo);
        for (const auto & beta: bnd_.beta(bndNo))
        {
            const int nodeNoSolid = grid.neighbor(DXQY::reverseDirection(beta), nodeNo);
            const lbBase_t q = phi(0, nodeNo)/(phi(0, nodeNo)- phi(0, nodeNoSolid));
            const lbBase_t fval = betaLinkIBB(q, beta, nodeNo, fieldNo, f, grid);
            f(fieldNo, beta, nodeNo) = fval; 
        }

        for (const auto & delta: bnd_.delta(bndNo))
        {
            const lbBase_t fval = deltaLinkIBB(delta, nodeNo, fieldNo, f, grid);
            f(fieldNo, delta, nodeNo) = fval; 

            const int deltaRev = DXQY::reverseDirection(delta);
            const lbBase_t fvalRev = deltaLinkIBB(deltaRev, nodeNo, fieldNo, f, grid);
            f(fieldNo, deltaRev, nodeNo) = fvalRev;
        }
    }
}


//                             InterpolatedBounceBackBoundary
//----------------------------------------------------------------------------------- apply version 03
template<typename DXQY>
std::valarray<lbBase_t> InterpolatedBounceBackBoundary<DXQY>::apply(const ScalarField &phi, const int fieldNo, LbField<DXQY> &f, const Grid<DXQY> &grid, VectorField<DXQY> &momentumExchange) const
{
    std::valarray<lbBase_t> surfaceForce(0.0, DXQY::nD);
    for (int bndNo = 0; bndNo < bnd_.size(); ++bndNo) 
    {
        const int nodeNo = bnd_.nodeNo(bndNo);
        momentumExchange.set(fieldNo, nodeNo) = 0;

        lbBase_t wTtot = 0;

        for (const auto & beta: bnd_.beta(bndNo))
        {
            const int betaRev = DXQY::reverseDirection(beta);
            const int nodeNoSolid = grid.neighbor(betaRev, nodeNo);
            const lbBase_t q = phi(0, nodeNo)/(phi(0, nodeNo)- phi(0, nodeNoSolid));
            const lbBase_t fval = betaLinkIBB(q, beta, nodeNo, fieldNo, f, grid);

            f(fieldNo, beta, nodeNo) = fval; 

            std::valarray<lbBase_t> c(DXQY::nD);
            for (int nd=0; nd < DXQY::nD; ++nd) 
                c[nd] = DXQY::c(betaRev, nd);

            momentumExchange.set(fieldNo, nodeNo) += (f(fieldNo, betaRev, nodeNoSolid) + fval)*c;
            wTtot += DXQY::w[betaRev];            
        }


        for (const auto & delta: bnd_.delta(bndNo))
        {
            const lbBase_t fval = deltaLinkIBB(delta, nodeNo, fieldNo, f, grid);
            f(fieldNo, delta, nodeNo) = fval; 

            const int deltaRev = DXQY::reverseDirection(delta);

            std::valarray<lbBase_t> c(DXQY::nD);
            for (int nd=0; nd < DXQY::nD; ++nd) 
                c[nd] = DXQY::c(deltaRev, nd);

            momentumExchange.set(fieldNo, nodeNo) += (f(fieldNo, deltaRev, grid.neighbor(deltaRev, nodeNo)) + fval)*c;
            wTtot += DXQY::w[deltaRev];            

            const lbBase_t fvalRev = deltaLinkIBB(deltaRev, nodeNo, fieldNo, f, grid);
            f(fieldNo, deltaRev, nodeNo) = fvalRev;

            momentumExchange.set(fieldNo, nodeNo) -= (f(fieldNo, delta, grid.neighbor(delta, nodeNo)) + fval)*c;
            wTtot += DXQY::w[delta];            
        }

        surfaceForce += momentumExchange(fieldNo, nodeNo);

        /*if (wTtot > 0) {
            momentumExchange.set(fieldNo, nodeNo) = momentumExchange(fieldNo,nodeNo)/wTtot;
        }*/

    }

    return surfaceForce;
}



//                               InterpolatedBounceBackBoundary
//----------------------------------------------------------------------------------- betaLinkIBB
template<typename DXQY>
lbBase_t InterpolatedBounceBackBoundary<DXQY>::betaLinkIBB(const int beta, const int nodeNo, const int fieldNo, const LbField<DXQY> &f, const Grid<DXQY> &grid) const
{
    const int betaRev = DXQY::reverseDirection(beta);

    const lbBase_t fval = f(fieldNo, betaRev, grid.neighbor(betaRev, nodeNo));

    return fval;
}

//----------------------------------------------------------------------------------- betaLinkIBB version 02
template<typename DXQY>
lbBase_t InterpolatedBounceBackBoundary<DXQY>::betaLinkIBB(const lbBase_t q, const int beta, const int nodeNo, const int fieldNo, const LbField<DXQY> &f, const Grid<DXQY> &grid) const
{
    const int betaRev = DXQY::reverseDirection(beta);
    const int xs = grid.neighbor(betaRev, nodeNo);
    if ( q > 0.5) 
    {
        const int xf = grid.neighbor(beta, nodeNo);

        const lbBase_t tmps = 0.5/q;
        const lbBase_t tmpf = 1-tmps;

        return tmps*f(fieldNo, betaRev, xs) + tmpf*f(fieldNo, beta, xf);
    }

    const int x = nodeNo;

    const lbBase_t tmps = 2*q;
    const lbBase_t tmp = 1 - tmps;

    return tmps*f(fieldNo, betaRev, xs) + tmp*f(fieldNo, betaRev, x);
}


//                               InterpolatedBounceBackBoundary
//----------------------------------------------------------------------------------- deltaLinkIBB
template<typename DXQY>
lbBase_t InterpolatedBounceBackBoundary<DXQY>::deltaLinkIBB(const int delta, const int nodeNo, const int fieldNo, const LbField<DXQY> &f, const Grid<DXQY> &grid) const
{
    const int deltaRev = DXQY::reverseDirection(delta);

    const lbBase_t fval = f(fieldNo, deltaRev, grid.neighbor(deltaRev, nodeNo));

    return fval;
}



//=====================================================================================
//
//                          W E T  N O D E  B O U N D A R Y
//
//=====================================================================================
template<typename DXQY>
class WetNodeBoundary
{
public:
    template<typename T>
    WetNodeBoundary(const T & bulkNodes, LBvtk<DXQY> & vtklb, const Nodes<DXQY> &nodes, const Grid<DXQY> & grid);
    void applyBoundaryCondition(const int fieldNo, LbField<DXQY> &f, const VectorField<DXQY> & force, const Grid<DXQY> &grid) const;
    void addMassToWallBoundary(const int fieldNo, LbField<DXQY> &f, const VectorField<DXQY> & force, const lbBase_t mass);

private:
    template<typename T>
    std::vector<T> readScalarValues(const std::string attributeName, LBvtk<DXQY> & vtk, const Nodes<DXQY> &nodes, const Grid<DXQY> & grid);
    template<typename T>
    std::vector<std::valarray<T>> readVectorValues(const std::string attributeName, LBvtk<DXQY> & vtk, const Nodes<DXQY> &nodes, const Grid<DXQY> & grid);

    const std::valarray<lbBase_t> w_;
    int numBoundaries_;
    struct DiffusionBoundaryNodes
    {
        std::vector<std::valarray<lbBase_t>> normals;
        std::vector<lbBase_t> qs;
        std::vector<int> indicators;
        std::vector<int> neighbors;
        Boundary<DXQY> bnd;
    } wallBoundary_, pressureBoundary_, wallPressureBoundary_;
    lbBase_t outfluxWallTotal_;
};


//                               WetNodeBoundary
//----------------------------------------------------------------------------------- WetNodeBoundary
template<typename DXQY>
template<typename T>
WetNodeBoundary<DXQY>::WetNodeBoundary(const T &bulkNodes, LBvtk<DXQY> & vtklb, const Nodes<DXQY> &nodes, const Grid<DXQY> & grid)
:w_(DXQY::w, DXQY::nQ)
//-----------------------------------------------------------------------------------
{
    std::vector<int> wallBoundaryNodes;
    std::vector<int> pressureBoundaryNodes;
    std::vector<int> wallPressureBoundaryNodes;

    // Read pressure_boundary
    int numBnd = 0;
    vtklb.toAttribute("pressure_boundary");
//    for (int nodeNo = vtklb.beginNodeNo(); nodeNo < vtklb.endNodeNo(); nodeNo++)
    for (auto & nodeNo: bulkNodes)
    {
        const auto pInd = vtklb.template getScalarAttribute<int>();
        numBnd = std::max(numBnd, pInd);
        if ( nodes.isFluidBoundary(nodeNo) ) {
            auto hasSolidNeighbors = [&nodeNo, &nodes, &grid]() -> bool {
                for (auto neighNo: grid.neighbor(nodeNo)) 
                    if ( nodes.isBulkSolid(neighNo) || nodes.isSolidBoundary(neighNo) ) 
                        return true;
                return false;
            };

            if (hasSolidNeighbors()) {
                if (pInd == 0) {
                    wallBoundaryNodes.push_back(nodeNo);
                 } else {
                    wallPressureBoundaryNodes.push_back(nodeNo);
                    wallPressureBoundary_.indicators.push_back(pInd);
                    // pressureBoundaryNodes.push_back(nodeNo);
                    // pressureBoundary_.indicators.push_back(pInd); 
                }
            } else {
                if (pInd > 0) {
                    pressureBoundaryNodes.push_back(nodeNo);
                    pressureBoundary_.indicators.push_back(pInd);
                } else {
                    std::cout << "Node " << nodeNo << " on processor rank " << nodes.getRank(nodeNo);
                    std::cout << " is not a wall node and is not assigned a pressure!" << std::endl;
                    exit(1);
                }
            }
        }  
    }
    // Setup the different boundary nodes.
    wallBoundary_.bnd = Boundary<DXQY>(wallBoundaryNodes, nodes, grid);
    pressureBoundary_.bnd =  Boundary<DXQY>(pressureBoundaryNodes, nodes, grid);
    wallPressureBoundary_.bnd = Boundary<DXQY>(wallPressureBoundaryNodes, nodes, grid);         
    numBoundaries_ = numBnd;

    auto qvalues = readScalarValues<lbBase_t>("q", vtklb, nodes, grid);
    auto normalvec = readVectorValues<lbBase_t>("normal", vtklb, nodes, grid);
    auto neighvec = readVectorValues<int>("neighbor", vtklb, nodes, grid);

    auto fillBoundaryNodes = [&qvalues, &normalvec, &neighvec](DiffusionBoundaryNodes &diffBnd) 
    {
        for (int n = 0; n < diffBnd.bnd.size(); ++n) {
            const int nodeNo = diffBnd.bnd.nodeNo(n);
            diffBnd.qs.push_back(qvalues[nodeNo]);
            diffBnd.normals.push_back(normalvec[nodeNo]);
            std::vector<int> nvec(DXQY::nD);
            for (int d=0; d < DXQY::nD; ++d)  nvec[d] = neighvec[nodeNo][d];
            diffBnd.neighbors.push_back(DXQY::c2q(nvec));
        }        
    };

    fillBoundaryNodes(wallBoundary_);
    fillBoundaryNodes(pressureBoundary_);
    fillBoundaryNodes(wallPressureBoundary_);

    //----------------------------------------------------------------------------------- setup mass conservation
    outfluxWallTotal_ = 0.0;
    const Boundary<DXQY> bnd = wallBoundary_.bnd;
    for (int bndNo=0; bndNo<bnd.size(); ++bndNo) {
        for (const auto & beta: bnd.beta(bndNo)) {
            outfluxWallTotal_ += DXQY::w[beta];
        }
        for (const auto & delta: bnd.delta(bndNo)) {
            outfluxWallTotal_ += 2*DXQY::w[delta];
        }
    }    

}

//                               WetNodeBoundary
//----------------------------------------------------------------------------------- applyBoundaryCondition
template<typename DXQY>
void WetNodeBoundary<DXQY>::applyBoundaryCondition(const int fieldNo, LbField<DXQY> &f, const VectorField<DXQY> & force, const Grid<DXQY> &grid) const
//-----------------------------------------------------------------------------------
{
    //----------------------------------------------------------------------------------- wall boundary
    for (int bndNo = 0; bndNo < wallBoundary_.bnd.size(); ++bndNo) {
        const int nodeNo = wallBoundary_.bnd.nodeNo(bndNo);
        const int alphaNeig = wallBoundary_.neighbors[bndNo];
        const int nodeNeig = grid.neighbor(alphaNeig, nodeNo);
        const auto fNeig = f(fieldNo, nodeNeig);

        // Calculate first moment
        const auto qNode = wallBoundary_.qs[bndNo];
        const std::valarray<lbBase_t> uNeig = DXQY::qSumC(fNeig) + 0.5*force(fieldNo, nodeNeig);
        const auto uNode = qNode*uNeig/(1 +  qNode);

        // Calculate zeroths moment
        lbBase_t lhs = 1.0;
        lbBase_t rhs = f(fieldNo, DXQY::nQNonZero_, nodeNo);

        for (auto &gamma: wallBoundary_.bnd.gamma(bndNo)) {
            rhs += f(fieldNo, gamma, nodeNo);
            rhs += f(fieldNo, DXQY::reverseDirection(gamma), nodeNo);
        }

        const std::valarray<lbBase_t> forceNode = force(fieldNo, nodeNo);
        for (auto &beta: wallBoundary_.bnd.beta(bndNo)) {
            const auto betaHat = DXQY::reverseDirection(beta);
            const auto c_beta = DXQY::c(beta);
            rhs += 2*f(fieldNo, betaHat, nodeNo);
            rhs -= w_[beta]*DXQY::c2Inv*DXQY::dot(c_beta, forceNode);
            lhs -= 2*w_[beta]*DXQY::c2Inv*DXQY::dot(c_beta, uNode);
        }

        const auto rhoNeig = DXQY::qSum(fNeig);
        const auto ccfNeig = DXQY::qSumCCLowTri(fNeig);
        const auto piTrace = DXQY::traceLowTri(ccfNeig);
        const auto uu = DXQY::dot(uNeig, uNeig);
        const auto c2 = DXQY::c2;

        std::valarray<lbBase_t> QPi_neq(DXQY::nQ);
        for (auto &delta: wallBoundary_.bnd.delta(bndNo)) {
            const auto c_delta = DXQY::c(delta);
            const auto ccpi = DXQY::dot(DXQY::contractionLowTriVec(ccfNeig, c_delta), c_delta);
            const auto cc = DXQY::dot(c_delta, c_delta);
            const auto cu = DXQY::dot(c_delta, uNeig);
            QPi_neq[delta] = ccpi-c2*piTrace - rhoNeig*c2*(cc - c2*DXQY::nD) - rhoNeig*(cu*cu - c2*uu);

            rhs += w_[delta]*DXQY::c4Inv*QPi_neq[delta];
            lhs -= 2*w_[delta];
            lhs -=  w_[delta]*DXQY::c4Inv*(cu*cu - c2*uu);
        }
        const auto rhoNode = rhs/lhs;

        for (auto &beta: wallBoundary_.bnd.beta(bndNo)) {           
            const auto betaHat = DXQY::reverseDirection(beta);
            const auto c_beta = DXQY::c(beta);
            const auto cu = DXQY::dot(c_beta, uNode);
            const auto cF = DXQY::dot(c_beta, forceNode);

            f(fieldNo, beta, nodeNo) = f(fieldNo, betaHat, nodeNo) + w_[beta]*DXQY::c2Inv*(2*rhoNode*cu - cF);         
        }

        const auto uuNode = DXQY::dot(uNode, uNode);

        for (auto &delta: wallBoundary_.bnd.delta(bndNo)) {
            const auto deltaHat = DXQY::reverseDirection(delta);
            const auto c_delta = DXQY::c(delta);
            const auto cc = DXQY::dot(c_delta, c_delta);
            const auto cu = DXQY::dot(c_delta, uNode);
            const auto cF = DXQY::dot(c_delta, forceNode);
            
            f(fieldNo, delta, nodeNo) = w_[delta]*rhoNode*(1 + DXQY::c2Inv*cu + DXQY::c4Inv0_5*(cu*cu - c2*uuNode) );
            f(fieldNo, delta, nodeNo) += w_[delta]*DXQY::c4Inv0_5*QPi_neq[delta] - w_[delta]*0.5*DXQY::c2Inv*cF;

            f(fieldNo, deltaHat, nodeNo) = w_[delta]*rhoNode*(1 - DXQY::c2Inv*cu + DXQY::c4Inv0_5*(cu*cu - c2*uuNode) );
            f(fieldNo, deltaHat, nodeNo) += w_[delta]*DXQY::c4Inv0_5*QPi_neq[delta] + w_[delta]*0.5*DXQY::c2Inv*cF;
        }     
    }
    //----------------------------------------------------------------------------------- wall pressure boundary
    for (int bndNo = 0; bndNo < wallPressureBoundary_.bnd.size(); ++bndNo) {
        const int nodeNo = wallPressureBoundary_.bnd.nodeNo(bndNo);
        const int alphaNeig = wallPressureBoundary_.neighbors[bndNo];
        const int nodeNeig = grid.neighbor(alphaNeig, nodeNo);
        const auto fNeig = f(fieldNo, nodeNeig);

        // Calculate first moment
        const auto qNode = wallPressureBoundary_.qs[bndNo];
        const std::valarray<lbBase_t> uNeig = DXQY::qSumC(fNeig) + 0.5*force(fieldNo, nodeNeig);
        std::valarray<lbBase_t>  uNode = qNode*uNeig/(1 +  qNode);
        const std::valarray<lbBase_t> forceNode = force(fieldNo, nodeNo);

        const lbBase_t rhoNode = 1.0;

        lbBase_t wUnknown = 0.0;
        std::valarray<lbBase_t> uUnknown(DXQY::nD);
        uUnknown = 0;

        for (auto &beta: wallPressureBoundary_.bnd.beta(bndNo)) {           
            const auto betaHat = DXQY::reverseDirection(beta);
            const auto c_beta = DXQY::c(beta);
            const auto cu = DXQY::dot(c_beta, uNode);
            const auto cF = DXQY::dot(c_beta, forceNode);

            f(fieldNo, beta, nodeNo) = f(fieldNo, betaHat, nodeNo) + w_[beta]*DXQY::c2Inv*(2*rhoNode*cu - cF);         
            wUnknown += w_[beta];
        }

        const auto rhoNeig = DXQY::qSum(fNeig);
        const auto ccfNeig = DXQY::qSumCCLowTri(fNeig);
        const auto piTrace = DXQY::traceLowTri(ccfNeig);
        const auto uu = DXQY::dot(uNeig, uNeig);
        const auto c2 = DXQY::c2;
        const auto uuNode = DXQY::dot(uNode, uNode);

        for (auto &delta: wallPressureBoundary_.bnd.delta(bndNo)) {
            const auto deltaHat = DXQY::reverseDirection(delta);
            const auto c_delta = DXQY::c(delta);
            const auto ccpi = DXQY::dot(DXQY::contractionLowTriVec(ccfNeig, c_delta), c_delta);
            const auto cc = DXQY::dot(c_delta, c_delta);
            const auto cu = DXQY::dot(c_delta, uNode);
            const auto cF = DXQY::dot(c_delta, forceNode);
            const auto QPi_neq = ccpi-c2*piTrace - rhoNeig*c2*(cc - c2*DXQY::nD) - rhoNeig*(cu*cu - c2*uu);
            
            f(fieldNo, delta, nodeNo) = w_[delta]*rhoNode*(1 + DXQY::c2Inv*cu + DXQY::c4Inv0_5*(cu*cu - c2*uuNode) );
            f(fieldNo, delta, nodeNo) += w_[delta]*DXQY::c4Inv0_5*QPi_neq - w_[delta]*0.5*DXQY::c2Inv*cF;

            f(fieldNo, deltaHat, nodeNo) = w_[delta]*rhoNode*(1 - DXQY::c2Inv*cu + DXQY::c4Inv0_5*(cu*cu - c2*uuNode) );
            f(fieldNo, deltaHat, nodeNo) += w_[delta]*DXQY::c4Inv0_5*QPi_neq + w_[delta]*0.5*DXQY::c2Inv*cF;
            wUnknown += 2*w_[delta];
        }     
        const lbBase_t rhoCorr = (rhoNode - DXQY::qSum(f(fieldNo, nodeNo)))/wUnknown;
        for (auto &beta: wallPressureBoundary_.bnd.beta(bndNo)) {
             f(fieldNo, beta, nodeNo) += w_[beta]*rhoCorr;
        }
        for (auto &delta: wallPressureBoundary_.bnd.delta(bndNo)) {
            const auto deltaHat = DXQY::reverseDirection(delta);
             f(fieldNo, delta, nodeNo) += w_[delta]*rhoCorr;
             f(fieldNo, deltaHat, nodeNo) += w_[delta]*rhoCorr;
        } 
    }

    //----------------------------------------------------------------------------------- pressure boundary
    for (int bndNo = 0; bndNo < pressureBoundary_.bnd.size(); ++bndNo) {
        const int nodeNo = pressureBoundary_.bnd.nodeNo(bndNo);
        const int alphaNeig = pressureBoundary_.neighbors[bndNo];
        const auto nVec = pressureBoundary_.normals[bndNo];
        const int nodeNeig = grid.neighbor(alphaNeig, nodeNo);
        const auto fNeig = f(fieldNo, nodeNeig);
        const lbBase_t rhoNode = 1.0;

        lbBase_t eqA = 0;
        lbBase_t eqB = -rhoNode;
        lbBase_t eqC = 0.5*DXQY::dot(force(fieldNo, nodeNo), nVec);
        for (auto &gamma: pressureBoundary_.bnd.gamma(bndNo)) {
            const auto gammaHat = DXQY::reverseDirection(gamma);
            const auto c = DXQY::c(gamma);
            const auto cn = DXQY::dot(c, nVec);
            eqC += (f(fieldNo, gamma, nodeNo) - f(fieldNo, gammaHat, nodeNo))*cn;
        }

        const std::valarray<lbBase_t> uNeig = DXQY::qSumC(fNeig) + 0.5*force(fieldNo, nodeNeig);
        const auto rhoNeig = DXQY::qSum(fNeig);
        const auto ccfNeig = DXQY::qSumCCLowTri(fNeig);
        const auto piTrace = DXQY::traceLowTri(ccfNeig);
        const auto uu = DXQY::dot(uNeig, uNeig);
        const auto c2 = DXQY::c2;

        for (auto &beta: pressureBoundary_.bnd.beta(bndNo)) {
            const auto betaHat = DXQY::reverseDirection(beta);
            const auto c = DXQY::c(beta);
            const auto cn = DXQY::dot(c, nVec);
            const auto ccpi = DXQY::dot(DXQY::contractionLowTriVec(ccfNeig, c), c);
            const auto cc = DXQY::dot(c, c);
            const auto cu = DXQY::dot(c, uNeig);
            const auto QPi_neq = ccpi-c2*piTrace - rhoNeig*c2*(cc - c2*DXQY::nD) - rhoNeig*(cu*cu - c2*uu);

            eqC += -f(fieldNo, betaHat, nodeNo)*cn;

            eqC += w_[beta]*rhoNode*cn;
            eqC += w_[beta]*DXQY::c4Inv0_5*QPi_neq*cn;
            eqC -= w_[beta]*0.5*DXQY::c2Inv*DXQY::dot(c, force(fieldNo, nodeNo))*cn;

            eqB += w_[beta]*DXQY::c2*rhoNode*cn*cn;

            eqA += w_[beta]*DXQY::c4Inv0_5*rhoNode*(cn*cn - DXQY::c2)*cn;
        }
        for (auto &delta: pressureBoundary_.bnd.delta(bndNo)) {
            const auto c = DXQY::c(delta);
            const auto cn = DXQY::dot(c, nVec);
            eqC -= w_[delta]*0.5*DXQY::c2Inv*DXQY::dot(c, force(fieldNo, nodeNo))*cn;
            eqB += w_[delta]*DXQY::c2*rhoNode*cn*cn;
        }

        const lbBase_t uNodeN = -eqC/eqB - (eqA*eqC*eqC)/(eqB*eqB*eqB);
        const std::valarray<lbBase_t> uNode = uNodeN*nVec;

        lbBase_t wUnknown = 0;

        for (auto &beta: pressureBoundary_.bnd.beta(bndNo)) {           
            const auto betaHat = DXQY::reverseDirection(beta);
            const auto c_beta = DXQY::c(beta);
            const auto cu = DXQY::dot(c_beta, uNode);
            const auto cF = DXQY::dot(c_beta, force(fieldNo, nodeNo));

            f(fieldNo, beta, nodeNo) = f(fieldNo, betaHat, nodeNo) + w_[beta]*DXQY::c2Inv*(2*rhoNode*cu - cF);         

            wUnknown += w_[beta];
        }
        const auto uuNode = DXQY::dot(uNode, uNode);

        for (auto &delta: pressureBoundary_.bnd.delta(bndNo)) {
            const auto deltaHat = DXQY::reverseDirection(delta);
            const auto c_delta = DXQY::c(delta);
            const auto ccpi = DXQY::dot(DXQY::contractionLowTriVec(ccfNeig, c_delta), c_delta);
            const auto cc = DXQY::dot(c_delta, c_delta);
            const auto cu = DXQY::dot(c_delta, uNode);
            const auto cF = DXQY::dot(c_delta, force(fieldNo, nodeNo));
            const auto QPi_neq = ccpi-c2*piTrace - rhoNeig*c2*(cc - c2*DXQY::nD) - rhoNeig*(cu*cu - c2*uu);
            
            f(fieldNo, delta, nodeNo) = w_[delta]*rhoNode*(1 + DXQY::c2Inv*cu + DXQY::c4Inv0_5*(cu*cu - c2*uuNode) );
            f(fieldNo, delta, nodeNo) += w_[delta]*DXQY::c4Inv0_5*QPi_neq - w_[delta]*0.5*DXQY::c2Inv*cF;

            f(fieldNo, deltaHat, nodeNo) = w_[delta]*rhoNode*(1 - DXQY::c2Inv*cu + DXQY::c4Inv0_5*(cu*cu - c2*uuNode) );
            f(fieldNo, deltaHat, nodeNo) += w_[delta]*DXQY::c4Inv0_5*QPi_neq + w_[delta]*0.5*DXQY::c2Inv*cF;

            wUnknown += 2*w_[delta];
        }    

        std::valarray<lbBase_t> uCorr = uNode - DXQY::qSumC(f(fieldNo, nodeNo)) - 0.5*force(fieldNo, nodeNo);
        const lbBase_t rhoCorr = (rhoNode - DXQY::qSum(f(fieldNo, nodeNo)))/wUnknown;
        for (auto &beta: pressureBoundary_.bnd.beta(bndNo)) {
             const auto c = DXQY::c(beta);
             f(fieldNo, beta, nodeNo) += w_[beta]*rhoCorr;
        }
        for (auto &delta: pressureBoundary_.bnd.delta(bndNo)) {
            const auto deltaHat = DXQY::reverseDirection(delta);
             f(fieldNo, delta, nodeNo) += w_[delta]*rhoCorr;
             f(fieldNo, deltaHat, nodeNo) += w_[delta]*rhoCorr;
        } 
    }
}

//                               WetNodeBoundary
//----------------------------------------------------------------------------------- readScalarValues
template<typename DXQY>
template<typename T>
std::vector<T> WetNodeBoundary<DXQY>::readScalarValues(const std::string attributeName, LBvtk<DXQY> & vtklb, const Nodes<DXQY> &nodes, const Grid<DXQY> & grid)
//-----------------------------------------------------------------------------------
{
    vtklb.toAttribute(attributeName);
    std::vector<T> ret(grid.size());
    for (int nodeNo = vtklb.beginNodeNo(); nodeNo < vtklb.endNodeNo(); nodeNo++) 
    {
        ret[nodeNo] = vtklb.template getScalarAttribute<T>();
    }

    return ret;
}

//                               WetNodeBoundary
//----------------------------------------------------------------------------------- readVectorValues
template<typename DXQY>
template<typename T>
std::vector<std::valarray<T>> WetNodeBoundary<DXQY>::readVectorValues(const std::string attributeName, LBvtk<DXQY> & vtklb, const Nodes<DXQY> &nodes, const Grid<DXQY> & grid)
//-----------------------------------------------------------------------------------
{
    const std::array<const std::string, 3> index{"x", "y", "z"};
    std::vector<std::valarray<T>> ret(grid.size(), std::valarray<T>(DXQY::nD));

    for (int d = 0; d < DXQY::nD; ++d) {
        vtklb.toAttribute(attributeName + "_" + index[d]);
        for (int nodeNo = vtklb.beginNodeNo(); nodeNo < vtklb.endNodeNo(); nodeNo++) 
        {
            ret[nodeNo][d] = vtklb.template getScalarAttribute<T>();
        }
    }
    return ret;
}

//                               WetNodeBoundary
//----------------------------------------------------------------------------------- addMassToWallBoundary
template<typename DXQY>
void WetNodeBoundary<DXQY>::addMassToWallBoundary(const int fieldNo, LbField<DXQY> &f, const VectorField<DXQY> & force, const lbBase_t mass)
{
    const Boundary<DXQY> bnd = wallBoundary_.bnd;
/*    const lbBase_t deltaRho = mass/bnd.size();    
    for (int bndNo=0; bndNo<bnd.size(); ++bndNo) {
        const int nodeNo = bnd.nodeNo(bndNo);
        const std::valarray<lbBase_t> fNode = f(fieldNo, nodeNo);
        const auto rhoNode = DXQY::qSum(fNode);
        const std::valarray<lbBase_t>  u = (DXQY::qSumC(fNode) + 0.5*force(fieldNo, nodeNo))/rhoNode;
        const auto cu = DXQY::cDotAll(u);
        for (int q=0; q<DXQY::nQ; ++q)
            f(0, q, nodeNo) += DXQY::w[q]*deltaRho*(1.0 + DXQY::c2Inv*cu[q]);
    }    */
    lbBase_t weight = 0.0;
    for (int bndNo=0; bndNo<bnd.size(); ++bndNo) {
        const int nodeNo = bnd.nodeNo(bndNo);
        const std::valarray<lbBase_t> fNode = f(fieldNo, nodeNo);
        const auto rhoNode = DXQY::qSum(fNode);
        const std::valarray<lbBase_t>  u = (DXQY::qSumC(fNode) + 0.5*force(fieldNo, nodeNo))/rhoNode;
        const auto cu = DXQY::cDotAll(u);
        for (const auto & beta: bnd.beta(bndNo)) {
            weight +=  DXQY::w[beta]*(1 + DXQY::c2Inv*cu[beta]);
        }
        for (const auto & delta: bnd.delta(bndNo)) {
            weight +=  DXQY::w[delta]*(1 + DXQY::c2Inv*cu[delta]);
            const auto deltaHat = bnd.dirRev(delta);
            weight +=  DXQY::w[deltaHat]*(1 + DXQY::c2Inv*cu[deltaHat]);
        }
    }    
    
    const lbBase_t deltaRho = mass/weight;
    for (int bndNo=0; bndNo<bnd.size(); ++bndNo) {
        const int nodeNo = bnd.nodeNo(bndNo);
        const std::valarray<lbBase_t> fNode = f(fieldNo, nodeNo);
        const auto rhoNode = DXQY::qSum(fNode);
        const std::valarray<lbBase_t>  u = (DXQY::qSumC(fNode) + 0.5*force(fieldNo, nodeNo))/rhoNode;
        const auto cu = DXQY::cDotAll(u);
        for (const auto & beta: bnd.beta(bndNo)) {
            f(0, beta, nodeNo) +=  DXQY::w[beta]*deltaRho*(1 + DXQY::c2Inv*cu[beta]);
        }
        for (const auto & delta: bnd.delta(bndNo)) {
            f(0, delta, nodeNo) +=  DXQY::w[delta]*deltaRho*(1 + DXQY::c2Inv*cu[delta]);
            const auto deltaHat = bnd.dirRev(delta);
            f(0, deltaHat, nodeNo) +=  DXQY::w[deltaHat]*deltaRho*(1 + DXQY::c2Inv*cu[deltaHat]);
        }
    }    
    
/*    const lbBase_t deltaRho = mass/outfluxWallTotal_;
    for (int bndNo=0; bndNo<bnd.size(); ++bndNo) {
        const int nodeNo = bnd.nodeNo(bndNo);
        for (const auto & beta: bnd.beta(bndNo)) {
            f(0, beta, nodeNo) += DXQY::w[beta]*deltaRho;
        }
        for (const auto & delta: bnd.delta(bndNo)) {
            f(0, delta, nodeNo) += DXQY::w[delta]*deltaRho;
            const auto deltaHat = bnd.dirRev(delta);
            f(0, deltaHat, nodeNo) += DXQY::w[delta]*deltaRho;
        }
    } */    

}

#endif