#ifndef LBFREEFLOWCARTESIAN_H
#define LBFREEFLOWCARTESIAN_H

#include "LBglobal.h"
#include "LBboundary.h"
#include "LBgrid.h"
#include "LBfield.h"


/*********************************************************
 * class FREEFLOWCARTESIAN: class that performes a
 * boundary condition for free flow out in on of the
 * Cartesian direction.
 * 
 * NB this is a prototype function so that only outflow in the
 * negative z direction is considered.
 *
 * FreeFlowCartesian.apply(...) needs to be run straight
 * after propagation.
 *
 *********************************************************/

template <typename DXQY>
class FreeFlowCartesian : public Boundary<DXQY>
{
public:
    FreeFlowCartesian(const std::vector<int> bndNodes, const Nodes<DXQY> &nodes, const Grid<DXQY> &grid);
    void apply(const int fieldNo, LbField<DXQY> &f, const Grid<DXQY> &grid) const;

private:
    int q_normal;
};


template <typename DXQY>
FreeFlowCartesian<DXQY>::FreeFlowCartesian(const std::vector<int> bndNodes, const Nodes<DXQY> &nodes, const Grid<DXQY> &grid)
    : Boundary<DXQY>(bndNodes, nodes, grid)
{
    std::vector<int> n_normal {0, 1, 0};

    q_normal = DXQY::c2q(n_normal);
}


template <typename DXQY>
inline void FreeFlowCartesian<DXQY>::apply(const int fieldNo, LbField<DXQY> &f, const Grid<DXQY> &grid) const
/* apply : performs the free slip condition, at the bondary nodes.
    *
    * fieldNo :
    the lB-field number
    * f       :
    the field object
    * grid    :
    grid object
    *
    * Use 'this->' to access functions and variables in the parent class, Boundary<DXQY>.
    *
    */
{
    for (int n = 0; n < this->nBoundaryNodes_; ++n) {
        int node = this->nodeNo(n);
        int nodeNeig = grid.neighbor(q_normal, node);

        // Need to calculate the density and velocity at the neighbor node
        const std::valarray<lbBase_t> fNode = f(0, node);
        const std::valarray<lbBase_t> fNeig = f(0, nodeNeig);

        lbBase_t rhoNeig = DXQY::qSum(fNeig);
        std::valarray<lbBase_t> velNeig = DXQY::qSumC(fNeig) / rhoNeig;

        lbBase_t C0 = 0;
        std::valarray<lbBase_t> CV(0.0, DXQY::nD);
	//std::valarray<lbBase_t> CV(DXQY::nD, 0.0);

        for (auto gamma: this->gamma(n)) {
            C0 += fNode[gamma];
            std::valarray<int> c_va(DXQY::c(gamma).data(), DXQY::nD);
	    for(int d=0; d<DXQY::nD; d++){
	      CV[d] += fNode[gamma]*c_va[d];
	    }
	    
	    //CV += fNode[gamma]*c_va;
	    
            int gamma_rev = this->dirRev(gamma);
            C0 += fNode[gamma_rev];
            std::valarray<int> c_va_rev(DXQY::c(gamma_rev).data(), DXQY::nD);
	    for(int d=0; d<DXQY::nD; d++){
	      CV[d] += fNode[gamma_rev]*c_va_rev[d];
	    }
        }

        for (auto beta: this->beta(n)) {
            int beta_rev = this->dirRev(beta);
            C0 += fNode[beta_rev];
            std::valarray<int> c_va(DXQY::c(beta_rev).data(), DXQY::nD);
	    for(int d=0; d<DXQY::nD; d++){
	      CV[d] += fNode[beta_rev]*c_va[d];
	    }
        }

        std::vector<lbBase_t> fNeq(DXQY::nQ);
        lbBase_t uu = DXQY::dot(velNeig, velNeig);
        lbBase_t vel_tmp[] = {velNeig[0], velNeig[1], velNeig[2]};
        for (auto beta: this->beta(n)) {
            lbBase_t cu = DXQY::cDot(beta, vel_tmp);
            fNeq[beta] = fNeig[beta] - rhoNeig * DXQY::w[beta]*(1.0 + DXQY::c2Inv*cu + DXQY::c4Inv0_5*(cu*cu - DXQY::c2*uu) );
            C0 += fNeq[beta];
            std::valarray<int> c_va(DXQY::c(beta).data(), DXQY::nD);
	    for(int d=0; d<DXQY::nD; d++){
	      CV[d] += c_va[d]*fNeq[beta];
	    }
        }
        
        lbBase_t velNode[DXQY::nD];
	/*
        velNode[0] = (6.0/5.0)*CV[0];
        velNode[1] = (6.0/5.0)*CV[1];
        velNode[2] = (1.0/22.0)*(6*C0 + 33*CV[2]);
	*/
	velNode[0] = (6.0/5.0)*CV[0];
        velNode[1] = (1.0/22.0)*(6*C0 + 33*CV[1]);
	velNode[2] = (6.0/5.0)*CV[2];
	
        lbBase_t rhoNode = (6.0/5.0)*C0 + (3.0/5.0)*velNode[1];
        for (int d=0; d < DXQY::nD; ++d) {
            velNode[d] /= rhoNode;
        }
	std::cout<<"nodeNo = "<< node <<", rho = "<<rhoNode<<std::endl;
	
        for (auto beta: this->beta(n)) {
            lbBase_t cu = DXQY::cDot(beta, velNode);
            f(0, node, beta) = DXQY::w[beta]*rhoNode*(1.0 + DXQY::c2Inv*cu + DXQY::c4Inv0_5*(cu*cu - DXQY::c2*uu) ) + fNeq[beta];            
        }

    }
}

#endif // LBFREESLIPCARTESIAN_H
