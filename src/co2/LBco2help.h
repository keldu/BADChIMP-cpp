#ifndef LBCO2HELP_H
#define LBCO2HELP_H

#include <string>
#include "../lbsolver/LBglobal.h"
#include "../lbsolver/LBlatticetypes.h"
#include "../lbsolver/LBfield.h"
#include "../lbsolver/LBvtk.h"
#include "../lbsolver/LBnodes.h"

template <typename DXQY>
void setScalarAttribute(ScalarField &field, const std::string &attributeName, LBvtk<DXQY> &vtklb) {
    for (int fieldNo=0; fieldNo < field.num_fields(); fieldNo++) {
        vtklb.toAttribute(attributeName + std::to_string(fieldNo));
        for (int n=vtklb.beginNodeNo(); n < vtklb.endNodeNo(); ++n) {
            field(fieldNo, n) = vtklb.template getScalarAttribute<lbBase_t>(); //vtklb.getScalarAttribute<lbBase_t>();
        }
    }
}

template <typename DXQY>
void setScalarAttributeWall(ScalarField &field, const std::string &attributeName, LBvtk<DXQY> &vtklb, const Nodes<DXQY> &nodes) {
    for (int fieldNo=0; fieldNo < field.num_fields(); fieldNo++) {
        vtklb.toAttribute(attributeName + std::to_string(fieldNo));
        for (int n=vtklb.beginNodeNo(); n < vtklb.endNodeNo(); ++n) {
            const auto val = vtklb.template getScalarAttribute<lbBase_t>();
            if (nodes.isSolidBoundary(n)) {
                field(fieldNo, n) = val;
            }
        }
    }
}

template<typename T>
void normelizeScalarField(ScalarField &field, T nodeList)
{
    const int nFields = field.num_fields();
    for (auto nodeNo: nodeList) {
        lbBase_t tmp = 0;
        for (int fieldNo=0; fieldNo < nFields; ++fieldNo) {
            tmp += field(fieldNo, nodeNo);
        }
        for (int fieldNo=0; fieldNo < nFields; ++fieldNo) {
            field(fieldNo, nodeNo) /= tmp + lbBaseEps;
        }
    }
}

#endif