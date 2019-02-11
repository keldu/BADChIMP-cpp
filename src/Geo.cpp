/*
 * Geometry.cpp
 *
 *  Created on: Feb 8, 2019
 *      Author: janlv
 */

#include "Geo.h"

//------------------------------------
//
//------------------------------------
void Geo::set_nodes(const std::vector<char>& geo) {
  std::vector<int> n = get_local_size();
  std::vector<int> i(n.size());
  i[2] = n.size() - 2; // 2D:z=0, 3D:z=1 to skip periodic/ghost rim
  do {
    for(i[1]=1; i[1]<n[1]-1; ++i[1]) {
      for(i[0]=1; i[0]<n[0]-1; ++i[0]) {
        //int a = mpi.get_pos(i, n_);
        //int b = mpi.get_pos(i+lb_-2, geo_n);
        int a = get_local_pos(i);
        int b = get_geofile_pos(i);
        //std::cout << a << "," << b << ":" << geo_in[b] << std::endl;
        nodes[a] = geo[b];
      }
    }
    ++i[2];
  } while (i[2]<n[2]-1);
}

//------------------------------------
// Load distribution
// Set N, n, lb, ub
//------------------------------------
void Geo::set_limits(MPI& mpi) {
  for (size_t i = 0; i < N_.size(); ++i) {
    //N_[i] = n_[i] + 2;
    //N_[i] = n_[i];
    int rank_ind = mpi.get_rank_ind(i);
    int nprocs = mpi.get_num_procs(i);
    int n_tmp = n_[i] / nprocs; //mpi.procs_[i];
    int n_rest = n_[i] % nprocs; // mpi.get_num_procs(i); //mpi.procs_[i];
    //lb_[i] = mpi.rank_ind_[i] * n_tmp;
    lb_[i] = rank_ind * n_tmp;
    lb_[i] += (rank_ind <= n_rest) ? rank_ind : n_rest;
    if (rank_ind + 1 <= n_rest)
      ++n_tmp;
    n_[i] = n_tmp;
    lb_[i] += 1;
    ub_[i] = lb_[i] + n_[i] - 1;
  }
}


//------------------------------------
//------------------------------------
void Geo::add_ghost_nodes() {
  for (size_t i = 0; i < N_.size(); ++i) {
    N_[i] += 2;
    n_[i] += 2;
  }
};
