#ifndef LBNODES_H
#define LBNODES_H

#include <vector>
#include <numeric>
#include "LBglobal.h"
#include "LBlatticetypes.h"
#include "Input.h"
#include "LBgrid.h"


/*
 * Nodes: -contains additional informatino about a node, eg. rank, node type etc.
 *        -class Grid will still contain the cartesion indices of the nodes and
 *         the list of node-numbers of the nodes in a node's neighborhood.
 *
 * Work in progress: moving node-information from class Grid to class Nodes
*/

template<typename DXQY>
class Nodes
{
public:
    Nodes(const int nNodes, const int myRank):
        nNodes_(nNodes),
        myRank_(myRank),
        nodeType_(static_cast<std::size_t>(nNodes)),
        nodeRank_(static_cast<std::size_t>(nNodes), -1),
        nodeT_(static_cast<std::size_t>(nNodes), -1)
    {}
    inline int size() const {return nNodes_;}
    void setup(MpiFile<DXQY> &mfs, MpiFile<DXQY> &rfs, const Grid<DXQY> &grid);
    inline int getType(const int nodeNo) const {return nodeT_[nodeNo];}
    inline int getRank(const int nodeNo) const {return nodeRank_[nodeNo];}
    inline bool isMyRank(const int nodeNo) const {return nodeRank_[nodeNo] == myRank_;}
    inline bool isSolid(const int nodeNo) const {return (nodeT_[nodeNo] < 2);} /* Solid: -1, 0 or 1*/
    inline bool isBulkSolid(const int nodeNo) const {return (nodeT_[nodeNo] == 0);}
    inline bool isSolidBoundary(const int nodeNo) const {return (nodeT_[nodeNo] == 1);}
    inline bool isFluid(const int nodeNo) const {return (nodeT_[nodeNo] > 1);} /* Fluid: 2, 3 or 4 */
    inline bool isBulkFluid(const int nodeNo) const {return (nodeT_[nodeNo] == 3);}
    inline bool isFluidBoundary(const int nodeNo) const {return (nodeT_[nodeNo] == 2);}
    inline bool isMpiBoundary(const int nodeNo) const {return (nodeT_[nodeNo] == 4);}


    //inline int getRank(const int nodeNo) const {return nodeRank_[nodeNo];}
    static Nodes<DXQY> makeObject(MpiFile<DXQY> &mfs, MpiFile<DXQY> &rfs, const int myRank, const Grid<DXQY> &grid);

private:
    int nNodes_;
    int myRank_;
    std::vector<int> nodeType_; // Zero for solid and rank = value-1
    std::vector<int> nodeRank_; // The actual node type
    std::vector<short int> nodeT_; // The actual node type

};


template<typename DXQY>
void Nodes<DXQY>::setup(MpiFile<DXQY> &mfs, MpiFile<DXQY> &rfs, const Grid<DXQY> &grid)
/*
 * mfs : local node number file
 * rfs : rank number file
 *
 * Node type
 * -1 : default
 *  0 : solid (bulk solid)
 *  1 : solid boundary
 *  2 : fluid boundary
 *  3 : fluid (bulk fluid)
 *  4 : fluid on another process
 */
{
    mfs.reset();
    rfs.reset();

    for (int pos=0; pos < static_cast<int>(mfs.size()); ++pos)
    {
        auto nodeNo = mfs.template getVal<int>(); // Gets node number. The template name is needed
        auto nodeType =  rfs.template getVal<int>(); // get the node type

        // If rank is already set to myRank do not change it
        if (nodeNo > 0) {
            // Set the rank of the node (-1 if no rank is set, eg. for the default node)
            if (nodeRank_[nodeNo] != myRank_)
                nodeRank_[nodeNo] = nodeType - 1;
            // Set node to solid (0) or fluid (3)
            if (nodeType == 0) nodeT_[nodeNo] = 0;
            else nodeT_[nodeNo] = 3;
        }


        if ( mfs.insideDomain(pos) ) { // Update the grid object
            if (nodeNo > 0) { // Only do changes if it is a non-default node
                // Add node type
                nodeType_[nodeNo] = nodeType;
            }
        }
    }

    mfs.reset();
    rfs.reset();

    for (int nodeNo = 1; nodeNo < grid.size(); ++nodeNo) {
        if (isSolid(nodeNo)) {
            bool hasFluidNeig = false;
            for (auto neigNode: grid.neighbor(nodeNo)) {
                if (isFluid(neigNode))  hasFluidNeig = true;
            }
            if (hasFluidNeig) nodeT_[nodeNo] = 1;
            else nodeT_[nodeNo] = 0;
        }
        else if (isFluid(nodeNo)) {
            if (getRank(nodeNo) != myRank_) {
                nodeT_[nodeNo] = 4;
            } else {
                bool hasSolidNeig = false;
                for (auto neigNode: grid.neighbor(nodeNo)) {
                    if (isSolid(neigNode))  hasSolidNeig = true;
                }
                if (hasSolidNeig)  nodeT_[nodeNo] = 2;
                else nodeT_[nodeNo] = 3;
            }
        } else {
            std::cout << "WARNING: NodeNo = " << nodeNo << " is neither fluid or solid" << std::endl;
        }
    }
}

template<typename DXQY>
Nodes<DXQY> Nodes<DXQY>::makeObject(MpiFile<DXQY> &mfs, MpiFile<DXQY> &rfs, const int myRank, const Grid<DXQY> &grid)
/* Makes a grid object using the information in the file created by our
 * python program, for each mpi-processor.
 *
 * mfs : local node number file
 * rfs : rank number file
 *
 * We assume that the file contains:
 *  1) Dimesions of the system (including the rim)
 *  2) The global Cartesian coordiantes of the local origo (first node
 *       in the list of nodes)
 *  3) Rim-width/thickness in number of nodes.
 *  4) List of all nodes including rim-nodes for this processor.
 */
{
    Nodes<DXQY> newNodes(grid.size(), myRank);

    mfs.reset();
    rfs.reset();

    newNodes.setup(mfs, rfs, grid);

    mfs.reset();
    rfs.reset();

    return newNodes;
}

#endif // LBNODES_H
